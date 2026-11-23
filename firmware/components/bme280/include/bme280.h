/**
 * @file bme280.h
 * @brief BME280 temperature / humidity / pressure sensor driver (I2C).
 *
 * Uses forced mode: each bme280_read() triggers one measurement at 1x
 * oversampling, waits for it to finish and returns compensated values.
 * This keeps self-heating low, which suits ambient monitoring.
 */
#pragma once

#include <stdint.h>

#include "bme280_compensate.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    i2c_master_dev_handle_t dev;
    bme280_calib_t calib;
} bme280_t;

typedef struct {
    float temperature_c; /**< degrees Celsius */
    float pressure_hpa;  /**< hectopascal (millibar) */
    float humidity_pct;  /**< relative humidity, percent */
} bme280_reading_t;

/**
 * @brief Attach to a BME280 on an existing bus, verify its ID and load calibration.
 *
 * @param[out] sensor handle to initialise
 * @param bus         bus handle from i2c_bus_get()
 * @param addr        7-bit address (see BOARD_I2C_ADDR_BME280)
 * @return ESP_OK, ESP_ERR_NOT_FOUND if no device answers or the chip ID is
 *         wrong (a BMP280 reports 0x58), or another I2C error.
 */
esp_err_t bme280_init(bme280_t *sensor, i2c_master_bus_handle_t bus, uint8_t addr);

/**
 * @brief Take one measurement.
 * @return ESP_OK, ESP_ERR_TIMEOUT if the sensor never finished, or an I2C error.
 */
esp_err_t bme280_read(bme280_t *sensor, bme280_reading_t *out);

/** @brief Remove the device from the bus and clear the handle. */
void bme280_deinit(bme280_t *sensor);

#ifdef __cplusplus
}
#endif
