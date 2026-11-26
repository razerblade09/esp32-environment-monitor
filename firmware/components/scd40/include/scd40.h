/**
 * @file scd40.h
 * @brief SCD40 CO2, temperature and humidity sensor driver.
 *
 * Uses the SCD40 periodic measurement mode (one new sample every ~5 s).
 * The single-shot commands are SCD41-only and are not available on the SCD40.
 * scd40_measure() waits for the next sample, so it blocks for up to ~5 s.
 *
 * Datasheet: https://sensirion.com/products/catalog/SCD40/
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Measurement result returned by scd40_measure(). */
typedef struct {
    uint16_t co2_ppm;    /**< CO2 concentration in parts-per-million. */
    float temperature_c; /**< On-chip temperature in degrees Celsius.  */
    float humidity_pct;  /**< Relative humidity in percent.            */
} scd40_data_t;

/**
 * @brief Add the SCD40 to the I2C bus and start periodic measurement.
 *
 * Always call this before scd40_measure(). Stops any previous measurement
 * first, checks the serial number, then starts periodic mode.
 */
esp_err_t scd40_init(void);

/**
 * @brief Wait for the next periodic sample and return it.
 *
 * Blocks until data is ready (up to ~5 s, 7 s timeout -> ESP_ERR_TIMEOUT).
 * @param out  Pointer to scd40_data_t to fill. Must not be NULL.
 */
esp_err_t scd40_measure(scd40_data_t *out);

#ifdef __cplusplus
}
#endif
