/**
 * @file sgp40.c
 * @brief SGP40 raw VOC signal driver — ESP-IDF v5 I2C master API.
 *
 * SGP40 uses a word-based protocol: every command is a 2-byte command code
 * followed by optional data words, each word followed by a CRC-8 byte.
 * CRC polynomial: 0x31, init: 0xFF, no final XOR.
 */

#include "sgp40.h"

#include "board_pins.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

static const char *TAG = "sgp40";

/* Commands (big-endian 16-bit) */
#define CMD_MEASURE_RAW   0x2600  /* measure_raw_signal                    */
#define CMD_SELF_TEST     0x280E  /* execute_self_test (~320 ms)            */
#define CMD_HEATER_OFF    0x3615  /* turn_heater_off (saves power)          */

#define SELF_TEST_OK      0xD400  /* expected result word if test passes    */
#define I2C_TIMEOUT_MS    50

static i2c_master_dev_handle_t s_dev = NULL;

/* ── CRC-8 (polynomial 0x31, init 0xFF) ─────────────────────────────────── */
static uint8_t crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : (crc << 1);
        }
    }
    return crc;
}

/* ── Send a 2-byte command ───────────────────────────────────────────────── */
static esp_err_t send_cmd(uint16_t cmd)
{
    uint8_t buf[2] = {cmd >> 8, cmd & 0xFF};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

/* ── Send a 2-byte command + one data word with CRC ─────────────────────── */
static esp_err_t send_cmd_word(uint16_t cmd, uint16_t word)
{
    uint8_t w[2] = {word >> 8, word & 0xFF};
    uint8_t buf[5] = {cmd >> 8, cmd & 0xFF, w[0], w[1], crc8(w, 2)};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

/* ── Read n response words (each word + CRC) ────────────────────────────── */
static esp_err_t read_words(uint16_t *words, size_t n)
{
    size_t  bytes = n * 3;
    uint8_t buf[bytes];
    esp_err_t err = i2c_master_receive(s_dev, buf, bytes, I2C_TIMEOUT_MS);
    if (err != ESP_OK) return err;

    for (size_t i = 0; i < n; i++) {
        uint8_t *w = &buf[i * 3];
        uint8_t expected = crc8(w, 2);
        if (w[2] != expected) {
            ESP_LOGE(TAG, "CRC error on word %u (got 0x%02X, expected 0x%02X)",
                     (unsigned)i, w[2], expected);
            return ESP_ERR_INVALID_CRC;
        }
        words[i] = (uint16_t)(w[0] << 8 | w[1]);
    }
    return ESP_OK;
}

/* ── Humidity-compensation encoding ─────────────────────────────────────── */
/* SGP40 expects RH and T as ticks: RH_ticks = RH% / 100 * 65535          */
/*                                  T_ticks  = (T_C + 45) / 175 * 65535   */
static uint16_t encode_rh(float rh_pct)
{
    if (rh_pct < 0.0f)   rh_pct = 0.0f;
    if (rh_pct > 100.0f) rh_pct = 100.0f;
    return (uint16_t)(rh_pct / 100.0f * 65535.0f + 0.5f);
}

static uint16_t encode_temp(float temp_c)
{
    return (uint16_t)((temp_c + 45.0f) / 175.0f * 65535.0f + 0.5f);
}

/* ── Public API ──────────────────────────────────────────────────────────── */

esp_err_t sgp40_init(void)
{
    if (s_dev != NULL) return ESP_ERR_INVALID_STATE;

    const i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BOARD_I2C_ADDR_SGP40,
        .scl_speed_hz    = 400000,
    };
    esp_err_t err = i2c_master_bus_add_device(i2c_bus_get(), &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Run self-test (~320 ms) */
    err = send_cmd(CMD_SELF_TEST);
    if (err != ESP_OK) goto fail;
    vTaskDelay(pdMS_TO_TICKS(320));

    uint16_t result;
    err = read_words(&result, 1);
    if (err != ESP_OK) goto fail;

    if (result != SELF_TEST_OK) {
        ESP_LOGE(TAG, "self-test failed (result 0x%04X)", result);
        err = ESP_ERR_INVALID_RESPONSE;
        goto fail;
    }

    ESP_LOGI(TAG, "init OK (self-test passed)");
    return ESP_OK;

fail:
    i2c_master_bus_rm_device(s_dev);
    s_dev = NULL;
    return err;
}

esp_err_t sgp40_measure_raw(float temperature_c, float humidity_pct, uint16_t *raw_out)
{
    if (s_dev == NULL) return ESP_ERR_INVALID_STATE;
    if (raw_out == NULL) return ESP_ERR_INVALID_ARG;

    /* measure_raw_signal: CMD (2) + RH_word+CRC (3) + T_word+CRC (3) = 8 bytes */
    uint16_t rh   = encode_rh(humidity_pct);
    uint16_t temp = encode_temp(temperature_c);
    uint8_t  rh_b[2]   = {rh >> 8,   rh & 0xFF};
    uint8_t  temp_b[2] = {temp >> 8, temp & 0xFF};

    uint8_t buf[8] = {
        CMD_MEASURE_RAW >> 8, CMD_MEASURE_RAW & 0xFF,
        rh_b[0], rh_b[1], crc8(rh_b, 2),
        temp_b[0], temp_b[1], crc8(temp_b, 2),
    };

    esp_err_t err = i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
    if (err != ESP_OK) return err;

    vTaskDelay(pdMS_TO_TICKS(30));   /* max measurement time per datasheet */

    uint16_t raw;
    err = read_words(&raw, 1);
    if (err != ESP_OK) return err;

    *raw_out = raw;
    return ESP_OK;
}
