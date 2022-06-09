#define USE_ZNP_TASK
#define USE_WIFI_TASK
#define USE_LCD_TASK
#define USE_RTC_TASK

#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "dev_timeslot.h"
#include "ds1307.h"
#include "esp_log.h"
#include "esp_smartconfig.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "lcd5110.h"
#include "mq135.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "wifi_app.h"
#include "zb_data.h"
#include "zb_znp.h"

/* SPI multi-slave pins */
#define ESP_SPI_MISO_PIN              18
#define ESP_SPI_MOSI_PIN              19
#define ESP_SPI_SLCK_PIN              21

/* Define pins for SZ1V5 CC2530 Zigbee module */
#define ZNP_RST_PIN                   26
#define ZNP_TX_PIN                    27
#define ZNP_RX_PIN                    14

/* Define pins for DS1307 Tiny RTC module */
#define RTC_SQ_PIN                    25
#define RTC_SCL_PIN                   33
#define RTC_SDA_PIN                   32

/* Define pins for Nokia LCD5110 module */
#define LCD_RST_PIN                   2
#define LCD_CE_PIN                    4
#define LCD_DC_PIN                    16
#define LCD_DIN_PIN                   ESP_SPI_MOSI_PIN
#define LCD_CLK_PIN                   ESP_SPI_SLCK_PIN

/* LCD screen settings */
#define LCD_REFRESH_RATE_HZ           5
#define LCD_SCREEN_CHANGE_INTERVAL_US 5 * 1000 * 1000

/* Custom user defines */
#define MQTT_QOS_LEVEL                2
#define USE_ESPTOUCH_V2
// #define CLEAR_WIFI_CONFIG_ON_RESET
// #define CLEAR_ZB_DEV_LIST_ON_RESET
// #define PRINT_ZNP_ATTRIBUTES
#define PRINT_TELEMETRY_DATA
#define USE_DEFAULT_MQTT_BROKER
// #define REBOOT_ON_RESET

/* Global variables for tasks */
static const char*              TAG = "smarthome_app";
static time_t                   now;
static struct tm                timeinfo;
static char                     strftime_buf[64];
static esp_ip4_addr_t           device_ipv4;
static char                     mqtt_server_uri[20];
static esp_mqtt_client_handle_t client;
static uint16_t                 node_button_addr = 0;
static uint16_t                 node_relay_addr  = 0;

/* Event group */
EventGroupHandle_t event_group;
#define WIFI_CONNECTED_BIT       BIT1
#define WIFI_PROVISIONED_BIT     BIT2
#define ESPTOUCH_DONE_BIT        BIT3
#define MQTT_CONNECTED_BIT       BIT4
#define RTC_DS1307_CONNECTED_BIT BIT5
#define ZNP_COORD_STARTED_BIT    BIT6

#ifdef USE_ZNP_TASK

static bool check_wday(weekday_bits_t weekday) {
    struct tm temp_time;
    localtime_r(&now, &temp_time);
    if (weekday.monday && temp_time.tm_wday == 1) return true;
    if (weekday.tuesday && temp_time.tm_wday == 2) return true;
    if (weekday.wednesday && temp_time.tm_wday == 3) return true;
    if (weekday.thursday && temp_time.tm_wday == 4) return true;
    if (weekday.friday && temp_time.tm_wday == 5) return true;
    if (weekday.saturday && temp_time.tm_wday == 6) return true;
    if (weekday.sunday && temp_time.tm_wday == 0) return true;
    return false;
}

