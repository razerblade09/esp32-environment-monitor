/* Minimal host stand-in for ESP-IDF's driver/i2c_master.h (tests only). */
#pragma once
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

typedef struct fake_bus *i2c_master_bus_handle_t;
typedef struct fake_dev *i2c_master_dev_handle_t;

typedef enum { I2C_ADDR_BIT_LEN_7 = 0 } i2c_addr_bit_len_t;

typedef struct {
    i2c_addr_bit_len_t dev_addr_length;
    uint16_t device_address;
    uint32_t scl_speed_hz;
} i2c_device_config_t;

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus, const i2c_device_config_t *cfg,
                                    i2c_master_dev_handle_t *dev);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev, const uint8_t *buf, size_t len,
                              int timeout_ms);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev, const uint8_t *wbuf,
                                      size_t wlen, uint8_t *rbuf, size_t rlen, int timeout_ms);
