/**
 * @file pms5003.c
 * @brief PMS5003 UART driver — ESP-IDF UART driver API.
 *
 * Frame format (32 bytes, big-endian):
 *   [0]  0x42  Start byte 1
 *   [1]  0x4D  Start byte 2
 *   [2-3]  Frame length (28)
 *   [4-5]  PM1.0  standard (factory)
 *   [6-7]  PM2.5  standard
 *   [8-9]  PM10.0 standard
 *   [10-11] PM1.0  atmospheric
 *   [12-13] PM2.5  atmospheric  ← we use these
 *   [14-15] PM10.0 atmospheric
 *   [16-27] Particle counts by size (not used here)
 *   [28-29] Reserved
 *   [30-31] Checksum (sum of bytes [0..29])
 */

#include "pms5003.h"

#include "board_pins.h"
#include "driver/uart.h"
#include "esp_log.h"

static const char *TAG = "pms5003";

#define UART_PORT    UART_NUM_2
#define UART_BAUD    9600
#define FRAME_LEN    32
#define START1       0x42
#define START2       0x4D
#define RX_BUF_SIZE  256

esp_err_t pms5003_init(void)
{
    const uart_config_t cfg = {
        .baud_rate  = UART_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(UART_PORT, RX_BUF_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
        return err;
    }
    err = uart_param_config(UART_PORT, &cfg);
    if (err != ESP_OK) return err;

    err = uart_set_pin(UART_PORT,
                       BOARD_PMS_TX_GPIO,  /* ESP32 TX → PMS5003 RX */
                       BOARD_PMS_RX_GPIO,  /* ESP32 RX ← PMS5003 TX */
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    uart_flush(UART_PORT);
    ESP_LOGI(TAG, "init OK (UART%d, TX=%d, RX=%d, %d baud)",
             UART_PORT, BOARD_PMS_TX_GPIO, BOARD_PMS_RX_GPIO, UART_BAUD);
    return ESP_OK;
}

esp_err_t pms5003_read(pms5003_data_t *out, uint32_t timeout_ms)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[FRAME_LEN];

    while (timeout_ms > 0) {
        /* Scan for start byte 1 — this call enforces the outer timeout */
        uint8_t b = 0;
        int n = uart_read_bytes(UART_PORT, &b, 1, pdMS_TO_TICKS(timeout_ms));
        if (n <= 0) {
            break; /* timeout elapsed */
        }
        if (b != START1) {
            continue;
        }

        /* Start byte 2 */
        n = uart_read_bytes(UART_PORT, &b, 1, pdMS_TO_TICKS(100));
        if (n <= 0 || b != START2) {
            continue;
        }

        buf[0] = START1;
        buf[1] = START2;

        /* Read the remaining 30 bytes */
        n = uart_read_bytes(UART_PORT, &buf[2], FRAME_LEN - 2, pdMS_TO_TICKS(200));
        if (n != FRAME_LEN - 2) {
            continue;
        }

        /* Verify checksum: sum of bytes [0..29] must equal bytes [30..31] */
        uint16_t checksum = 0;
        for (int i = 0; i < 30; i++) {
            checksum += buf[i];
        }
        uint16_t received = (uint16_t)(buf[30] << 8 | buf[31]);
        if (checksum != received) {
            ESP_LOGW(TAG, "checksum mismatch (calc 0x%04X, got 0x%04X)", checksum, received);
            continue;
        }

        /* Extract atmospheric PM values (bytes 10-15) */
        out->pm1_0_ug_m3 = (uint16_t)(buf[10] << 8 | buf[11]);
        out->pm2_5_ug_m3 = (uint16_t)(buf[12] << 8 | buf[13]);
        out->pm10_ug_m3 = (uint16_t)(buf[14] << 8 | buf[15]);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "timeout waiting for valid frame");
    return ESP_ERR_TIMEOUT;
}
