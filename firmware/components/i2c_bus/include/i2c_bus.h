/**
 * @file i2c_bus.h
 * @brief Shared I2C master bus used by all on-board I2C devices.
 */
#pragma once

#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the shared I2C master bus on the board's SDA/SCL pins.
 *
 * Safe to call once at startup. Returns ESP_ERR_INVALID_STATE if the bus
 * has already been initialised.
 */
esp_err_t i2c_bus_init(void);

/** @brief Get the bus handle for adding devices. NULL before i2c_bus_init(). */
i2c_master_bus_handle_t i2c_bus_get(void);

/**
 * @brief Probe a 7-bit address on the bus.
 * @return ESP_OK if a device acknowledged, ESP_ERR_NOT_FOUND otherwise.
 */
esp_err_t i2c_bus_probe(uint8_t addr);

#ifdef __cplusplus
}
#endif
