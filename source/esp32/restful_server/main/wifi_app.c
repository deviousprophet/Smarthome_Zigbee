#include "wifi_app.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <string.h>

static const char* TAG = "wifi_app";

esp_err_t esp_wifi_is_provisioned(bool* provisioned) {
    if (!provisioned) return ESP_ERR_INVALID_ARG;

    *provisioned = false;

    /* Get Wi-Fi Station configuration */
    wifi_config_t wifi_cfg;
    if (esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK) return ESP_FAIL;

    if (strlen((const char*)wifi_cfg.sta.ssid)) {
        *provisioned = true;

        ESP_LOGI(
            TAG, "Found Wi-Fi SSID     : %.*s",
            strnlen((const char*)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid)),
            (const char*)wifi_cfg.sta.ssid);

        size_t passlen = strlen((const char*)wifi_cfg.sta.password);
        if (passlen) {
            memset(wifi_cfg.sta.password + (passlen > 3), '*',
                   passlen - 2 * (passlen > 3));
            ESP_LOGI(TAG, "Found Wi-Fi Password : %s",
                     (const char*)wifi_cfg.sta.password);
        }
    }
    return ESP_OK;
}

esp_err_t esp_wifi_clear_config(void) {
    wifi_config_t wifi_config;
    bzero(&wifi_config, sizeof(wifi_config_t));
    return esp_wifi_set_config(0, &wifi_config);
}

esp_err_t mqtt_store_uri(const char* uri) {
    esp_err_t    ret;
    nvs_handle_t nvs_handle;
    ret = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;
    ret = nvs_set_str(nvs_handle, "mqtt_server", uri);
    if (ret != ESP_OK) return ret;
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) return ret;
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t mqtt_get_uri(char* uri, size_t* uri_len) {
    esp_err_t    ret;
    nvs_handle_t nvs_handle;
    ret = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;
    ret = nvs_get_str(nvs_handle, "mqtt_server", uri, uri_len);
    if (ret != ESP_OK) return ret;
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) return ret;
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t mqtt_clear_uri(void) {
    esp_err_t    ret;
    nvs_handle_t nvs_handle;
    ret = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) return ret;
    ret = nvs_erase_key(nvs_handle, "mqtt_server");
    if (ret != ESP_OK) return ret;
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) return ret;
    nvs_close(nvs_handle);
    return ret;
}

esp_err_t mqtt_is_provisioned(bool* provisioned) {
    if (!provisioned) return ESP_ERR_INVALID_ARG;

    *provisioned = false;

    char      uri[20];
    size_t    uri_len;
    esp_err_t ret = mqtt_get_uri(uri, &uri_len);
    if (ret != ESP_OK) return ret;

    if (uri_len > 8) {
        *provisioned = true;
        ESP_LOGI(TAG, "Found MQTT Server: %s", uri);
    }

    return ret;
}