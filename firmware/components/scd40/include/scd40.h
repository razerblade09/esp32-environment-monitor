/**
 * @file scd40.h
 * @brief SCD40 CO2, temperature and humidity sensor driver.
 *
 * Uses the SCD40 "single-shot" measurement command (no continuous mode).
 * One measurement takes ~5 s including the internal warm-up; call
 * scd40_measure() and await its ~5 s blocking call, or call it on a timer.
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
    uint16_t co2_ppm;       /**< CO2 concentration in parts-per-million. */
    float    temperature_c; /**< On-chip temperature in degrees Celsius.  */
    float    humidity_pct;  /**< Relative humidity in percent.            */
} scd40_data_t;

/**
 * @brief Add the SCD40 to the I2C bus and stop any running measurement.
 *
 * Always call this before scd40_measure(). Sends stop_periodic_measurement
 * in case the sensor was left in continuous mode from a previous session.
 */
esp_err_t scd40_init(void);

/**
 * @brief Trigger one single-shot measurement and return the result.
 *
 * Blocks for ~5 seconds (SCD40 single-shot measurement time).
 * @param out  Pointer to scd40_data_t to fill. Must not be NULL.
 */
esp_err_t scd40_measure(scd40_data_t *out);

#ifdef __cplusplus
}
#endif
