/**
 * @file scd40.c
 * @brief SCD40 CO2 / temperature / humidity driver — ESP-IDF v5 I2C master API.
 *
 * SCD40 protocol: 2-byte command, optional data words each with CRC-8.
 * CRC polynomial: 0x31, init: 0xFF — same as SGP40 / most Sensirion sensors.
 *
 * The SCD40 only supports periodic measurement (one sample every ~5 s). The
 * single-shot commands (0x219D, 0x2196) exist only on the SCD41 and are NACKed
 * by the SCD40. scd40_init() starts periodic mode; scd40_measure() waits for
 * the next sample using get_data_ready_status, then reads it.
 */

#include "scd40.h"

#include "board_pins.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

static const char *TAG = "scd40";

/* Commands */
#define CMD_START_PERIODIC   0x21B1 /* start_periodic_measurement     */
#define CMD_STOP_PERIODIC    0x3F86 /* stop_periodic_measurement      */
#define CMD_READ_MEASUREMENT 0xEC05 /* read_measurement (3 words)     */
#define CMD_DATA_READY       0xE4B8 /* get_data_ready_status (1 word) */
#define CMD_GET_SERIAL       0x3682 /* get_serial_number (3 words)    */

#define DATA_READY_MASK    0x07FF /* least significant 11 bits == 0 -> not ready */
#define POLL_INTERVAL_MS   200
#define MEASURE_TIMEOUT_MS 7000 /* period is ~5 s; allow margin for first sample */
#define I2C_TIMEOUT_MS     50

/* Fixed buffer sized for the largest SCD40 response: 3 words × 3 bytes */
#define MAX_RESPONSE_WORDS 3
#define BYTES_PER_WORD     3 /* data[2] + crc[1] */

static i2c_master_dev_handle_t s_dev = NULL;

/* ── CRC-8 (same as SGP40) ───────────────────────────────────────────────── */
static uint8_t crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static esp_err_t send_cmd(uint16_t cmd)
{
    uint8_t buf[2] = {(uint8_t)(cmd >> 8), (uint8_t)(cmd & 0xFF)};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

/* Read n words (each word[2] + crc[1]) into caller-supplied array */
static esp_err_t read_words(uint16_t *words, size_t n)
{
    uint8_t buf[MAX_RESPONSE_WORDS * BYTES_PER_WORD];

    if (n > MAX_RESPONSE_WORDS) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = i2c_master_receive(s_dev, buf, n * BYTES_PER_WORD, I2C_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }

    for (size_t i = 0; i < n; i++) {
        uint8_t *w = &buf[i * BYTES_PER_WORD];
        if (crc8(w, 2) != w[2]) {
            ESP_LOGE(TAG, "CRC error on word %u", (unsigned)i);
            return ESP_ERR_INVALID_CRC;
        }
        words[i] = (uint16_t)(w[0] << 8 | w[1]);
    }
    return ESP_OK;
}

/* Send command, optional delay, then read n words */
static esp_err_t cmd_then_read(uint16_t cmd, uint16_t *words, size_t n, uint32_t wait_ms)
{
    esp_err_t err = send_cmd(cmd);
    if (err != ESP_OK) {
        return err;
    }
    if (wait_ms) {
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
    return read_words(words, n);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

esp_err_t scd40_init(void)
{
    if (s_dev != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BOARD_I2C_ADDR_SCD40,
        .scl_speed_hz = 100000, /* SCD40 max I2C speed is 100 kHz */
    };
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_get(), &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Stop any running measurement (the sensor ignores most commands while measuring) */
    err = send_cmd(CMD_STOP_PERIODIC);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "stop_periodic failed (may not have been running): %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(500)); /* datasheet: 500 ms after stop */

    /* Read serial number to confirm device is alive */
    uint16_t serial[3];
    err = cmd_then_read(CMD_GET_SERIAL, serial, 3, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "serial read failed: %s", esp_err_to_name(err));
        goto fail;
    }
    ESP_LOGI(TAG, "init OK (serial 0x%04X%04X%04X)", serial[0], serial[1], serial[2]);

    err = send_cmd(CMD_START_PERIODIC);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "start_periodic failed: %s", esp_err_to_name(err));
        goto fail;
    }
    return ESP_OK;

fail:
    i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
    return err;
}

esp_err_t scd40_measure(scd40_data_t *out)
{
    if (s_dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    /* Poll until a new sample is available (SCD40 produces one every ~5 s) */
    esp_err_t err = ESP_ERR_TIMEOUT;
    for (uint32_t waited = 0; waited < MEASURE_TIMEOUT_MS; waited += POLL_INTERVAL_MS) {
        uint16_t status = 0;
        err = cmd_then_read(CMD_DATA_READY, &status, 1, 1);
        if (err != ESP_OK) {
            return err;
        }
        if ((status & DATA_READY_MASK) != 0) {
            err = ESP_OK;
            break;
        }
        err = ESP_ERR_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
    if (err != ESP_OK) {
        return err;
    }

    /* Read 3 result words: CO2, raw_T, raw_RH. Reading clears the ready flag. */
    uint16_t words[3];
    err = cmd_then_read(CMD_READ_MEASUREMENT, words, 3, 1);
    if (err != ESP_OK) {
        return err;
    }

    /* Convert per SCD40 datasheet */
    out->co2_ppm = words[0];
    out->temperature_c = -45.0f + 175.0f * (float)words[1] / 65535.0f;
    out->humidity_pct = 100.0f * (float)words[2] / 65535.0f;

    return ESP_OK;
}
