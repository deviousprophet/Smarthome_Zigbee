#include <string.h>

#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "lwip/apps/netbiosns.h"
#include "mdns.h"
#include "nvs_flash.h"
#include "wifi_app.h"

#define MDNS_INSTANCE "esp home web server"

static const char* TAG = "example";

/* Global variables for tasks */
static esp_ip4_addr_t device_ipv4;
static char           mqtt_server_uri[20];

/* Event group */
EventGroupHandle_t event_group;
#define WIFI_CONNECTED_BIT        BIT1
#define WIFI_PROVISIONED_BIT      BIT2
#define ESPTOUCH_DONE_BIT         BIT3
#define MQTT_CONNECTED_BIT        BIT4
#define RTC_CONNECTED_BIT         BIT5
#define ZNP_COORD_CONFIG_DONE_BIT BIT6

esp_err_t start_rest_server(const char* base_path);

static void initialise_mdns(void) {
    mdns_init();
    mdns_hostname_set(CONFIG_EXAMPLE_MDNS_HOST_NAME);
    mdns_instance_name_set(MDNS_INSTANCE);

    mdns_txt_item_t serviceTxtData[] = {
        {"board", "esp32"},
        {"path", "/"},
    };

    ESP_ERROR_CHECK(
        mdns_service_add("ESP32-WebServer", "_http", "_tcp", 80, serviceTxtData,
                         sizeof(serviceTxtData) / sizeof(serviceTxtData[0])));
}

static void event_handler(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    if (event_base == SC_EVENT) {
        switch (event_id) {
            case SC_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "Scan done");
                break;
            case SC_EVENT_FOUND_CHANNEL:
                ESP_LOGI(TAG, "Found channel");
                break;
            case SC_EVENT_GOT_SSID_PSWD: {
                ESP_LOGI(TAG, "Got SSID and password");

                smartconfig_event_got_ssid_pswd_t* evt =
                    (smartconfig_event_got_ssid_pswd_t*)event_data;
                wifi_config_t wifi_config;
                uint8_t       ssid[33]     = {0};
                uint8_t       password[65] = {0};

                bzero(&wifi_config, sizeof(wifi_config_t));
                memcpy(wifi_config.sta.ssid, evt->ssid,
                       sizeof(wifi_config.sta.ssid));
                memcpy(wifi_config.sta.password, evt->password,
                       sizeof(wifi_config.sta.password));
                wifi_config.sta.bssid_set = evt->bssid_set;
                if (wifi_config.sta.bssid_set == true) {
                    memcpy(wifi_config.sta.bssid, evt->bssid,
                           sizeof(wifi_config.sta.bssid));
                }

                memcpy(ssid, evt->ssid, sizeof(evt->ssid));
                memcpy(password, evt->password, sizeof(evt->password));
                ESP_LOGI(TAG, "SSID:     %s", ssid);
                ESP_LOGI(TAG, "PASSWORD: %s", password);
                if (evt->type == SC_TYPE_ESPTOUCH_V2) {
                    esp_smartconfig_get_rvd_data((uint8_t*)mqtt_server_uri,
                                                 sizeof(mqtt_server_uri));
                    ESP_LOGI(TAG, "MQTT SERVER: %s", mqtt_server_uri);
                }

                esp_wifi_disconnect();
                esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
                esp_wifi_connect();
                break;
            }
            case SC_EVENT_SEND_ACK_DONE:
                xEventGroupSetBits(event_group, ESPTOUCH_DONE_BIT);
                break;
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                xEventGroupClearBits(event_group, WIFI_CONNECTED_BIT);
                esp_wifi_connect();
                ESP_LOGI(TAG, "retry to connect to the AP");
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        device_ipv4 = event->ip_info.ip;
        ESP_LOGI(TAG, "WiFi connected!");
    }
}
esp_err_t init_fs(void) {
    esp_vfs_spiffs_conf_t conf = {
        .base_path              = CONFIG_EXAMPLE_WEB_MOUNT_POINT,
        .partition_label        = NULL,
        .max_files              = 5,
        .format_if_mount_failed = false,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)",
                     esp_err_to_name(ret));
        }
        return ESP_FAIL;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(NULL, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)",
                 esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ESP_OK;
}

void app_main(void) {
    event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    initialise_mdns();
    netbiosns_init();
    netbiosns_set_name(CONFIG_EXAMPLE_MDNS_HOST_NAME);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_register(SC_EVENT, ESP_EVENT_ANY_ID,
                                        &event_handler, NULL, NULL);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &event_handler, NULL, NULL);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        &event_handler, NULL, NULL);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_wifi_clear_config();
    bool is_provisioned = false;
    /* Check WIFI config in NVS */
    esp_wifi_is_provisioned(&is_provisioned);
    if (is_provisioned) {
        ESP_LOGI(TAG, "WIFI already provisioned");
        esp_wifi_connect();
    } else {
        ESP_LOGI(TAG, "Smartconfig start");
        esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2);
        esp_esptouch_set_timeout(15);  // 15 seconds timeout
        smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
        esp_smartconfig_start(&cfg);
        xEventGroupWaitBits(event_group, ESPTOUCH_DONE_BIT, true, false,
                            portMAX_DELAY);
        esp_smartconfig_stop();
        ESP_LOGI(TAG, "Smartconfig stop");
    }

    xEventGroupSetBits(event_group, WIFI_PROVISIONED_BIT);
    xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT, false, false,
                        portMAX_DELAY);

    ESP_ERROR_CHECK(init_fs());
    ESP_ERROR_CHECK(start_rest_server(CONFIG_EXAMPLE_WEB_MOUNT_POINT));
}
