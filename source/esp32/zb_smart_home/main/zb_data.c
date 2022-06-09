#include "zb_data.h"

#include <cJSON.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <stdlib.h>
#include <string.h>

static const char* TAG = "zb_data";

#define STORAGE_NAMESPACE "storage"
#define NVS_DEV_IEEE_KEY  "zb_ieee_list"

typedef struct {
    time_t   last_seen;
    uint64_t ieee_addr;
    uint16_t short_addr;
    uint16_t cluster_id;
} zb_nvs_info_t;

typedef struct {
    size_t         required_size;
    zb_nvs_info_t* dev_list;
} zb_nvs_dev_t;

zb_dev_t* g_zb_dev;

void _zb_add_device(uint16_t cluster_id, uint16_t short_addr,
                    uint64_t ieee_addr, zb_dev_data_t* data);
void _zb_remove_device(uint64_t ieee_addr);

void _zb_store_dev_to_nvs(void) {
    nvs_handle_t nvs_handle;
    nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);

    uint8_t dev_count = zb_get_device_count();

    zb_nvs_dev_t dev_info;
    dev_info.required_size = dev_count * sizeof(zb_nvs_info_t);
    dev_info.dev_list      = (zb_nvs_info_t*)malloc(dev_info.required_size);

    zb_dev_t* temp = g_zb_dev;

    for (uint8_t i = 0; i < dev_count; i++) {
        dev_info.dev_list[i].last_seen  = temp->last_seen;
        dev_info.dev_list[i].ieee_addr  = temp->ieee_addr;
        dev_info.dev_list[i].short_addr = temp->short_addr;
        dev_info.dev_list[i].cluster_id = temp->cluster_id;

        temp = temp->next;
    }

    nvs_set_blob(nvs_handle, NVS_DEV_IEEE_KEY, dev_info.dev_list,
                 dev_info.required_size);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    free(dev_info.dev_list);
    dev_info.dev_list = NULL;
}

void _zb_add_device(uint16_t cluster_id, uint16_t short_addr,
                    uint64_t ieee_addr, zb_dev_data_t* data) {
    _zb_remove_device(ieee_addr);
    zb_dev_t* new_dev         = (zb_dev_t*)malloc(sizeof(zb_dev_t));
    new_dev->cluster_id       = cluster_id;
    new_dev->short_addr       = short_addr;
    new_dev->ieee_addr        = ieee_addr;
    new_dev->new_data_updated = false;
    new_dev->data             = data;
    new_dev->next             = g_zb_dev;
    g_zb_dev                  = new_dev;
}

void _zb_remove_device(uint64_t ieee_addr) {
    zb_dev_t* temp = g_zb_dev;
    zb_dev_t* prev = NULL;
    if (temp != NULL && g_zb_dev->ieee_addr == ieee_addr) {
        g_zb_dev = temp->next;
        free(temp);
        temp = NULL;
        return;
    }
    while (temp != NULL && temp->ieee_addr != ieee_addr) {
        prev = temp;
        temp = temp->next;
    }
    if (temp == NULL) return;
    prev->next = temp->next;
    free(temp);
    temp = NULL;
}

void _zb_update_device_short_addr(uint16_t short_addr, uint64_t ieee_addr) {
    zb_dev_t* temp = zb_get_device_with_ieee_addr(ieee_addr);
    if (temp != NULL) temp->short_addr = short_addr;
}

void zb_device_init(void) {
    nvs_handle_t nvs_handle;
    zb_nvs_dev_t dev_info = {
        .required_size = 0,
    };

    nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &nvs_handle);

    if (nvs_get_blob(nvs_handle, NVS_DEV_IEEE_KEY, NULL,
                     &dev_info.required_size) == ESP_OK) {
        if (dev_info.required_size > 0) {
            dev_info.dev_list = (zb_nvs_info_t*)malloc(dev_info.required_size);

            nvs_get_blob(nvs_handle, NVS_DEV_IEEE_KEY, dev_info.dev_list,
                         &dev_info.required_size);

            uint8_t dev_count = dev_info.required_size / sizeof(zb_nvs_info_t);
            ESP_LOGI(TAG, "Found %d device(s) stored in nvs", dev_count);
            for (uint8_t i = 0; i < dev_count; i++) {
                _zb_add_device(dev_info.dev_list[i].cluster_id,
                               dev_info.dev_list[i].short_addr,
                               dev_info.dev_list[i].ieee_addr, NULL);
                zb_update_device_last_seen_with_time(
                    dev_info.dev_list[i].ieee_addr,
                    dev_info.dev_list[i].last_seen);
            }

            free(dev_info.dev_list);
            dev_info.dev_list = NULL;
        }
    } else {
        ESP_LOGW(TAG, "No device found in nvs");
    }
    nvs_close(nvs_handle);
}

