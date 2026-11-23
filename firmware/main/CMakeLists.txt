idf_component_register(
    SRCS "main.c"
    REQUIRES board i2c_bus bme280 sgp40 scd40 pms5003 ssd1306
)
