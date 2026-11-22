#include <stdint.h>

#include "board_pins.h"
#include "esp_log.h"
#include "i2c_bus.h"

static const char *TAG = "main";

static void report_device(const char *name, uint8_t addr)
{
    if (i2c_bus_probe(addr) == ESP_OK) {
        ESP_LOGI(TAG, "%-8s 0x%02X  found", name, addr);
    } else {
        ESP_LOGW(TAG, "%-8s 0x%02X  NOT found", name, addr);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Environmental Monitor starting");

    if (i2c_bus_init() != ESP_OK) {
        ESP_LOGE(TAG, "cannot continue without I2C bus");
        return;
    }

    report_device("BME280", BOARD_I2C_ADDR_BME280);
    report_device("SGP40", BOARD_I2C_ADDR_SGP40);
    report_device("SCD40", BOARD_I2C_ADDR_SCD40);
    report_device("SSD1306", BOARD_I2C_ADDR_SSD1306);
}
