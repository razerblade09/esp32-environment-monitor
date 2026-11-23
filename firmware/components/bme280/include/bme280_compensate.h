/**
 * @file bme280_compensate.h
 * @brief Pure-C BME280 calibration parsing and compensation maths.
 *
 * No ESP-IDF dependencies, so it can be unit-tested on a host computer.
 * Formulas are the fixed-point reference implementation from the Bosch
 * BME280 datasheet (section 4.2.3).
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Factory calibration coefficients stored in the sensor's NVM. */
typedef struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
    uint16_t dig_P1;
    int16_t dig_P2;
    int16_t dig_P3;
    int16_t dig_P4;
    int16_t dig_P5;
    int16_t dig_P6;
    int16_t dig_P7;
    int16_t dig_P8;
    int16_t dig_P9;
    uint8_t dig_H1;
    int16_t dig_H2;
    uint8_t dig_H3;
    int16_t dig_H4;
    int16_t dig_H5;
    int8_t dig_H6;
} bme280_calib_t;

/** Size of calibration block 1 (registers 0x88..0xA1). */
#define BME280_CALIB1_LEN 26
/** Size of calibration block 2 (registers 0xE1..0xE7). */
#define BME280_CALIB2_LEN 7

/**
 * @brief Decode the two raw calibration register blocks.
 * @param block1 registers 0x88..0xA1 (BME280_CALIB1_LEN bytes)
 * @param block2 registers 0xE1..0xE7 (BME280_CALIB2_LEN bytes)
 */
void bme280_parse_calib(const uint8_t block1[BME280_CALIB1_LEN],
                        const uint8_t block2[BME280_CALIB2_LEN], bme280_calib_t *calib);

/**
 * @brief Compensate a raw temperature reading.
 * @param adc_t 20-bit raw temperature
 * @param t_fine out: fine-resolution temperature, required by the pressure
 *               and humidity compensation
 * @return temperature in hundredths of a degree Celsius (2508 = 25.08 C)
 */
int32_t bme280_compensate_temperature(const bme280_calib_t *calib, int32_t adc_t, int32_t *t_fine);

/**
 * @brief Compensate a raw pressure reading.
 * @param adc_p 20-bit raw pressure
 * @return pressure in Pa as unsigned Q24.8 (divide by 256), 0 on invalid calibration
 */
uint32_t bme280_compensate_pressure(const bme280_calib_t *calib, int32_t adc_p, int32_t t_fine);

/**
 * @brief Compensate a raw humidity reading.
 * @param adc_h 16-bit raw humidity
 * @return relative humidity in %RH as unsigned Q22.10 (divide by 1024)
 */
uint32_t bme280_compensate_humidity(const bme280_calib_t *calib, int32_t adc_h, int32_t t_fine);

#ifdef __cplusplus
}
#endif
