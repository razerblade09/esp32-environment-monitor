/**
 * @file pms5003.h
 * @brief PMS5003 particulate matter sensor driver (UART2).
 *
 * The PMS5003 sends 32-byte frames at 9600 baud over UART.
 * This driver installs UART2 on the board's PMS_RX/TX pins and reads
 * one complete frame, returning PM1.0, PM2.5 and PM10 in µg/m³.
 *
 * Datasheet: https://www.plantower.com/en/products30/78.html
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** PM concentration readings (atmospheric environment values). */
typedef struct {
    uint16_t pm1_0_ug_m3;   /**< PM1.0  concentration in µg/m³. */
    uint16_t pm2_5_ug_m3;   /**< PM2.5  concentration in µg/m³. */
    uint16_t pm10_ug_m3;    /**< PM10.0 concentration in µg/m³. */
} pms5003_data_t;

/**
 * @brief Install UART2 driver and flush the input buffer.
 *
 * Call once at startup. UART2 is configured for 9600-8-N-1.
 */
esp_err_t pms5003_init(void);

/**
 * @brief Read one complete 32-byte data frame from the sensor.
 *
 * Blocks until a valid frame arrives or the timeout elapses.
 * The PMS5003 normally sends a frame every ~1 s in active mode.
 *
 * @param out         Pointer to pms5003_data_t to fill.
 * @param timeout_ms  Maximum wait time in milliseconds (recommend ≥ 2000).
 */
esp_err_t pms5003_read(pms5003_data_t *out, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
