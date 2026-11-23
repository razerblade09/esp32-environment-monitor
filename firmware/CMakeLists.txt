cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Extra warnings for project code (ESP-IDF already enables -Wall).
idf_build_set_property(COMPILE_OPTIONS "-Wextra" APPEND)

project(esp32_environment_monitor)
