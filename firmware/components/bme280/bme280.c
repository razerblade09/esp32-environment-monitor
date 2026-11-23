/**
 * @file bme280.c
 * @brief BME280 driver implementation — forced mode, I2C, ESP-IDF v5 master API.
 *
 * Compensation formulas are taken verbatim from the BME280 datasheet
 * (section 4.2.3) so they can be cross-checked against it directly.
 */

#include "bme280.h"

#include <string.h>

#include "board_pins.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

static const char *TAG = "bme280";

/* ── Register map ───────────────────────────────────────────────────────── */
#define REG_CALIB_00   0x88   /* T1-P9 calibration block start             */
#define REG_CHIP_ID    0xD0   /* Should read 0x60                          */
#define REG_RESET      0xE0   /* Write 0xB6 to soft-reset                  */
#define REG_CALIB_H1   0xA1   /* dig_H1                                    */
#define REG_CALIB_H2   0xE1   /* dig_H2-H6 block start                     */
#define REG_CTRL_HUM   0xF2   /* Humidity oversampling; write before F4    */
#define REG_STATUS     0xF3
#define REG_CTRL_MEAS  0xF4   /* Temp/pressure oversampling + mode         */
#define REG_CONFIG     0xF5
#define REG_PRESS_MSB  0xF7   /* Raw data block start (6 bytes T+P, 2 H)   */

#define CHIP_ID_BME280 0x60

/* Oversampling ×1 for all three quantities, forced mode (bits [1:0] = 01) */
#define CTRL_HUM_OSRS_H1   0x01   /* osrs_h = 001 */
#define CTRL_MEAS_FORCED   0x27   /* osrs_t=001, osrs_p=001, mode=01       */

#define I2C_TIMEOUT_MS  50

/* ── Calibration data ───────────────────────────────────────────────────── */
typedef struct {
    /* Temperature */
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;
    /* Pressure */
    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;
    /* Humidity */
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4;
    int16_t  dig_H5;
    int8_t   dig_H6;
} bme280_calib_t;

static i2c_master_dev_handle_t s_dev  = NULL;
static bme280_calib_t          s_calib;

/* ── Low-level I2C helpers ──────────────────────────────────────────────── */

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *out, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, out, len, I2C_TIMEOUT_MS);
}

/* ── Calibration parsing ────────────────────────────────────────────────── */

static esp_err_t read_calibration(void)
{
    uint8_t buf[26];

    /* Block 1: 0x88-0xA1  (24 bytes T+P, then 1 byte H1) */
    esp_err_t err = reg_read(REG_CALIB_00, buf, 24);
    if (err != ESP_OK) return err;

    s_calib.dig_T1 = (uint16_t)(buf[1]  << 8 | buf[0]);
    s_calib.dig_T2 = (int16_t) (buf[3]  << 8 | buf[2]);
    s_calib.dig_T3 = (int16_t) (buf[5]  << 8 | buf[4]);
    s_calib.dig_P1 = (uint16_t)(buf[7]  << 8 | buf[6]);
    s_calib.dig_P2 = (int16_t) (buf[9]  << 8 | buf[8]);
    s_calib.dig_P3 = (int16_t) (buf[11] << 8 | buf[10]);
    s_calib.dig_P4 = (int16_t) (buf[13] << 8 | buf[12]);
    s_calib.dig_P5 = (int16_t) (buf[15] << 8 | buf[14]);
    s_calib.dig_P6 = (int16_t) (buf[17] << 8 | buf[16]);
    s_calib.dig_P7 = (int16_t) (buf[19] << 8 | buf[18]);
    s_calib.dig_P8 = (int16_t) (buf[21] << 8 | buf[20]);
    s_calib.dig_P9 = (int16_t) (buf[23] << 8 | buf[22]);

    /* dig_H1 sits alone at 0xA1 */
    err = reg_read(REG_CALIB_H1, buf, 1);
    if (err != ESP_OK) return err;
    s_calib.dig_H1 = buf[0];

    /* Block 2: 0xE1-0xE7  (7 bytes H2-H6) */
    err = reg_read(REG_CALIB_H2, buf, 7);
    if (err != ESP_OK) return err;

    s_calib.dig_H2 = (int16_t)(buf[1] << 8 | buf[0]);
    s_calib.dig_H3 = buf[2];
    /* dig_H4 / H5 share a byte (0xE5) — datasheet section 4.2.2 */
    s_calib.dig_H4 = (int16_t)((int8_t)buf[3] << 4 | (buf[4] & 0x0F));
    s_calib.dig_H5 = (int16_t)((int8_t)buf[5] << 4 | (buf[4] >> 4));
    s_calib.dig_H6 = (int8_t)buf[6];

    return ESP_OK;
}

/* ── Compensation formulas (BME280 datasheet §4.2.3, integer path) ──────── */

static int32_t compensate_temperature(int32_t adc_T, int32_t *t_fine)
{
    const bme280_calib_t *c = &s_calib;
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)c->dig_T1 << 1))) * c->dig_T2) >> 11;
    int32_t var2 = (((((adc_T >> 4) - (int32_t)c->dig_T1) *
                       ((adc_T >> 4) - (int32_t)c->dig_T1)) >> 12) *
                    c->dig_T3) >> 14;
    *t_fine = var1 + var2;
    return (*t_fine * 5 + 128) >> 8;   /* units: 0.01 °C */
}

