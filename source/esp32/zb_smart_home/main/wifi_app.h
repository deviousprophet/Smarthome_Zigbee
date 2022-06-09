#pragma once

#include <esp_err.h>
#include <stdbool.h>

#define MQTT_BROKER_URL_DEFAULT "broker.hivemq.com"
// #define MQTT_BROKER_URL_DEFAULT "172.29.5.92"
#define MQTT_PUB_TOPIC          "mybk/up"
#define MQTT_SUB_TOPIC          "mybk/down"

esp_err_t esp_wifi_is_provisioned(bool* provisioned);
esp_err_t esp_wifi_clear_config(void);

esp_err_t mqtt_store_uri(const char* uri);
esp_err_t mqtt_get_uri(char* uri);
esp_err_t mqtt_clear_uri(void);
