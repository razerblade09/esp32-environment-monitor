/**
 * @file bme280.h
 * @brief BME280 temperature, humidity and pressure sensor driver.
 *
 * Uses the shared I2C master bus (i2c_bus component). Call bme280_init()
 * once after i2c_bus_init(), then bme280_read() as often as needed.
 *
 * Hardware: BME280 at I2C address 0x76 (SDO tied to GND on this board).
 * Datasheet: https://www.bosch-sensortec.com/products/environmental-sensors/humidity-sensors-bme280/
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Measurement result returned by bme280_read(). */
typedef struct {
    float temperature_c;  /**< Temperature in degrees Celsius. */
    float pressure_hpa;   /**< Pressure in hectopascals (hPa). */
    float humidity_pct;   /**< Relative humidity in percent (0–100). */
} bme280_data_t;

/**
 * @brief Add the BME280 to the I2C bus and read its calibration data.
 *
 * Must be called after i2c_bus_init() and before bme280_read().
 * Returns ESP_ERR_NOT_FOUND if no device acknowledges at address 0x76,
 * or ESP_ERR_INVALID_RESPONSE if the chip ID is wrong.
 */
esp_err_t bme280_init(void);

/**
 * @brief Trigger one forced measurement and return the compensated result.
 *
 * Blocks for ~10 ms while the sensor takes the measurement.
 * @param out  Pointer to a bme280_data_t to fill. Must not be NULL.
 */
esp_err_t bme280_read(bme280_data_t *out);

#ifdef __cplusplus
}
#endif