static uint32_t compensate_pressure(int32_t adc_P, int32_t t_fine)
{
    const bme280_calib_t *c = &s_calib;
    int64_t var1 = (int64_t)t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)c->dig_P6;
    var2 += (var1 * (int64_t)c->dig_P5) << 17;
    var2 += (int64_t)c->dig_P4 << 35;
    var1  = ((var1 * var1 * (int64_t)c->dig_P3) >> 8) + ((var1 * (int64_t)c->dig_P2) << 12);
    var1  = (((int64_t)1 << 47) + var1) * (int64_t)c->dig_P1 >> 33;
    if (var1 == 0) return 0;
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = ((int64_t)c->dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = ((int64_t)c->dig_P8 * p) >> 19;
    p = ((p + var1 + var2) >> 8) + ((int64_t)c->dig_P7 << 4);
    return (uint32_t)p;    /* units: Q24.8 Pa, divide by 256 for Pa */
}

static uint32_t compensate_humidity(int32_t adc_H, int32_t t_fine)
{
    const bme280_calib_t *c = &s_calib;
    int32_t v = t_fine - 76800;
    v = (((((adc_H << 14) - ((int32_t)c->dig_H4 << 20) - ((int32_t)c->dig_H5 * v)) + 16384) >> 15) *
         (((((((v * (int32_t)c->dig_H6) >> 10) *
              (((v * (int32_t)c->dig_H3) >> 11) + 32768)) >> 10) + 2097152) *
           (int32_t)c->dig_H2 + 8192) >> 14));
    v -= (((((v >> 15) * (v >> 15)) >> 7) * (int32_t)c->dig_H1) >> 4);
    if (v < 0)         v = 0;
    if (v > 419430400) v = 419430400;
    return (uint32_t)(v >> 12);   /* units: Q22.10, divide by 1024 for %RH */
}

/* ── Public API ─────────────────────────────────────────────────────────── */

esp_err_t bme280_init(void)
{
    if (s_dev != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    /* Register device on the shared bus */
    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BOARD_I2C_ADDR_BME280,
        .scl_speed_hz    = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_get(), &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to add device: %s", esp_err_to_name(err));
        return err;
    }

    /* Verify chip ID */
    uint8_t id = 0;
    err = reg_read(REG_CHIP_ID, &id, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "chip ID read failed: %s", esp_err_to_name(err));
        goto fail;
    }
    if (id != CHIP_ID_BME280) {
        ESP_LOGE(TAG, "unexpected chip ID 0x%02X (expected 0x%02X)", id, CHIP_ID_BME280);
        err = ESP_ERR_INVALID_RESPONSE;
        goto fail;
    }

    /* Read factory calibration coefficients */
    err = read_calibration();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "calibration read failed: %s", esp_err_to_name(err));
        goto fail;
    }

    ESP_LOGI(TAG, "init OK (chip ID 0x%02X)", id);
    return ESP_OK;

fail:
    i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
    return err;
}

esp_err_t bme280_read(bme280_data_t *out)
{
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;
    if (out == NULL)   return ESP_ERR_INVALID_ARG;

    /* Set humidity oversampling BEFORE ctrl_meas (datasheet requirement) */
    esp_err_t err = reg_write(REG_CTRL_HUM, CTRL_HUM_OSRS_H1);
    if (err != ESP_OK) return err;

    /* Trigger one forced measurement */
    err = reg_write(REG_CTRL_MEAS, CTRL_MEAS_FORCED);
    if (err != ESP_OK) return err;

    /* Wait for measurement to complete (~10 ms for ×1 oversampling) */
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Poll status register until measuring bit clears */
    for (int retries = 0; retries < 10; retries++) {
        uint8_t status = 0;
        err = reg_read(REG_STATUS, &status, 1);
        if (err != ESP_OK) return err;
        if ((status & 0x08) == 0) break;   /* bit 3 = measuring */
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    /* Burst-read 8 bytes: press[7:0] temp[7:0] hum[7:0] */
    uint8_t raw[8];
    err = reg_read(REG_PRESS_MSB, raw, sizeof(raw));
    if (err != ESP_OK) return err;

    /* Reconstruct 20-bit ADC values (data is MSB-first, bottom 4 bits unused) */
    int32_t adc_P = (int32_t)((uint32_t)raw[0] << 12 | (uint32_t)raw[1] << 4 | raw[2] >> 4);
    int32_t adc_T = (int32_t)((uint32_t)raw[3] << 12 | (uint32_t)raw[4] << 4 | raw[5] >> 4);
    int32_t adc_H = (int32_t)((uint32_t)raw[6] << 8  |  raw[7]);

    /* Apply Bosch compensation formulas */
    int32_t t_fine;
    int32_t  T = compensate_temperature(adc_T, &t_fine);
    uint32_t P = compensate_pressure(adc_P, t_fine);
    uint32_t H = compensate_humidity(adc_H, t_fine);

    out->temperature_c = (float)T  / 100.0f;
    out->pressure_hpa  = (float)P  / 256.0f / 100.0f;
    out->humidity_pct  = (float)H  / 1024.0f;

    return ESP_OK;
}