static void dev_check_timeslot(void) {
    struct tm temp_time;
    localtime_r(&now, &temp_time);

    zb_dev_timeslot_t* temp = get_timeslot_list();
    while (temp != NULL) {
        zb_dev_t* dev = zb_get_device_with_ieee_addr(temp->ieee_addr);
        if (dev != NULL) {
            if (temp->timeslot.time_on.hours +
                    (temp->timeslot.time_on.am_pm ? 12 : 0) !=
                temp_time.tm_hour)
                goto check_time_off;
            if (temp->timeslot.time_on.minutes != temp_time.tm_min)
                goto check_time_off;
            if (temp->timeslot.time_on.seconds != temp_time.tm_sec)
                goto check_time_off;
            if (!check_wday(temp->timeslot.weekday)) goto check_time_off;

            af_data_request_t control_data = {
                .DstAddr      = dev->short_addr,
                .DestEndpoint = ENDPOINT_COMMAND,
                .SrcEndpoint  = ENDPOINT_COMMAND,
                .ClusterID    = 0x0000,
                .TransID      = 0,
                .Options      = 0,
                .Radius       = 0x0F,
            };

            uint8_t ctrl_data[2] = {0, 0};

            if (temp->cluster_id == CLUSTER_ID_RELAY_CTRL) {
                if (temp->timeslot.channel_1) ctrl_data[1] |= 0x21;
                if (temp->timeslot.channel_2) ctrl_data[1] |= 0x42;
                if (temp->timeslot.channel_3) ctrl_data[1] |= 0x84;
            } else if (temp->cluster_id == CLUSTER_ID_COMPTEUR) {
                ctrl_data[0] = 1;
            } else
                goto check_time_off;

            control_data.Data = ctrl_data;
            control_data.Len  = sizeof(ctrl_data);
            znp_af_data_request(control_data);

        check_time_off:
            if (temp->timeslot.time_off.hours +
                    (temp->timeslot.time_off.am_pm ? 12 : 0) !=
                temp_time.tm_hour)
                goto check_next_timeslot;
            if (temp->timeslot.time_off.minutes != temp_time.tm_min)
                goto check_next_timeslot;
            if (temp->timeslot.time_off.seconds != temp_time.tm_sec)
                goto check_next_timeslot;
            if (!check_wday(temp->timeslot.weekday)) goto check_next_timeslot;

            ctrl_data[1] = 0;
            if (temp->cluster_id == CLUSTER_ID_RELAY_CTRL) {
                if (temp->timeslot.channel_1) ctrl_data[1] |= 0x20;
                if (temp->timeslot.channel_2) ctrl_data[1] |= 0x40;
                if (temp->timeslot.channel_3) ctrl_data[1] |= 0x80;
            } else if (temp->cluster_id == CLUSTER_ID_COMPTEUR) {
                ctrl_data[0] = 0;
            } else
                goto check_time_off;

            control_data.Data = ctrl_data;
            control_data.Len  = sizeof(ctrl_data);
            znp_af_data_request(control_data);
        }
    check_next_timeslot:
        temp = temp->next;
    }
}

static void znp_check_alive(void) {
    zb_dev_t* temp = zb_get_device_list();
    while (temp != NULL) {
        if (temp->cluster_id != UNKNOWN_CLUSTER_ID) {
            if (temp->is_alive &&
                difftime(now, temp->last_seen) >= ZB_KEEPALIVE_TIMEOUT) {
                temp->is_alive = false;
                ESP_LOGI(TAG, "0x%016llX has disconnected. Last seen: %s",
                         temp->ieee_addr, ctime(&temp->last_seen));
            }
        }
        temp = temp->next;
    }
}

static void znp_link_relay_button_task(void* arg) {
    union {
        uint8_t raw[3];
        struct {
            uint8_t command_option;
            uint8_t lsb_addr;
            uint8_t msb_addr;
        };
    } link_data;

    link_data.command_option = 1;

    af_data_request_t link_msg = {
        .DestEndpoint = ENDPOINT_COMMAND,
        .SrcEndpoint  = ENDPOINT_COMMAND,
        .ClusterID    = 0x0000,
        .TransID      = 0,
        .Options      = 0,
        .Radius       = 0x0F,
        .Len          = 3,
    };

    link_data.lsb_addr = LSB(node_relay_addr);
    link_data.msb_addr = MSB(node_relay_addr);
    link_msg.DstAddr   = node_button_addr;
    link_msg.Data      = link_data.raw;
    znp_af_data_request(link_msg);

    link_data.lsb_addr = LSB(node_button_addr);
    link_data.msb_addr = MSB(node_button_addr);
    link_msg.DstAddr   = node_relay_addr;
    link_msg.Data      = link_data.raw;
    znp_af_data_request(link_msg);

    printf("Relay-Button link message sent\n");

    vTaskDelete(NULL);
}

