#include "bme280.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BME280_I2C_SPEED_HZ   100000
#define BME280_I2C_TIMEOUT_MS 100

#define BME280_CHIP_ID        0x60
#define BME280_SOFT_RESET_CMD 0xB6

/* Registers */
#define REG_CALIB1    0x88 /* 0x88..0xA1 */
#define REG_ID        0xD0
#define REG_RESET     0xE0
#define REG_CALIB2    0xE1 /* 0xE1..0xE7 */
#define REG_CTRL_HUM  0xF2
#define REG_STATUS    0xF3
#define REG_CTRL_MEAS 0xF4
#define REG_CONFIG    0xF5
#define REG_DATA      0xF7 /* press[3] temp[3] hum[2] */

#define STATUS_MEASURING (1u << 3)
#define STATUS_IM_UPDATE (1u << 0)

/* 1x oversampling on all three channels, forced mode (0b01) */
#define CTRL_HUM_OSRS_H_X1  0x01
#define CTRL_MEAS_X1_FORCED ((1u << 5) | (1u << 2) | 0x01u)

#define BME280_DATA_LEN 8
/* Datasheet: maximum measurement time at 1x/1x/1x is 9.3 ms. */
#define MEASURE_POLL_MS   2
#define MEASURE_MAX_POLLS 20

/* Values the sensor reports for a channel whose oversampling is "skipped". */
#define RAW_SKIPPED_20BIT 0x80000
#define RAW_SKIPPED_16BIT 0x8000

static const char *TAG = "bme280";

static esp_err_t read_regs(bme280_t *s, uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s->dev, &reg, 1, buf, len, BME280_I2C_TIMEOUT_MS);
}

static esp_err_t write_reg(bme280_t *s, uint8_t reg, uint8_t value)
{
    const uint8_t frame[2] = {reg, value};
    return i2c_master_transmit(s->dev, frame, sizeof(frame), BME280_I2C_TIMEOUT_MS);
}

static esp_err_t wait_ready(bme280_t *s, uint8_t busy_mask)
{
    for (int i = 0; i < MEASURE_MAX_POLLS; i++) {
        uint8_t status;
        esp_err_t err = read_regs(s, REG_STATUS, &status, 1);
        if (err != ESP_OK) {
            return err;
        }
        if ((status & busy_mask) == 0) {
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(MEASURE_POLL_MS));
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t bme280_init(bme280_t *sensor, i2c_master_bus_handle_t bus, uint8_t addr)
{
    if (sensor == NULL || bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = BME280_I2C_SPEED_HZ,
    };
    sensor->dev = NULL;
    esp_err_t err = i2c_master_bus_add_device(bus, &cfg, &sensor->dev);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t id = 0;
    err = read_regs(sensor, REG_ID, &id, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "no response at 0x%02X: %s", addr, esp_err_to_name(err));
        err = ESP_ERR_NOT_FOUND;
        goto fail;
    }
    if (id != BME280_CHIP_ID) {
        ESP_LOGE(TAG, "unexpected chip ID 0x%02X at 0x%02X (BME280 is 0x%02X; 0x58 is a BMP280)",
                 id, addr, BME280_CHIP_ID);
        err = ESP_ERR_NOT_FOUND;
        goto fail;
    }

    /* Reset to a known state and wait until calibration has been copied from NVM. */
    err = write_reg(sensor, REG_RESET, BME280_SOFT_RESET_CMD);
    if (err != ESP_OK) {
        goto fail;
    }
    vTaskDelay(pdMS_TO_TICKS(5)); /* datasheet: 2 ms start-up time */
    err = wait_ready(sensor, STATUS_IM_UPDATE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "sensor did not finish loading calibration");
        goto fail;
    }

    uint8_t block1[BME280_CALIB1_LEN];
    uint8_t block2[BME280_CALIB2_LEN];
    err = read_regs(sensor, REG_CALIB1, block1, sizeof(block1));
    if (err == ESP_OK) {
        err = read_regs(sensor, REG_CALIB2, block2, sizeof(block2));
    }
    if (err != ESP_OK) {
        goto fail;
    }
    bme280_parse_calib(block1, block2, &sensor->calib);

    /* IIR filter off, no standby (forced mode). */
    err = write_reg(sensor, REG_CONFIG, 0x00);
    if (err != ESP_OK) {
        goto fail;
    }

    ESP_LOGI(TAG, "ready at 0x%02X", addr);
    return ESP_OK;

fail:
    i2c_master_bus_rm_device(sensor->dev);
    sensor->dev = NULL;
    return err;
}

esp_err_t bme280_read(bme280_t *sensor, bme280_reading_t *out)
{
    if (sensor == NULL || sensor->dev == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* ctrl_hum only takes effect after a write to ctrl_meas. */
    esp_err_t err = write_reg(sensor, REG_CTRL_HUM, CTRL_HUM_OSRS_H_X1);
    if (err != ESP_OK) {
        return err;
    }
    err = write_reg(sensor, REG_CTRL_MEAS, CTRL_MEAS_X1_FORCED);
    if (err != ESP_OK) {
        return err;
    }
    err = wait_ready(sensor, STATUS_MEASURING);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t raw[BME280_DATA_LEN];
    err = read_regs(sensor, REG_DATA, raw, sizeof(raw));
    if (err != ESP_OK) {
        return err;
    }

    const int32_t adc_p = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    const int32_t adc_t = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    const int32_t adc_h = ((int32_t)raw[6] << 8) | raw[7];

    if (adc_t == RAW_SKIPPED_20BIT || adc_p == RAW_SKIPPED_20BIT || adc_h == RAW_SKIPPED_16BIT) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    int32_t t_fine;
    const int32_t temp = bme280_compensate_temperature(&sensor->calib, adc_t, &t_fine);
    const uint32_t press = bme280_compensate_pressure(&sensor->calib, adc_p, t_fine);
    const uint32_t hum = bme280_compensate_humidity(&sensor->calib, adc_h, t_fine);

    out->temperature_c = temp / 100.0f;
    out->pressure_hpa = press / 256.0f / 100.0f;
    out->humidity_pct = hum / 1024.0f;
    return ESP_OK;
}

void bme280_deinit(bme280_t *sensor)
{
    if (sensor != NULL && sensor->dev != NULL) {
        i2c_master_bus_rm_device(sensor->dev);
        sensor->dev = NULL;
    }
}
