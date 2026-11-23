/*
 * Host-side test of the BME280 driver against a fake I2C device that behaves
 * like a real sensor holding the Bosch datasheet example data.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bme280.h"

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

/* ---- fake sensor ------------------------------------------------------- */

struct fake_dev {
    uint8_t regs[256];
    int attached;
    int ctrl_hum_writes;
    int ctrl_meas_writes;
    int hum_before_meas; /* ctrl_hum written before ctrl_meas */
    int nack;            /* simulate a missing device */
};

static struct fake_dev g_dev;

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus, const i2c_device_config_t *cfg,
                                    i2c_master_dev_handle_t *dev)
{
    (void)bus;
    (void)cfg;
    g_dev.attached = 1;
    *dev = &g_dev;
    return ESP_OK;
}

esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev)
{
    dev->attached = 0;
    return ESP_OK;
}

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev, const uint8_t *buf, size_t len,
                              int timeout_ms)
{
    (void)timeout_ms;
    if (dev->nack) {
        return ESP_FAIL;
    }
    if (len == 2) {
        dev->regs[buf[0]] = buf[1];
        if (buf[0] == 0xF2) {
            dev->ctrl_hum_writes++;
        }
        if (buf[0] == 0xF4) {
            if (dev->ctrl_meas_writes == 0 && dev->ctrl_hum_writes > 0) {
                dev->hum_before_meas = 1;
            }
            dev->ctrl_meas_writes++;
        }
    }
    return ESP_OK;
}

esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev, const uint8_t *wbuf, size_t wlen,
                                      uint8_t *rbuf, size_t rlen, int timeout_ms)
{
    (void)timeout_ms;
    if (dev->nack || wlen != 1) {
        return ESP_FAIL;
    }
    for (size_t i = 0; i < rlen; i++) {
        rbuf[i] = dev->regs[(uint8_t)(wbuf[0] + i)];
    }
    return ESP_OK;
}

/* ---- helpers ----------------------------------------------------------- */

static void put16(uint8_t reg, uint16_t v)
{
    g_dev.regs[reg] = v & 0xFF;
    g_dev.regs[reg + 1] = v >> 8;
}

static void reset_fake(uint8_t chip_id)
{
    memset(&g_dev, 0, sizeof(g_dev));
    g_dev.regs[0xD0] = chip_id;

    /* Calibration from the datasheet example (see test_bme280_compensate.c) */
    put16(0x88, 27504);
    put16(0x8A, (uint16_t)26435);
    put16(0x8C, (uint16_t)-1000);
    put16(0x8E, 36477);
    put16(0x90, (uint16_t)-10685);
    put16(0x92, 3024);
    put16(0x94, 2855);
    put16(0x96, 140);
    put16(0x98, (uint16_t)-7);
    put16(0x9A, 15500);
    put16(0x9C, (uint16_t)-14600);
    put16(0x9E, 6000);
    g_dev.regs[0xA1] = 75;   /* H1 */
    put16(0xE1, 362);        /* H2 */
    g_dev.regs[0xE3] = 0;    /* H3 */
    g_dev.regs[0xE4] = 0x13; /* H4 = 312 */
    g_dev.regs[0xE5] = 0x08;
    g_dev.regs[0xE6] = 0x03; /* H5 = 50 = 0x032 -> E6 = 0x03, E5[7:4] = 0x2 */
    g_dev.regs[0xE5] |= 0x20;
    g_dev.regs[0xE7] = 30; /* H6 */

    /* Raw data: pressure 415148, temperature 519888, humidity 30000 */
    int32_t p = 415148, t = 519888, h = 30000;
    g_dev.regs[0xF7] = p >> 12;
    g_dev.regs[0xF8] = (p >> 4) & 0xFF;
    g_dev.regs[0xF9] = (p & 0x0F) << 4;
    g_dev.regs[0xFA] = t >> 12;
    g_dev.regs[0xFB] = (t >> 4) & 0xFF;
    g_dev.regs[0xFC] = (t & 0x0F) << 4;
    g_dev.regs[0xFD] = h >> 8;
    g_dev.regs[0xFE] = h & 0xFF;
}

/* ---- tests ------------------------------------------------------------- */

static void test_init_and_read(void)
{
    reset_fake(0x60);
    bme280_t s;
    CHECK(bme280_init(&s, (i2c_master_bus_handle_t)1, 0x76) == ESP_OK, "init should succeed");

    bme280_reading_t r;
    CHECK(bme280_read(&s, &r) == ESP_OK, "read should succeed");
    CHECK(fabsf(r.temperature_c - 25.08f) < 0.005f, "temperature %.3f", r.temperature_c);
    CHECK(fabsf(r.pressure_hpa - 1006.5327f) < 0.01f, "pressure %.3f hPa", r.pressure_hpa);
    CHECK(r.humidity_pct > 0.0f && r.humidity_pct <= 100.0f, "humidity %.2f", r.humidity_pct);

    CHECK(g_dev.hum_before_meas, "ctrl_hum must be written before ctrl_meas");
    CHECK(g_dev.regs[0xF4] == 0x25, "ctrl_meas should be 0x25, got 0x%02X", g_dev.regs[0xF4]);
    CHECK(g_dev.regs[0xF2] == 0x01, "ctrl_hum should be 0x01, got 0x%02X", g_dev.regs[0xF2]);

    bme280_deinit(&s);
    CHECK(!g_dev.attached, "deinit should detach the device");
}