static void znp_af_incoming_msg_handler(af_incoming_msg_t af_incoming_msg) {
    zb_dev_t* dev = zb_get_device_with_short_addr(af_incoming_msg.SrcAddr);

    if (dev == NULL) return;

    zb_update_device_last_seen(dev->ieee_addr);

    if (af_incoming_msg.SrcEndpoint == ENDPOINT_PROVISION) {
        switch (af_incoming_msg.ClusterID) {
            case CLUSTER_ID_RELAY_CTRL:
            case CLUSTER_ID_SENSOR:
            case CLUSTER_ID_COMPTEUR: {
                char* provision_msg = zb_get_device_provision_msg(dev);
                esp_mqtt_client_publish(client, MQTT_PUB_TOPIC, provision_msg,
                                        strlen(provision_msg), MQTT_QOS_LEVEL,
                                        0);
                free(provision_msg);
                provision_msg = NULL;
            }
                __attribute__((fallthrough));
            case CLUSTER_ID_BUTTON:
                zb_set_device_cluster_id(dev->ieee_addr,
                                         af_incoming_msg.ClusterID);
                break;

            default:
                znp_zdo_mgmt_leave_req(dev->short_addr, dev->ieee_addr, 0x02);
                zb_remove_device(dev->ieee_addr);
                break;
        }

        if (dev->cluster_id == CLUSTER_ID_RELAY_CTRL)
            node_relay_addr = dev->short_addr;

        if (dev->cluster_id == CLUSTER_ID_BUTTON)
            node_button_addr = dev->short_addr;

        if (node_relay_addr != 0 && node_button_addr != 0)
            xTaskCreate(znp_link_relay_button_task, "link_relay_button",
                        1024 * 2, NULL, 10, NULL);

    } else if (af_incoming_msg.SrcEndpoint == ENDPOINT_TELEMETRY) {
        switch (af_incoming_msg.ClusterID) {
            case CLUSTER_ID_RELAY_CTRL: {
                zb_dev_data_t relay_data =
                    *(zb_dev_data_t*)af_incoming_msg.Data;
#ifdef PRINT_TELEMETRY_DATA
                ESP_LOGI(TAG, "relay_channel_num: %u",
                         relay_data.relay_channel_num);
                for (uint8_t i = 0; i < relay_data.relay_channel_num; i++) {
                    ESP_LOGI(
                        TAG, "relay[%u]: %s", i + 1,
                        (relay_data.relay_bit_mask & (1 << i)) ? "on" : "off");
                }
#endif  // PRINT_TELEMETRY_DATA
                zb_update_device_data(dev->ieee_addr, af_incoming_msg.ClusterID,
                                      &relay_data);
            } break;

            case CLUSTER_ID_BUTTON: {
                zb_dev_data_t button_data =
                    *(zb_dev_data_t*)af_incoming_msg.Data;
#ifdef PRINT_TELEMETRY_DATA
                ESP_LOGI(TAG, "button_channel_num: %u",
                         button_data.relay_channel_num);
                for (uint8_t i = 0; i < button_data.relay_channel_num; i++) {
                    ESP_LOGI(
                        TAG, "button[%u]: %s", i + 1,
                        (button_data.relay_bit_mask & (1 << i)) ? "on" : "off");
                }
#endif  // PRINT_TELEMETRY_DATA
            } break;

            case CLUSTER_ID_SENSOR: {
                zb_sensor_t   zb_sensor   = *(zb_sensor_t*)af_incoming_msg.Data;
                zb_dev_data_t sensor_data = {
                    .temperature = zb_sensor.T1 + zb_sensor.T2 / 10.0,
                    .humidity    = zb_sensor.RH1 + zb_sensor.RH2 / 10.0,
                };
                sensor_data.air_quality = mq135_calc_corrected_ppm(
                    zb_sensor.ppm_adc, sensor_data.temperature,
                    sensor_data.humidity);
#ifdef PRINT_TELEMETRY_DATA
                ESP_LOGI(TAG, "Temperature: %.2f", sensor_data.temperature);
                ESP_LOGI(TAG, "Humidity: %.2f", sensor_data.humidity);
                ESP_LOGI(TAG, "Air Quality: %f", sensor_data.air_quality);
#endif  // PRINT_TELEMETRY_DATA
                zb_update_device_data(dev->ieee_addr, af_incoming_msg.ClusterID,
                                      &sensor_data);

            } break;

            case CLUSTER_ID_COMPTEUR: {
                zb_dev_data_t compteur_data =
                    *(zb_dev_data_t*)af_incoming_msg.Data;

                compteur_data.power /= 1.316340539;

#ifdef PRINT_TELEMETRY_DATA
                ESP_LOGI(TAG, "Irms: %.2f", compteur_data.irms);
                ESP_LOGI(TAG, "Vrms: %.2f", compteur_data.vrms);
                ESP_LOGI(TAG, "Active Power: %.2f", compteur_data.power);
                ESP_LOGI(TAG, "Relay State: %s",
                         compteur_data.relay ? "on" : "off");
#endif  // PRINT_TELEMETRY_DATA
                zb_update_device_data(dev->ieee_addr, af_incoming_msg.ClusterID,
                                      &compteur_data);

            } break;

            default:
                break;
        }
    }
}

