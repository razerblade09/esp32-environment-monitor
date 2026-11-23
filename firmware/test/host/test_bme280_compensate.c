/*
 * Host-side unit tests for bme280_compensate.c. No hardware or ESP-IDF needed.
 * Build and run with ./run.sh
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bme280_compensate.h"

static int failures;

#define CHECK(cond, ...)                                                                           \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                            \
            printf(__VA_ARGS__);                                                                   \
            printf("\n");                                                                          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

/* Calibration and raw values from the Bosch BMP280/BME280 datasheet example
 * (T and P share the same compensation maths). Humidity coefficients are
 * typical values for a real device. */
static const bme280_calib_t k_calib = {
    .dig_T1 = 27504,
    .dig_T2 = 26435,
    .dig_T3 = -1000,
    .dig_P1 = 36477,
    .dig_P2 = -10685,
    .dig_P3 = 3024,
    .dig_P4 = 2855,
    .dig_P5 = 140,
    .dig_P6 = -7,
    .dig_P7 = 15500,
    .dig_P8 = -14600,
    .dig_P9 = 6000,
    .dig_H1 = 75,
    .dig_H2 = 362,
    .dig_H3 = 0,
    .dig_H4 = 312,
    .dig_H5 = 50,
    .dig_H6 = 30,
};