void zb_print_dev_list(void) {
    ESP_LOGI(TAG, "Devices list");
    zb_dev_t* temp = g_zb_dev;

    if (temp == NULL) {
        ESP_LOGI(TAG, "No device");
        return;
    }

    while (temp != NULL) {
        ESP_LOGI(TAG, "0x%016llX - 0x%04X - lastseen %ld", temp->ieee_addr,
                 temp->short_addr, temp->last_seen);
        temp = temp->next;
    }
}

void zb_add_device(uint16_t short_addr, uint64_t ieee_addr) {
    if (zb_get_device_with_ieee_addr(ieee_addr) == NULL) {
        _zb_add_device(UNKNOWN_CLUSTER_ID, short_addr, ieee_addr, NULL);
        ESP_LOGI(TAG, "New device added (0x%016llX)", ieee_addr);
    } else {
        _zb_update_device_short_addr(short_addr, ieee_addr);
        ESP_LOGI(TAG, "Already in the list so no action needed (0x%016llX)",
                 ieee_addr);
    }
    zb_update_device_last_seen(ieee_addr);
    _zb_store_dev_to_nvs();
}

void zb_remove_device(uint64_t ieee_addr) {
    _zb_remove_device(ieee_addr);
    _zb_store_dev_to_nvs();
}

void zb_update_device_data(uint64_t ieee_addr, uint16_t cluster_id,
                           zb_dev_data_t* data) {
    zb_dev_t* temp = zb_get_device_with_ieee_addr(ieee_addr);

    if (temp == NULL) {
        ESP_LOGI(TAG,
                 "Cannot update data. Device is not in the list (0x%016llX)",
                 ieee_addr);
        return;
    }

    if (temp->cluster_id == UNKNOWN_CLUSTER_ID &&
        cluster_id != UNKNOWN_CLUSTER_ID)
        zb_set_device_cluster_id(ieee_addr, cluster_id);

    if (temp->data == NULL)
        temp->data = (zb_dev_data_t*)malloc(sizeof(zb_dev_data_t));
    memcpy(temp->data, data, sizeof(zb_dev_data_t));
    temp->new_data_updated = true;
    ESP_LOGI(TAG, "Data updated (0x%016llX)", ieee_addr);
}

void zb_update_device_last_seen(uint64_t ieee_addr) {
    zb_dev_t* temp = zb_get_device_with_ieee_addr(ieee_addr);
    if (temp != NULL) {
        time_t now;
        time(&now);
        temp->last_seen = now;
        temp->is_alive  = true;
    }
    ESP_LOGI(TAG, "Last seen updated (0x%016llX)", ieee_addr);
}

void zb_update_device_last_seen_with_time(uint64_t ieee_addr,
                                          time_t   last_seen) {
    zb_dev_t* temp = zb_get_device_with_ieee_addr(ieee_addr);
    if (temp != NULL) {
        time_t now;
        time(&now);
        temp->last_seen = last_seen;
        temp->is_alive  = (difftime(now, last_seen) < ZB_KEEPALIVE_TIMEOUT);
    }
}

void zb_set_device_cluster_id(uint64_t ieee_addr, uint16_t cluster_id) {
    zb_dev_t* temp = zb_get_device_with_ieee_addr(ieee_addr);
    if (temp != NULL) {
        if (temp->cluster_id == UNKNOWN_CLUSTER_ID) {
            temp->cluster_id = cluster_id;
            ESP_LOGI(TAG, "Set ClusterID to 0x%04X (0x%016llX)", cluster_id,
                     ieee_addr);
        } else {
            ESP_LOGI(TAG,
                     "Found ClusterID 0x%04X so no action needed (0x%016llX)",
                     temp->cluster_id, ieee_addr);
        }
    }
}