/* ZNP message callback */
void znp_message_callback(uint16_t cmd, uint8_t* data, uint8_t len) {
    switch (cmd) {
        case AF_DATA_CONFIRM: {
            ESP_LOGI(TAG, "AF_DATA_CONFIRM");
#ifdef PRINT_ZNP_ATTRIBUTES
            ESP_LOGI(TAG, "Status: " Z_CODE_NAME_STR, ZSTATUS2STR(data[0]));
            ESP_LOGI(TAG, "Endpoint: 0x%02X", data[1]);
            ESP_LOGI(TAG, "TransID: 0x%02X", data[2]);
#endif  // PRINT_ZNP_ATTRIBUTES
        } break;

        case AF_INCOMING_MSG: {
            ESP_LOGI(TAG, "AF_INCOMING_MSG");
            af_incoming_msg_t af_incoming_msg = *(af_incoming_msg_t*)data;
#ifdef PRINT_ZNP_ATTRIBUTES
            ESP_LOGI(TAG, "GroupID: 0x%04X", af_incoming_msg.GroupID);
            ESP_LOGI(TAG, "ClusterID: 0x%04X", af_incoming_msg.ClusterID);
            ESP_LOGI(TAG, "SrcAddr: 0x%04X", af_incoming_msg.SrcAddr);
            ESP_LOGI(TAG, "SrcEndpoint: 0x%02X", af_incoming_msg.SrcEndpoint);
            ESP_LOGI(TAG, "DestEndpoint: 0x%02X", af_incoming_msg.DestEndpoint);
            ESP_LOGI(TAG, "WasBroadcast: 0x%02X", af_incoming_msg.WasBroadcast);
            ESP_LOGI(TAG, "LinkQuality: 0x%02X", af_incoming_msg.LinkQuality);
            ESP_LOGI(TAG, "SecurityUse: 0x%02X", af_incoming_msg.SecurityUse);
            ESP_LOGI(TAG, "Timestamp: 0x%08X", af_incoming_msg.Timestamp);
            ESP_LOGI(TAG, "TransSeqNumber: 0x%02X",
                     af_incoming_msg.TransSeqNumber);
            ESP_LOGI(TAG, "Len: 0x%02X", af_incoming_msg.Len);
            if (af_incoming_msg.Len > 0) {
                ESP_LOGI(TAG, "Data:");
                printf(LOG_COLOR_I "(HEX) " LOG_RESET_COLOR);
                for (uint8_t i = 0; i < af_incoming_msg.Len; i++)
                    printf(LOG_COLOR_I "%02X " LOG_RESET_COLOR,
                           af_incoming_msg.Data[i]);
                printf("\n");
                printf(LOG_COLOR_I "(DEC) " LOG_RESET_COLOR);
                for (uint8_t i = 0; i < af_incoming_msg.Len; i++)
                    printf(LOG_COLOR_I "%d " LOG_RESET_COLOR,
                           af_incoming_msg.Data[i]);
                printf("\n");
            } else
                ESP_LOGI(TAG, "Data: (empty)");
#endif  // PRINT_ZNP_ATTRIBUTES
            znp_af_incoming_msg_handler(af_incoming_msg);
        } break;

        case ZDO_TC_DEV_IND: {
            ESP_LOGI(TAG, "ZDO_TC_DEV_IND");
            zdo_tc_dev_ind_t zdo_tc_dev_ind = *(zdo_tc_dev_ind_t*)data;
#ifdef PRINT_ZNP_ATTRIBUTES
            ESP_LOGI(TAG, "SrcNwkAddr: 0x%04X", zdo_tc_dev_ind.SrcNwkAddr);
            ESP_LOGI(TAG, "SrcIEEEAdrr: 0x%016llX", zdo_tc_dev_ind.SrcIEEEAddr);
            ESP_LOGI(TAG, "ParentNwkAddr: 0x%04X",
                     zdo_tc_dev_ind.ParentNwkAddr);
#endif  // PRINT_ZNP_ATTRIBUTES
            zb_add_device(zdo_tc_dev_ind.SrcNwkAddr,
                          zdo_tc_dev_ind.SrcIEEEAddr);
        } break;

        case ZDO_MGMT_LEAVE_RSP:
            ESP_LOGI(TAG, "ZDO_MGMT_LEAVE_RSP");
#ifdef PRINT_ZNP_ATTRIBUTES
            ESP_LOGI(TAG, "SrcAddr: 0x%02X%02X", data[1], data[0]);
            ESP_LOGI(TAG, "Status: " Z_CODE_NAME_STR, ZSTATUS2STR(data[2]));
#endif  // PRINT_ZNP_ATTRIBUTES
            break;

        default:
            break;
    }
}