/* Datasheet floating-point reference for pressure (section 4.2.3). */
static double pressure_reference(const bme280_calib_t *c, int32_t adc_p, int32_t t_fine)
{
    double var1 = ((double)t_fine / 2.0) - 64000.0;
    double var2 = var1 * var1 * (double)c->dig_P6 / 32768.0;
    var2 = var2 + var1 * (double)c->dig_P5 * 2.0;
    var2 = (var2 / 4.0) + ((double)c->dig_P4 * 65536.0);
    var1 = ((double)c->dig_P3 * var1 * var1 / 524288.0 + (double)c->dig_P2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * (double)c->dig_P1;
    double p = 1048576.0 - (double)adc_p;
    p = (p - (var2 / 4096.0)) * 6250.0 / var1;
    var1 = (double)c->dig_P9 * p * p / 2147483648.0;
    var2 = p * (double)c->dig_P8 / 32768.0;
    return p + (var1 + var2 + (double)c->dig_P7) / 16.0;
}

static void test_temperature_and_pressure_datasheet_example(void)
{
    int32_t t_fine;
    int32_t t = bme280_compensate_temperature(&k_calib, 519888, &t_fine);
    CHECK(t == 2508, "temperature: expected 2508 (25.08 C), got %d", (int)t);

    /* Datasheet example: about 100653.27 Pa. */
    double pa = bme280_compensate_pressure(&k_calib, 415148, t_fine) / 256.0;
    CHECK(fabs(pa - 100653.27) < 0.05, "pressure: expected ~100653.27 Pa, got %.4f", pa);

    /* Integer maths must agree with the float reference to within 1 LSB (1/256 Pa). */
    double ref = pressure_reference(&k_calib, 415148, t_fine);
    CHECK(fabs(pa - ref) < 2.0 / 256.0, "pressure: integer %.4f vs float reference %.4f", pa, ref);
}

static void test_pressure_matches_float_reference_over_range(void)
{
    int32_t t_fine;
    (void)bme280_compensate_temperature(&k_calib, 519888, &t_fine);

    /* Roughly 300 hPa to 1100 hPa */
    for (int32_t adc_p = 300000; adc_p <= 520000; adc_p += 10000) {
        double got = bme280_compensate_pressure(&k_calib, adc_p, t_fine) / 256.0;
        double want = pressure_reference(&k_calib, adc_p, t_fine);
        CHECK(fabs(got - want) < 0.05, "adc_p=%d: got %.3f Pa, reference %.3f", (int)adc_p, got,
              want);
    }
}

/* Datasheet floating-point reference for humidity (section 4.2.3). */
static double humidity_reference(const bme280_calib_t *c, int32_t adc_h, int32_t t_fine)
{
    double v = (double)t_fine - 76800.0;
    v = (adc_h - ((double)c->dig_H4 * 64.0 + (double)c->dig_H5 / 16384.0 * v)) *
        ((double)c->dig_H2 / 65536.0 *
         (1.0 + (double)c->dig_H6 / 67108864.0 * v * (1.0 + (double)c->dig_H3 / 67108864.0 * v)));
    v = v * (1.0 - (double)c->dig_H1 * v / 524288.0);
    if (v > 100.0) {
        v = 100.0;
    } else if (v < 0.0) {
        v = 0.0;
    }
    return v;
}

static void test_humidity_matches_float_reference(void)
{
    int32_t t_fine;
    (void)bme280_compensate_temperature(&k_calib, 519888, &t_fine);

    for (int32_t adc_h = 20000; adc_h <= 40000; adc_h += 500) {
        double got = bme280_compensate_humidity(&k_calib, adc_h, t_fine) / 1024.0;
        double want = humidity_reference(&k_calib, adc_h, t_fine);
        CHECK(fabs(got - want) < 0.05, "adc_h=%d: got %.3f %%RH, reference %.3f", (int)adc_h, got,
              want);
    }
}

static void test_humidity_is_clamped(void)
{
    int32_t t_fine;
    (void)bme280_compensate_temperature(&k_calib, 519888, &t_fine);

    uint32_t lo = bme280_compensate_humidity(&k_calib, 0, t_fine);
    uint32_t hi = bme280_compensate_humidity(&k_calib, 65535, t_fine);
    CHECK(lo == 0, "humidity floor: expected 0, got %u", (unsigned)lo);
    CHECK(hi <= 100u * 1024u, "humidity ceiling: got %.2f %%RH", hi / 1024.0);
}

static void test_pressure_invalid_calibration_returns_zero(void)
{
    bme280_calib_t bad = k_calib;
    bad.dig_P1 = 0; /* would divide by zero */
    CHECK(bme280_compensate_pressure(&bad, 415148, 100000) == 0,
          "pressure with dig_P1 == 0 must return 0");
}

static void test_parse_calib(void)
{
    uint8_t b1[BME280_CALIB1_LEN] = {0};
    uint8_t b2[BME280_CALIB2_LEN] = {0};

    /* T1 = 27504 = 0x6B70, T2 = 26435 = 0x6743, T3 = -1000 = 0xFC18 */
    b1[0] = 0x70;
    b1[1] = 0x6B;
    b1[2] = 0x43;
    b1[3] = 0x67;
    b1[4] = 0x18;
    b1[5] = 0xFC;
    /* P9 = 6000 = 0x1770 */
    b1[22] = 0x70;
    b1[23] = 0x17;
    b1[25] = 75; /* H1 */

    /* H2 = 362 = 0x016A, H3 = 0, H4 = 312 = 0x138 -> E4 = 0x13, E5[3:0] = 0x8,
     * H5 = -3 = 0xFFD -> E6 = 0xFF, E5[7:4] = 0xD, H6 = -5 */
    b2[0] = 0x6A;
    b2[1] = 0x01;
    b2[2] = 0;
    b2[3] = 0x13;
    b2[4] = 0xD8;
    b2[5] = 0xFF;
    b2[6] = (uint8_t)-5;

    bme280_calib_t c;
    bme280_parse_calib(b1, b2, &c);

    CHECK(c.dig_T1 == 27504, "T1 %u", (unsigned)c.dig_T1);
    CHECK(c.dig_T2 == 26435, "T2 %d", c.dig_T2);
    CHECK(c.dig_T3 == -1000, "T3 %d", c.dig_T3);
    CHECK(c.dig_P9 == 6000, "P9 %d", c.dig_P9);
    CHECK(c.dig_H1 == 75, "H1 %u", (unsigned)c.dig_H1);
    CHECK(c.dig_H2 == 362, "H2 %d", c.dig_H2);
    CHECK(c.dig_H3 == 0, "H3 %u", (unsigned)c.dig_H3);
    CHECK(c.dig_H4 == 312, "H4 %d", c.dig_H4);
    CHECK(c.dig_H5 == -3, "H5 %d", c.dig_H5);
    CHECK(c.dig_H6 == -5, "H6 %d", c.dig_H6);
}

int main(void)
{
    test_temperature_and_pressure_datasheet_example();
    test_pressure_matches_float_reference_over_range();
    test_humidity_matches_float_reference();
    test_humidity_is_clamped();
    test_pressure_invalid_calibration_returns_zero();
    test_parse_calib();

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all BME280 compensation tests passed\n");
    return EXIT_SUCCESS;
}