static void set_raw(int32_t p, int32_t t, int32_t h)
{
    g_dev.regs[0xF7] = p >> 12;
    g_dev.regs[0xF8] = (p >> 4) & 0xFF;
    g_dev.regs[0xF9] = (p & 0x0F) << 4;
    g_dev.regs[0xFA] = t >> 12;
    g_dev.regs[0xFB] = (t >> 4) & 0xFF;
    g_dev.regs[0xFC] = (t & 0x0F) << 4;
    g_dev.regs[0xFD] = h >> 8;
    g_dev.regs[0xFE] = h & 0xFF;
}

/* Raw values whose low bits are all non-zero, so every bit of the 20-bit and
 * 16-bit fields must be unpacked correctly to match the reference maths. */
static void test_raw_unpacking_uses_all_bits(void)
{
    static const int32_t cases[][3] = {
        {415149, 519893, 30001},
        {415163, 519903, 30011},
        {400007, 500015, 27777},
        {0x5ABCD, 0x7BCDF, 0x6543},
    };
    bme280_calib_t calib;
    reset_fake(0x60);
    bme280_t s;
    CHECK(bme280_init(&s, (i2c_master_bus_handle_t)1, 0x76) == ESP_OK, "init");
    calib = s.calib;

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int32_t p = cases[i][0], t = cases[i][1], h = cases[i][2];
        set_raw(p, t, h);

        int32_t t_fine;
        float want_t = bme280_compensate_temperature(&calib, t, &t_fine) / 100.0f;
        float want_p = bme280_compensate_pressure(&calib, p, t_fine) / 256.0f / 100.0f;
        float want_h = bme280_compensate_humidity(&calib, h, t_fine) / 1024.0f;

        bme280_reading_t r;
        CHECK(bme280_read(&s, &r) == ESP_OK, "case %zu: read", i);
        CHECK(r.temperature_c == want_t, "case %zu: temp %.3f vs %.3f", i, r.temperature_c, want_t);
        CHECK(r.pressure_hpa == want_p, "case %zu: press %.3f vs %.3f", i, r.pressure_hpa, want_p);
        CHECK(r.humidity_pct == want_h, "case %zu: hum %.3f vs %.3f", i, r.humidity_pct, want_h);
    }
}

static void test_wrong_chip_id(void)
{
    reset_fake(0x58); /* BMP280: no humidity */
    bme280_t s;
    CHECK(bme280_init(&s, (i2c_master_bus_handle_t)1, 0x76) == ESP_ERR_NOT_FOUND,
          "a BMP280 must be rejected");
    CHECK(!g_dev.attached, "failed init must detach the device");
}

static void test_no_device(void)
{
    reset_fake(0x60);
    g_dev.nack = 1;
    bme280_t s;
    CHECK(bme280_init(&s, (i2c_master_bus_handle_t)1, 0x76) == ESP_ERR_NOT_FOUND,
          "missing device must give ESP_ERR_NOT_FOUND");
    CHECK(!g_dev.attached, "failed init must detach the device");
}

static void test_skipped_channel_is_rejected(void)
{
    reset_fake(0x60);
    bme280_t s;
    bme280_reading_t r;
    CHECK(bme280_init(&s, (i2c_master_bus_handle_t)1, 0x76) == ESP_OK, "init");
    g_dev.regs[0xFA] = 0x80; /* temperature raw = 0x80000 (skipped) */
    g_dev.regs[0xFB] = 0x00;
    g_dev.regs[0xFC] = 0x00;
    CHECK(bme280_read(&s, &r) == ESP_ERR_INVALID_RESPONSE, "skipped channel must be an error");
}

static void test_invalid_args(void)
{
    bme280_t s;
    bme280_reading_t r;
    CHECK(bme280_init(NULL, (i2c_master_bus_handle_t)1, 0x76) == ESP_ERR_INVALID_ARG,
          "null sensor");
    CHECK(bme280_init(&s, NULL, 0x76) == ESP_ERR_INVALID_ARG, "null bus");
    s.dev = NULL;
    CHECK(bme280_read(&s, &r) == ESP_ERR_INVALID_ARG, "read before init");
}

int main(void)
{
    test_init_and_read();
    test_raw_unpacking_uses_all_bits();
    test_wrong_chip_id();
    test_no_device();
    test_skipped_channel_is_rejected();
    test_invalid_args();

    if (failures) {
        printf("%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all BME280 driver tests passed\n");
    return EXIT_SUCCESS;
}