static void znp_task(void* arg) {
    zb_device_init();

#ifdef CLEAR_ZB_DEV_LIST_ON_RESET
    zb_remove_all_device();
#endif  // CLEAR_ZB_DEV_LIST_ON_RESET

    znp_device_config_t znp_dev_cfg = {
        .baud_rate  = 19200,
        .tx_io_num  = ZNP_TX_PIN,
        .rx_io_num  = ZNP_RX_PIN,
        .rst_io_num = ZNP_RST_PIN,
        .znp_msg_cb = znp_message_callback,
    };
    znp_init(&znp_dev_cfg);
    znp_start_coordinator(STARTOPT_CLEAR_NONE);
    znp_af_register(ENDPOINT_PROVISION);
    znp_af_register(ENDPOINT_TELEMETRY);
    znp_af_register(ENDPOINT_COMMAND);
    znp_af_register(ENDPOINT_KEEPALIVE);
    xEventGroupSetBits(event_group, ZNP_COORD_STARTED_BIT);

    zb_print_dev_list();
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        time(&now);
        znp_check_alive();
        dev_check_timeslot();
        vTaskDelayUntil(&xLastWakeTime, 500 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}
#endif  // USE_ZNP_TASK

#ifdef USE_WIFI_TASK
static void log_error_if_nonzero(const char* message, int error_code) {
    if (!error_code) ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
}

static void mqtt_data_handler(const char* topic, const char* payload) {
    printf("TOPIC = %s\n", topic);
    printf("DATA = %s\n", payload);

    cJSON* json_payload = cJSON_Parse(payload);
    cJSON* action       = cJSON_GetObjectItem(json_payload, "action");
    if (cJSON_IsString(action) &&
        (xEventGroupGetBits(event_group) & ZNP_COORD_STARTED_BIT)) {
        /* Handle action command */
        if (!strcmp(action->valuestring, "command")) {
            cJSON* command = cJSON_GetObjectItem(json_payload, "command");
            if (cJSON_IsString(command)) {
                /* Handle command permit join */
                if (!strcmp(command->valuestring, "permit_join")) {
                    znp_permit_join(ALL_ROUTERS_AND_COORDINATOR);
                }

                /* Handle command leave request */
                else if (!strcmp(command->valuestring, "leave_req")) {
                    cJSON* dev_addr =
                        cJSON_GetObjectItem(json_payload, "dev_addr");
                    if (cJSON_IsString(dev_addr)) {
                        uint64_t d_addr =
                            strtoull(dev_addr->valuestring, NULL, 16);

                        zb_dev_t* dev = zb_get_device_with_ieee_addr(d_addr);
                        if (dev != NULL) {
                            ESP_LOGI(TAG, "0x%016llX will be removed", d_addr);
                            znp_zdo_mgmt_leave_req(dev->short_addr,
                                                   dev->ieee_addr, 0x02);
                            zb_remove_device(d_addr);
                        }
                    }
                }

                /* Handle command reboot */
                else if (!strcmp(command->valuestring, "reboot")) {
                    nvs_flash_erase();
                    esp_restart();
                }
            }

            /* Handle action control */
        } else if (!strcmp(action->valuestring, "control")) {
            cJSON* dev_addr = cJSON_GetObjectItem(json_payload, "dev_addr");
            if (cJSON_IsString(dev_addr)) {
                uint64_t d_addr = strtoull(dev_addr->valuestring, NULL, 16);
                ESP_LOGI(TAG, "Control device 0x%016llX", d_addr);

                zb_dev_t* dev = zb_get_device_with_ieee_addr(d_addr);
                if (dev != NULL) {
                    timeslot_t timeslot;
                    if (json_parse_timeslot(json_payload, &timeslot) ==
                        ESP_OK) {
                        if (dev->cluster_id == CLUSTER_ID_RELAY_CTRL) {
                            cJSON* channel1 =
                                cJSON_GetObjectItem(json_payload, "channel1");
                            cJSON* channel2 =
                                cJSON_GetObjectItem(json_payload, "channel2");
                            cJSON* channel3 =
                                cJSON_GetObjectItem(json_payload, "channel3");

                            if (cJSON_IsBool(channel1) &&
                                cJSON_IsBool(channel2) &&
                                cJSON_IsBool(channel3)) {
                                timeslot.channel_1 = cJSON_IsTrue(channel1);
                                timeslot.channel_2 = cJSON_IsTrue(channel2);
                                timeslot.channel_3 = cJSON_IsTrue(channel3);
                            } else
                                goto end_json_handler;
                        }
                        set_timeslot_to_dev(dev->ieee_addr, dev->cluster_id,
                                            &timeslot);
                        print_timeslot_list();
                    } else {
                        af_data_request_t control_data = {
                            .DstAddr      = dev->short_addr,
                            .DestEndpoint = ENDPOINT_COMMAND,
                            .SrcEndpoint  = ENDPOINT_COMMAND,
                            .ClusterID    = 0x0000,
                            .TransID      = 0,
                            .Options      = 0,
                            .Radius       = 0x0F,
                        };

                        switch (dev->cluster_id) {
                            case CLUSTER_ID_RELAY_CTRL: {
                                cJSON* relay1 = cJSON_GetObjectItem(
                                    json_payload, "status1");
                                cJSON* relay2 = cJSON_GetObjectItem(
                                    json_payload, "status2");
                                cJSON* relay3 = cJSON_GetObjectItem(
                                    json_payload, "status3");

                                if (!cJSON_IsString(relay1) &&
                                    !cJSON_IsString(relay2) &&
                                    !cJSON_IsString(relay3))
                                    goto end_json_handler;

                                union {
                                    uint8_t raw[2];
                                    struct {
                                        uint8_t command_option;
                                        struct {
                                            // Bit state
                                            uint8_t relay1      : 1;
                                            uint8_t relay2      : 1;
                                            uint8_t relay3      : 1;
                                            uint8_t             : 2;
                                            uint8_t relay_mask1 : 1;
                                            uint8_t relay_mask2 : 1;
                                            uint8_t relay_mask3 : 1;
                                        };
                                    };
                                } relay_ctrl;

                                if (cJSON_IsString(relay1)) {
                                    relay_ctrl.relay_mask1 = true;
                                    relay_ctrl.relay1 =
                                        (strcmp(relay1->valuestring, "ON") ==
                                         0);
                                }

                                if (cJSON_IsString(relay2)) {
                                    relay_ctrl.relay_mask2 = true;
                                    relay_ctrl.relay2 =
                                        (strcmp(relay2->valuestring, "ON") ==
                                         0);
                                }

                                if (cJSON_IsString(relay3)) {
                                    relay_ctrl.relay_mask3 = true;
                                    relay_ctrl.relay3 =
                                        (strcmp(relay3->valuestring, "ON") ==
                                         0);
                                }

                                /* @Note1: Message will be wrong without delay.
                                 * Reason: Unknown */
                                vTaskDelay(1);

                                relay_ctrl.command_option = 0;

                                control_data.Data = relay_ctrl.raw;
                                control_data.Len  = sizeof(relay_ctrl.raw);
                            } break;

                            case CLUSTER_ID_COMPTEUR: {
                                cJSON* relay =
                                    cJSON_GetObjectItem(json_payload, "status");

                                uint8_t relay_ade = 0;

                                if (cJSON_IsString(relay)) {
                                    if (strcmp(relay->valuestring, "ON") == 0)
                                        relay_ade = 1;
                                } else
                                    goto end_json_handler;

                                /* See @Note1 */
                                vTaskDelay(1);

                                control_data.Data = &relay_ade;
                                control_data.Len  = sizeof(relay_ade);
                            } break;

                            default:
                                break;
                        }
                        znp_af_data_request(control_data);
                    }
                }
            }
        }
    }

end_json_handler:
    cJSON_Delete(json_payload);
}

static void mqtt_event_handler(void* handler_args, esp_event_base_t base,
                               int32_t event_id, void* event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            esp_mqtt_client_subscribe(client, MQTT_SUB_TOPIC, 2);
            xEventGroupSetBits(event_group, MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(event_group, MQTT_CONNECTED_BIT);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            char* topic = malloc(event->topic_len + 1);
            char* data  = malloc(event->data_len + 1);

            sprintf(topic, "%.*s", event->topic_len, event->topic);
            sprintf(data, "%.*s", event->data_len, event->data);

            mqtt_data_handler(topic, data);
            free(topic);
            free(data);

            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            if (event->error_handle->error_type ==
                MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                log_error_if_nonzero("reported from esp-tls",
                                     event->error_handle->esp_tls_last_esp_err);
                log_error_if_nonzero("reported from tls stack",
                                     event->error_handle->esp_tls_stack_err);
                log_error_if_nonzero(
                    "captured as transport's socket errno",
                    event->error_handle->esp_transport_sock_errno);
                ESP_LOGI(
                    TAG, "Last errno string (%s)",
                    strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;

        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
            break;

        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
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

#ifndef USE_DEFAULT_MQTT_BROKER
                if (evt->type == SC_TYPE_ESPTOUCH_V2) {
                    esp_smartconfig_get_rvd_data((uint8_t*)mqtt_server_uri,
                                                 sizeof(mqtt_server_uri));
                    ESP_LOGI(TAG, "MQTT SERVER: %s", mqtt_server_uri);
                    if (mqtt_store_uri(mqtt_server_uri) == ESP_FAIL)
                        ESP_LOGW(TAG, "Invalid MQTT server uri");
                }
#endif  // USE_DEFAULT_MQTT_BROKER
                esp_wifi_disconnect();
                esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
                esp_wifi_connect();
            } break;

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
    }
}

static void publish_new_data(void) {
    zb_dev_t* dev = zb_get_device_list();

    while (dev != NULL) {
        if (dev->new_data_updated && dev->cluster_id != UNKNOWN_CLUSTER_ID) {
            char* telemetry_msg = zb_get_device_telemetry_msg(dev);
            esp_mqtt_client_publish(client, MQTT_PUB_TOPIC, telemetry_msg,
                                    strlen(telemetry_msg), MQTT_QOS_LEVEL, 0);
            free(telemetry_msg);
            telemetry_msg = NULL;

            dev->new_data_updated = false;
        }

        dev = dev->next;
    }
}

static void wifi_task(void* arg) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

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

    /* Clear wifi config on reset */
#ifdef CLEAR_WIFI_CONFIG_ON_RESET
    esp_wifi_clear_config();
    mqtt_clear_uri();
#endif  // CLEAR_WIFI_CONFIG_ON_RESET

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    bool is_provisioned = false;
    /* Check WIFI config and MQTT  in NVS */
    esp_wifi_is_provisioned(&is_provisioned);
    if (is_provisioned) {
#ifndef USE_DEFAULT_MQTT_BROKER
        if ((mqtt_get_uri(mqtt_server_uri) == ESP_OK)) {
            ESP_LOGI(TAG, "Already provisioned");
            esp_wifi_connect();
        }
#else
        ESP_LOGI(TAG, "Already provisioned");
        esp_wifi_connect();
#endif  // USE_DEFAULT_MQTT_BROKER
    } else {
        ESP_LOGI(TAG, "Smartconfig start");

#ifdef USE_ESPTOUCH_V2
        esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2);
#else
        esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
#endif  // USE_ESPTOUCH_V2

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

    char mqtt_borker[50];

#ifndef USE_DEFAULT_MQTT_BROKER
    if (mqtt_get_uri(mqtt_server_uri) != ESP_OK)
        snprintf(mqtt_server_uri, sizeof(mqtt_server_uri),
                 MQTT_BROKER_URL_DEFAULT);
#else
    snprintf(mqtt_server_uri, sizeof(mqtt_server_uri), MQTT_BROKER_URL_DEFAULT);
#endif  // USE_DEFAULT_MQTT_BROKER

    snprintf(mqtt_borker, sizeof(mqtt_borker), "mqtt://%s", mqtt_server_uri);

    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = mqtt_borker,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler,
                                   NULL);
    esp_mqtt_client_start(client);

    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        if (xEventGroupGetBits(event_group) & MQTT_CONNECTED_BIT)
            publish_new_data();
        vTaskDelayUntil(&xLastWakeTime, 1000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}
#endif  // USE_WIFI_TASK

#ifdef USE_LCD_TASK
enum {
    LCD_SCREEN_WIFI_MQTT_INFO,
    LCD_SCREEN_DATE_TIME,
    LCD_SCREEN_MAX
} lcd_screen = LCD_SCREEN_WIFI_MQTT_INFO;

static void lcd_screen_change_callback(void* arg) {
    if (++lcd_screen == LCD_SCREEN_MAX) lcd_screen = LCD_SCREEN_WIFI_MQTT_INFO;
}

static void lcd_task(void* arg) {
    esp_timer_handle_t lcd_screen_timer;

    const esp_timer_create_args_t lcd_screen_timer_args = {
        .callback = &lcd_screen_change_callback,
    };
    esp_timer_create(&lcd_screen_timer_args, &lcd_screen_timer);

    lcd5110_config_t lcd_config = {
        .contrast   = LCD5110_DEFAULT_CONTRAST,
        .rst_io_num = LCD_RST_PIN,
        .ce_io_num  = LCD_CE_PIN,
        .dc_io_num  = LCD_DC_PIN,
        .din_io_num = LCD_DIN_PIN,
        .clk_io_num = LCD_CLK_PIN,
    };
    lcd5110_init(&lcd_config);

    lcd5110_display_BK_logo(false);
    xEventGroupWaitBits(event_group, ZNP_COORD_STARTED_BIT, false, false,
                        portMAX_DELAY);

    esp_timer_start_periodic(lcd_screen_timer, LCD_SCREEN_CHANGE_INTERVAL_US);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        lcd5110_clear();
        EventBits_t uxBits = xEventGroupGetBits(event_group);

        switch (lcd_screen) {
            case LCD_SCREEN_WIFI_MQTT_INFO: {
                lcd5110_goto_xy(10, 0);
                lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_5x7,
                             "WiFi Info");
                if (uxBits & WIFI_CONNECTED_BIT) {
                    wifi_config_t wifi_cfg;
                    esp_wifi_get_config(WIFI_IF_STA, &wifi_cfg);

                    lcd5110_goto_xy(0, 12);
                    lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_3x5,
                                 "SSID: %.*s",
                                 strnlen((const char*)wifi_cfg.sta.ssid,
                                         sizeof(wifi_cfg.sta.ssid)),
                                 (const char*)wifi_cfg.sta.ssid);

                    lcd5110_goto_xy(0, 24);
                    lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_3x5,
                                 "IP: " IPSTR, IP2STR(&device_ipv4));

                    lcd5110_goto_xy(0, 36);
                    lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_3x5,
                                 "BROKER:");

                    lcd5110_goto_xy(4, 42);
                    lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_3x5, "%s",
                                 (uxBits & MQTT_CONNECTED_BIT)
                                     ? mqtt_server_uri
                                     : "Disconnected");

                } else {
                    lcd5110_goto_xy(0, 12);
                    lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_5x7,
                                 "WiFi Disconnected");
                    if (!(uxBits & WIFI_PROVISIONED_BIT)) {
                        lcd5110_goto_xy(22, 30);
                        lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_5x7,
                                     "Running");
                        lcd5110_goto_xy(10, 40);
                        lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_5x7,
                                     "Smartconfig");
                    }
                }
                break;
            }

            case LCD_SCREEN_DATE_TIME:
                lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_5x7,
                             "Date & Time");
                lcd5110_goto_xy(0, 12);

                time(&now);
                localtime_r(&now, &timeinfo);
                strftime(strftime_buf, sizeof(strftime_buf), DATE_STR_FORMAT,
                         &timeinfo);
                lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_5x7,
                             strftime_buf);

                lcd5110_goto_xy(0, 24);
                strftime(strftime_buf, sizeof(strftime_buf), TIME_STR_FORMAT,
                         &timeinfo);
                lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_5x7,
                             strftime_buf);

                if (!(uxBits & RTC_DS1307_CONNECTED_BIT)) {
                    lcd5110_goto_xy(48, 42);
                    lcd5110_puts(LCD5110_PIXEL_SET, LCD5110_FONT_SIZE_3x5,
                                 "RTC Error");
                }
                break;

            default:
                break;
        }

        lcd5110_refresh();
        vTaskDelayUntil(&xLastWakeTime,
                        (1000 / LCD_REFRESH_RATE_HZ) / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}
