/**
 * @file board_pins.h
 * @brief GPIO assignments for the ESP32 Environmental Monitor PCB.
 *
 * Single source of truth for pin usage in firmware. Must mirror the net
 * names and GPIOs in hardware/schematic/Environment_Sensor.kicad_sch.
 * Do not change a value here without changing the schematic first.
 */
#pragma once

/* I2C bus shared by BME280, SGP40, SCD40 and SSD1306 */
#define BOARD_I2C_SDA_GPIO 21
#define BOARD_I2C_SCL_GPIO 22

/* UART2 to PMS5003. Names are ESP32-side: RX receives PMS5003 TX. */
#define BOARD_PMS_RX_GPIO 16
#define BOARD_PMS_TX_GPIO 17

/* 7-bit I2C addresses of on-board devices */
#define BOARD_I2C_ADDR_BME280  0x76 /* SDO tied to GND */
#define BOARD_I2C_ADDR_SGP40   0x59
#define BOARD_I2C_ADDR_SCD40   0x62
#define BOARD_I2C_ADDR_SSD1306 0x3C
