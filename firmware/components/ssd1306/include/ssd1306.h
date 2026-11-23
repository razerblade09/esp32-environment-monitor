/**
 * @file ssd1306.h
 * @brief SSD1306 128×64 OLED display driver (I2C, text-only).
 *
 * Provides a minimal text display — 4 rows of 16 characters using the
 * built-in 8×8 font. No graphics library dependency.
 *
 * Datasheet: https://cdn-shop.adafruit.com/datasheets/SSD1306.pdf
 */
#pragma once

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the OLED: send startup command sequence, clear screen.
 *
 * Call after i2c_bus_init().
 */
esp_err_t ssd1306_init(void);

/**
 * @brief Clear the entire display (fill with black).
 */
esp_err_t ssd1306_clear(void);

/**
 * @brief Write a text string at the given character row (0–7 for 8-pixel rows).
 *
 * Characters beyond column 16 are truncated. Non-printable bytes are
 * replaced with spaces.
 *
 * @param row   Display row (0 = top).
 * @param text  Null-terminated ASCII string.
 */
esp_err_t ssd1306_write_line(uint8_t row, const char *text);

#ifdef __cplusplus
}
#endif