#endif  // USE_LCD_TASK

#ifdef USE_RTC_TASK
void time_sync_notification_cb(struct timeval* tv) {
    localtime_r(&tv->tv_sec, &timeinfo);
    ds1307_set_datetime(&timeinfo);
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

void ds1307_sq_handle(void) {
    if (xEventGroupGetBits(event_group) & RTC_DS1307_CONNECTED_BIT) {
        if (ds1307_get_datetime(&timeinfo) == ESP_OK) {
            struct timeval tv = {
                .tv_sec  = mktime(&timeinfo),
                .tv_usec = 0,
            };
            settimeofday(&tv, NULL);
        } else {
            xEventGroupClearBits(event_group, RTC_DS1307_CONNECTED_BIT);
        }
    }
}

static void rtc_task(void* arg) {
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_servermode_dhcp(1);

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "vn.pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);
    sntp_init();

    /* Set timezone GMT+7 */
    setenv("TZ", "CST-7", 1);
    tzset();

    ds1307_config_t ds_cfg = {
        .scl_io_num        = RTC_SCL_PIN,
        .sda_io_num        = RTC_SDA_PIN,
        .sq_io_num         = RTC_SQ_PIN,
        .sq_output_freq_cb = ds1307_sq_handle,
    };
    ds1307_init(&ds_cfg);
    xEventGroupSetBits(event_group, RTC_DS1307_CONNECTED_BIT);
    ds1307_set_output_freq(DS1307_OUTPUT_FREQ_1HZ);

    xEventGroupWaitBits(event_group, ZNP_COORD_STARTED_BIT, false, false,
                        portMAX_DELAY);

    while (1) {
        if (ds1307_check() == ESP_OK)
            xEventGroupSetBits(event_group, RTC_DS1307_CONNECTED_BIT);
        else
            xEventGroupClearBits(event_group, RTC_DS1307_CONNECTED_BIT);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}
#endif  // USE_RTC_TASK

void app_main(void) {
    esp_err_t err = nvs_flash_init();

#ifdef REBOOT_ON_RESET
    ESP_ERROR_CHECK(nvs_flash_erase());
#else
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
#endif  // REBOOT_ON_RESET

    ESP_ERROR_CHECK(err);

    event_group = xEventGroupCreate();

#ifdef USE_ZNP_TASK
    xTaskCreate(znp_task, "znp_task", 1024 * 5, NULL, 10, NULL);
#endif  // USE_ZNP_TASK

#ifdef USE_WIFI_TASK
    xTaskCreate(wifi_task, "wifi_task", 1024 * 10, NULL, 10, NULL);
#endif  // USE_WIFI_TASK

#ifdef USE_LCD_TASK
    xTaskCreate(lcd_task, "lcd_task", 1024 * 5, NULL, 10, NULL);
#endif  // USE_LCD_TASK

#ifdef USE_RTC_TASK
    xTaskCreate(rtc_task, "rtc_task", 1024 * 5, NULL, 10, NULL);
#endif  // USE_RTC_TASK
}
