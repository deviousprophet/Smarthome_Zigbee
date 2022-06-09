#pragma once

#include <esp_err.h>
#include <stdbool.h>

esp_err_t esp_wifi_is_provisioned(bool* provisioned);
esp_err_t esp_wifi_clear_config(void);

esp_err_t mqtt_store_uri(const char* uri);
esp_err_t mqtt_get_uri(char* uri, size_t* uri_len);
esp_err_t mqtt_clear_uri(void);
esp_err_t mqtt_is_provisioned(bool* provisioned);