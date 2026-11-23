idf_component_register(
    SRCS "bme280.c" "bme280_compensate.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
)
