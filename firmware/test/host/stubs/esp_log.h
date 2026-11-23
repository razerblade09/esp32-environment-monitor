/* Minimal host stand-in for ESP-IDF's esp_log.h (tests only): logging is silent. */
#pragma once
#define ESP_LOGE(tag, ...) ((void)(tag))
#define ESP_LOGW(tag, ...) ((void)(tag))
#define ESP_LOGI(tag, ...) ((void)(tag))