uint8_t zb_get_device_count(void) {
    uint8_t   count = 0;
    zb_dev_t* temp  = g_zb_dev;
    while (temp != NULL) {
        count++;
        temp = temp->next;
    }
    return count;
}

zb_dev_t* zb_get_device_list(void) { return g_zb_dev; }

zb_dev_t* zb_get_device_with_ieee_addr(uint64_t ieee_addr) {
    zb_dev_t* temp = g_zb_dev;
    while (temp != NULL) {
        if (temp->ieee_addr == ieee_addr) return temp;
        temp = temp->next;
    }
    return NULL;
}

zb_dev_t* zb_get_device_with_short_addr(uint16_t short_addr) {
    zb_dev_t* temp = g_zb_dev;
    while (temp != NULL) {
        if (temp->short_addr == short_addr) return temp;
        temp = temp->next;
    }
    return NULL;
}

void zb_remove_all_device(void) {
    while (g_zb_dev != NULL) {
        ESP_LOGI(TAG, "remove %016llX", g_zb_dev->ieee_addr);
        zb_remove_device(g_zb_dev->ieee_addr);
    }

    nvs_handle_t nvs_handle;
    nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
    nvs_erase_key(nvs_handle, NVS_DEV_IEEE_KEY);
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

char* zb_get_device_provision_msg(zb_dev_t* device) {
    if (device == NULL) return NULL;

    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "action", "provision");
    char dev_addr[17];
    snprintf(dev_addr, sizeof(dev_addr), "%016llX", device->ieee_addr);
    cJSON_AddStringToObject(json, "dev_addr", dev_addr);

    switch (device->cluster_id) {
        case CLUSTER_ID_RELAY_CTRL:
            cJSON_AddStringToObject(json, "type", "Relay3Channel");
            break;
        case CLUSTER_ID_SENSOR:
            cJSON_AddStringToObject(json, "type", "Sensor");
            break;
        case CLUSTER_ID_COMPTEUR:
            cJSON_AddStringToObject(json, "type", "RelayAde");
            break;
        default:
            break;
    }

    char* provision_msg = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return provision_msg;
}

char* zb_get_device_telemetry_msg(zb_dev_t* device) {
    if (device == NULL) return NULL;

    cJSON* json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "action", "telemetry");
    char dev_addr[17];
    snprintf(dev_addr, sizeof(dev_addr), "%016llX", device->ieee_addr);
    cJSON_AddStringToObject(json, "dev_addr", dev_addr);
    cJSON_AddNumberToObject(json, "timestamp", device->last_seen);

    if (device->data != NULL) {
        switch (device->cluster_id) {
            case CLUSTER_ID_RELAY_CTRL:
                for (uint8_t i = 0; i < device->data->relay_channel_num; i++) {
                    char rl_str[17];
                    sprintf(rl_str, "status%u", i + 1);
                    cJSON_AddStringToObject(
                        json, rl_str,
                        (device->data->relay_bit_mask & (1 << i)) ? "ON"
                                                                  : "OFF");
                }
                break;

            case CLUSTER_ID_SENSOR:
                cJSON_AddNumberToObject(json, "temp",
                                        device->data->temperature);
                cJSON_AddNumberToObject(json, "humidity",
                                        device->data->humidity);
                cJSON_AddNumberToObject(json, "airquality",
                                        device->data->air_quality);
                break;

            case CLUSTER_ID_COMPTEUR:
                cJSON_AddNumberToObject(json, "irms", device->data->irms);
                cJSON_AddNumberToObject(json, "vrms", device->data->vrms);
                cJSON_AddNumberToObject(json, "power", device->data->power);
                cJSON_AddStringToObject(json, "status",
                                        device->data->relay ? "ON" : "OFF");
                break;
            default:
                break;
        }
    }

    char* telemetry_msg = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    return telemetry_msg;
}
