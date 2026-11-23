/**
 * @file sgp40.h
 * @brief SGP40 raw VOC index sensor driver.
 *
 * The SGP40 returns a raw signal (0–65535) that must be fed into Sensirion's
 * Gas Index Algorithm (GIA) to produce a VOC index (1–500). This driver
 * returns the raw value; plug it into the GIA library when you add it.
 *
 * Datasheet: https://sensirion.com/products/catalog/SGP40/
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Add the SGP40 to the I2C bus and run a self-test.
 *
 * The self-test takes ~320 ms. Returns ESP_ERR_INVALID_RESPONSE if it fails.
 */
esp_err_t sgp40_init(void);

/**
 * @brief Measure raw VOC signal with humidity-compensation inputs.
 *
 * Pass current temperature and relative humidity from the BME280 so the
 * SGP40 can compensate its signal internally.
 *
 * @param temperature_c   Ambient temperature in °C (e.g. from BME280).
 * @param humidity_pct    Relative humidity in % (e.g. from BME280).
 * @param raw_out         Receives the raw 16-bit VOC signal (0–65535).
 */
esp_err_t sgp40_measure_raw(float temperature_c, float humidity_pct, uint16_t *raw_out);

#ifdef __cplusplus
}
#endif
