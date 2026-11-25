/**
 * @file main.c
 * @brief ESP32 Environmental Monitor — application entry point.
 *
 * Startup sequence:
 *  1. Initialise the shared I2C bus.
 *  2. Initialise all sensors and the OLED display.
 *  3. Loop forever: read all sensors, display results on OLED, log to console.
 *
 * SCD40 single-shot takes ~5 s, so the main loop period is ~5 s.
 * BME280 and SGP40 readings are taken first; their temperature/humidity
 * values are passed to the SGP40 for humidity-compensated VOC measurement.
 */

#include <stdio.h>

#include "board_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "i2c_bus.h"
#include "bme280.h"
#include "sgp40.h"
#include "scd40.h"
#include "pms5003.h"
#include "ssd1306.h"

static const char *TAG = "main";

/* ── Helper: initialise one subsystem, log the result ───────────────────── */
static bool init_check(esp_err_t err, const char *name)
{
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "%-8s  OK", name);
        return true;
    }
    ESP_LOGE(TAG, "%-8s  FAILED (%s)", name, esp_err_to_name(err));
    return false;
}

/* ── Display layout (8 rows × 16 chars) ─────────────────────────────────── *
 * Row 0:  "ESP32 EnvMonitor"                                                *
 * Row 1:  "Tmp: 25.3 C"                                                     *
 * Row 2:  "Hum: 55.1 %"                                                     *
 * Row 3:  "Pre: 1013 hPa"                                                   *
 * Row 4:  "VOC raw: 32768"                                                  *
 * Row 5:  "CO2: 450 ppm"                                                    *
 * Row 6:  "PM2.5:  12 ug/m3"                                                *
 * Row 7:  "PM10:   15 ug/m3"                                                */

static void display_readings(const bme280_data_t *bme, uint16_t voc_raw, const scd40_data_t *scd,
                             const pms5003_data_t *pms)
{
    /* Scratch buffer sized for the widest possible output so the compiler can
     * prove snprintf never truncates. ssd1306_write_line() clips to the 16
     * characters that fit on a row and blanks the rest of the row. */
    char line[32];

    snprintf(line, sizeof(line), "Tmp: %.1f C     ", bme->temperature_c);
    ssd1306_write_line(1, line);

    snprintf(line, sizeof(line), "Hum: %.1f %%    ", bme->humidity_pct);
    ssd1306_write_line(2, line);

    snprintf(line, sizeof(line), "Pre: %.0f hPa ", bme->pressure_hpa);
    ssd1306_write_line(3, line);

    snprintf(line, sizeof(line), "VOC raw:%-6u", voc_raw);
    ssd1306_write_line(4, line);

    snprintf(line, sizeof(line), "CO2: %u ppm   ", scd->co2_ppm);
    ssd1306_write_line(5, line);

    snprintf(line, sizeof(line), "PM2.5: %3u ug/m3", pms->pm2_5_ug_m3);
    ssd1306_write_line(6, line);

    snprintf(line, sizeof(line), "PM10:  %3u ug/m3", pms->pm10_ug_m3);
    ssd1306_write_line(7, line);
}

/* ── Application entry point ─────────────────────────────────────────────── */
void app_main(void)
{
    ESP_LOGI(TAG, "=== ESP32 Environmental Monitor ===");

    /* I2C bus — must come before all I2C sensors */
    if (!init_check(i2c_bus_init(), "I2C bus"))
        return;

    /* Sensors & display */
    bool bme_ok = init_check(bme280_init(), "BME280");
    bool sgp_ok = init_check(sgp40_init(), "SGP40");
    bool scd_ok = init_check(scd40_init(), "SCD40");
    bool pms_ok = init_check(pms5003_init(), "PMS5003");
    bool oled_ok = init_check(ssd1306_init(), "SSD1306");

    if (oled_ok) {
        ssd1306_write_line(0, "ESP32 EnvMonitor");
    }

    if (!bme_ok && !sgp_ok && !scd_ok && !pms_ok) {
        ESP_LOGE(TAG, "No sensors available — halting.");
        return;
    }

    /* ── Main measurement loop ──────────────────────────────────────────── */
    bme280_data_t bme = {0};
    scd40_data_t scd = {0};
    pms5003_data_t pms = {0};
    uint16_t voc = 0;

    while (true) {

        /* BME280 — temperature, humidity, pressure */
        if (bme_ok) {
            esp_err_t err = bme280_read(&bme);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "BME280  T=%.2f C  H=%.2f%%  P=%.2f hPa", bme.temperature_c,
                         bme.humidity_pct, bme.pressure_hpa);
            } else {
                ESP_LOGW(TAG, "BME280 read failed: %s", esp_err_to_name(err));
            }
        }

        /* SGP40 — VOC raw (humidity-compensated using BME280 values) */
        if (sgp_ok) {
            esp_err_t err = sgp40_measure_raw(bme.temperature_c, bme.humidity_pct, &voc);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "SGP40   VOC raw=%u", voc);
            } else {
                ESP_LOGW(TAG, "SGP40 read failed: %s", esp_err_to_name(err));
            }
        }

        /* SCD40 — CO2, temperature, humidity (~5 s blocking) */
        if (scd_ok) {
            esp_err_t err = scd40_measure(&scd);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "SCD40   CO2=%u ppm  T=%.2f C  H=%.2f%%", scd.co2_ppm,
                         scd.temperature_c, scd.humidity_pct);
            } else {
                ESP_LOGW(TAG, "SCD40 read failed: %s", esp_err_to_name(err));
            }
        } else {
            vTaskDelay(pdMS_TO_TICKS(5000)); /* keep ~5 s loop period */
        }

        /* PMS5003 — particulate matter */
        if (pms_ok) {
            esp_err_t err = pms5003_read(&pms, 2000);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "PMS5003 PM1.0=%u  PM2.5=%u  PM10=%u  (ug/m3)", pms.pm1_0_ug_m3,
                         pms.pm2_5_ug_m3, pms.pm10_ug_m3);
            } else {
                ESP_LOGW(TAG, "PMS5003 read failed: %s", esp_err_to_name(err));
            }
        }

        /* Update OLED */
        if (oled_ok) {
            display_readings(&bme, voc, &scd, &pms);
        }
    }
}
