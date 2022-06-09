#include "dev_timeslot.h"

#include <esp_log.h>
#include <string.h>

static const char* TAG = "timeslot";

zb_dev_timeslot_t* g_dev_timeslot;

esp_err_t json_parse_timeslot(cJSON* data, timeslot_t* timeslot) {
    /* Get timeslot */
    cJSON* time_on  = cJSON_GetObjectItem(data, "timeOn");
    cJSON* time_off = cJSON_GetObjectItem(data, "timeOff");
    cJSON* weekday  = cJSON_GetObjectItem(data, "weekday");

    if (!cJSON_IsString(time_on)) return ESP_FAIL;
    if (!cJSON_IsString(time_off)) return ESP_FAIL;
    if (!cJSON_IsObject(weekday)) return ESP_FAIL;

    char timeslot_str_buf[12];

    if (strlen(time_on->valuestring) == 10) {
        memcpy(timeslot_str_buf + 1, time_on->valuestring, 11);
        timeslot_str_buf[0] = '0';
    } else if (strlen(time_on->valuestring) == 11) {
        memcpy(timeslot_str_buf, time_on->valuestring, 12);
    } else
        return ESP_FAIL;

    timeslot->time_on.am_pm = (timeslot_str_buf[9] == 'P');
    timeslot->time_on.hours =
        (uint8_t)((timeslot_str_buf[0] - 48) * 10 + (timeslot_str_buf[1] - 48));
    timeslot->time_on.minutes =
        (uint8_t)((timeslot_str_buf[3] - 48) * 10 + (timeslot_str_buf[4] - 48));
    timeslot->time_on.seconds =
        (uint8_t)((timeslot_str_buf[6] - 48) * 10 + (timeslot_str_buf[7] - 48));

    if (strlen(time_off->valuestring) == 10) {
        memcpy(timeslot_str_buf + 1, time_off->valuestring, 11);
        timeslot_str_buf[0] = '0';
    } else if (strlen(time_off->valuestring) == 11) {
        memcpy(timeslot_str_buf, time_off->valuestring, 12);
    } else
        return ESP_FAIL;

    timeslot->time_off.am_pm = (timeslot_str_buf[9] == 'P');
    timeslot->time_off.hours =
        (uint8_t)((timeslot_str_buf[0] - 48) * 10 + (timeslot_str_buf[1] - 48));
    timeslot->time_off.minutes =
        (uint8_t)((timeslot_str_buf[3] - 48) * 10 + (timeslot_str_buf[4] - 48));
    timeslot->time_off.seconds =
        (uint8_t)((timeslot_str_buf[6] - 48) * 10 + (timeslot_str_buf[7] - 48));

    /* Get weekday */
    cJSON* monday    = cJSON_GetObjectItem(weekday, "Monday");
    cJSON* tuesday   = cJSON_GetObjectItem(weekday, "Tuesday");
    cJSON* wednesday = cJSON_GetObjectItem(weekday, "Wednesday");
    cJSON* thursday  = cJSON_GetObjectItem(weekday, "Thursday");
    cJSON* friday    = cJSON_GetObjectItem(weekday, "Friday");
    cJSON* saturday  = cJSON_GetObjectItem(weekday, "Saturday");
    cJSON* sunday    = cJSON_GetObjectItem(weekday, "Sunday");

    if (!cJSON_IsBool(monday)) return ESP_FAIL;
    if (!cJSON_IsBool(tuesday)) return ESP_FAIL;
    if (!cJSON_IsBool(wednesday)) return ESP_FAIL;
    if (!cJSON_IsBool(thursday)) return ESP_FAIL;
    if (!cJSON_IsBool(friday)) return ESP_FAIL;
    if (!cJSON_IsBool(saturday)) return ESP_FAIL;
    if (!cJSON_IsBool(sunday)) return ESP_FAIL;

    timeslot->weekday.monday    = cJSON_IsTrue(monday);
    timeslot->weekday.tuesday   = cJSON_IsTrue(tuesday);
    timeslot->weekday.wednesday = cJSON_IsTrue(wednesday);
    timeslot->weekday.thursday  = cJSON_IsTrue(thursday);
    timeslot->weekday.friday    = cJSON_IsTrue(friday);
    timeslot->weekday.saturday  = cJSON_IsTrue(saturday);
    timeslot->weekday.sunday    = cJSON_IsTrue(sunday);

    return ESP_OK;
}

void set_timeslot_to_dev(uint64_t ieee_addr, uint16_t cluster_id,
                         timeslot_t* timeslot) {
    if (timeslot == NULL) return;

    remove_timeslot_from_dev(ieee_addr);
    zb_dev_timeslot_t* new_dev_timeslot =
        (zb_dev_timeslot_t*)malloc(sizeof(zb_dev_timeslot_t));
    new_dev_timeslot->ieee_addr  = ieee_addr;
    new_dev_timeslot->cluster_id = cluster_id;
    new_dev_timeslot->timeslot   = *timeslot;
    new_dev_timeslot->next       = g_dev_timeslot;
    g_dev_timeslot               = new_dev_timeslot;
    ESP_LOGI(TAG, "New timelsot set to %016llX", ieee_addr);
}

void remove_timeslot_from_dev(uint64_t ieee_addr) {
    zb_dev_timeslot_t* temp = g_dev_timeslot;
    zb_dev_timeslot_t* prev = NULL;
    if (temp != NULL && g_dev_timeslot->ieee_addr == ieee_addr) {
        g_dev_timeslot = temp->next;
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

zb_dev_timeslot_t* get_timeslot_list(void) { return g_dev_timeslot; }

void print_timeslot_list(void) {
    printf("Timeslot list\n");
    zb_dev_timeslot_t* temp = g_dev_timeslot;
    while (temp != NULL) {
        printf(
            "%016llX - %04X => %02d:%02d:%02d %s - %02d:%02d:%02d %s - "
            "%s%s%s%s%s%s%s\n",
            temp->ieee_addr, temp->cluster_id, temp->timeslot.time_on.hours,
            temp->timeslot.time_on.minutes, temp->timeslot.time_on.seconds,
            temp->timeslot.time_on.am_pm ? "PM" : "AM",
            temp->timeslot.time_off.hours, temp->timeslot.time_off.minutes,
            temp->timeslot.time_off.seconds,
            temp->timeslot.time_off.am_pm ? "PM" : "AM",
            temp->timeslot.weekday.monday ? "2" : "",
            temp->timeslot.weekday.tuesday ? "3" : "",
            temp->timeslot.weekday.wednesday ? "4" : "",
            temp->timeslot.weekday.thursday ? "5" : "",
            temp->timeslot.weekday.friday ? "6" : "",
            temp->timeslot.weekday.saturday ? "7" : "",
            temp->timeslot.weekday.sunday ? "CN" : "");

        temp = temp->next;
    }
}
