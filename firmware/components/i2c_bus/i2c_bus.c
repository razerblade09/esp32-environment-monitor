#include "i2c_bus.h"

#include "board_pins.h"
#include "esp_log.h"

#define I2C_PROBE_TIMEOUT_MS 50

static const char *TAG = "i2c_bus";
static i2c_master_bus_handle_t s_bus;

esp_err_t i2c_bus_init(void)
{
    if (s_bus != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const i2c_master_bus_config_t cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        /* Internal pull-ups are a fallback only; fit external pull-ups on the PCB. */
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bus init failed: %s", esp_err_to_name(err));
        s_bus = NULL;
        return err;
    }
    ESP_LOGI(TAG, "bus ready (SDA=%d, SCL=%d)", BOARD_I2C_SDA_GPIO, BOARD_I2C_SCL_GPIO);
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_bus_get(void)
{
    return s_bus;
}

esp_err_t i2c_bus_probe(uint8_t addr)
{
    if (s_bus == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = i2c_master_probe(s_bus, addr, I2C_PROBE_TIMEOUT_MS);
    return (err == ESP_OK) ? ESP_OK : ESP_ERR_NOT_FOUND;
}
