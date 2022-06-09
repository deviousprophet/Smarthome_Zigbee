// #define DEBUG_ZNP

#include "zb_znp.h"

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

const char* TAG = "zb_znp";

#define ZNP_UART_NUM UART_NUM_1
#define BUF_SIZE     1024 * 5

static int   _rst_io_num;
znp_msg_cb_t _znp_msg_cb;
uint16_t     _permit_join_dstaddr;

esp_timer_handle_t _permit_join_timer, _wait_msg_timer;
QueueHandle_t      _uart_queue;
SemaphoreHandle_t  znp_send_semphr;
EventGroupHandle_t event_group;
#define VALID_ZNP_MSG_RECEIVED BIT0
#define WAIT_ZNP_MSG_TIMEOUT   BIT1

static znp_msg_t znp_msg_rcv;
#define WAIT_FOR_CMD(cmd)                                                      \
    do {                                                                       \
        xEventGroupWaitBits(event_group, VALID_ZNP_MSG_RECEIVED, true, false,  \
                            portMAX_DELAY);                                    \
    } while (cmd != BUILD_UINT16(znp_msg_rcv.cmd1, znp_msg_rcv.cmd0));

#define WAIT_FOR_STATUS(cmd, offset, status_to_wait)                           \
    do {                                                                       \
        WAIT_FOR_CMD(cmd);                                                     \
    } while (status_to_wait != znp_msg_rcv.data[offset]);

uint8_t znp_calc_fcs(uint8_t* msg, uint8_t len) {
    uint8_t result = 0;
    while (len--) result ^= *msg++;
    return result;
}

static void znp_general_send(znp_msg_t znp_msg) {
    if (xSemaphoreTake(znp_send_semphr, portMAX_DELAY)) {
        size_t   buffer_size = 5 + znp_msg.len;
        uint8_t* buffer      = (uint8_t*)malloc(buffer_size);

        buffer[0] = ZNP_SOF;
        memcpy(buffer + 1, znp_msg.raw, buffer_size - 2);
        buffer[buffer_size - 1] = znp_calc_fcs(znp_msg.raw, buffer_size - 2);
        uart_write_bytes(ZNP_UART_NUM, buffer, buffer_size);
#ifdef DEBUG_ZNP
        printf("TX: ");
        for (uint8_t i = 1; i < buffer_size - 1; i++)
            printf("%02X ", buffer[i]);
        printf("\n");
#endif
        free(buffer);
        buffer = NULL;

        vTaskDelay(1 / portTICK_PERIOD_MS);
        xSemaphoreGive(znp_send_semphr);
    }
}

static void _znp_msg_handler(uint16_t cmd, uint8_t* data, uint8_t len) {
    xEventGroupSetBits(event_group, VALID_ZNP_MSG_RECEIVED);
    _znp_msg_cb(cmd, data, len);
#ifdef DEBUG_ZNP
    printf("RX: ");
    printf("%02X %02X %02X ", len, MSB(cmd), LSB(cmd));
    for (uint8_t i = 0; i < len; i++) printf("%02X ", data[i]);
    printf("\n");
#endif  // DEBUG_ZNP
    switch (cmd) {
        case ZDO_MGMT_LQI_RSP: {
            zdo_mgmt_lqi_rsp_t zdo_mgmt_lqi_rsp = *(zdo_mgmt_lqi_rsp_t*)data;
            ESP_LOGI(TAG, "ZDO_MGMT_LQI_RSP");
            ESP_LOGI(TAG, "SrcAddr: %04X", zdo_mgmt_lqi_rsp.SrcAddr);
            ESP_LOGI(TAG, "Status: %02X (%s)", zdo_mgmt_lqi_rsp.Status,
                     znp_status_to_name(zdo_mgmt_lqi_rsp.Status));
            ESP_LOGI(TAG, "NeighborTableEntries: %02X",
                     zdo_mgmt_lqi_rsp.NeighborTableEntries);
            ESP_LOGI(TAG, "StartIndex: %02X", zdo_mgmt_lqi_rsp.StartIndex);
            ESP_LOGI(TAG, "NeighborTableListCount: %02X",
                     zdo_mgmt_lqi_rsp.NeighborTableListCount);
            for (uint8_t i = 0; i < zdo_mgmt_lqi_rsp.NeighborTableListCount;
                 i++) {
                ESP_LOGI(TAG, "NeighborTableListRecords[%d]", i);
                ESP_LOGI(TAG, "     ExtendedPanID: %016llX",
                         zdo_mgmt_lqi_rsp.NeighborLqiList[i].ExtendedPanID);
                ESP_LOGI(TAG, "     ExtendedAddress: %016llX",
                         zdo_mgmt_lqi_rsp.NeighborLqiList[i].ExtendedAddress);
                ESP_LOGI(TAG, "     NetworkAddress: %04X",
                         zdo_mgmt_lqi_rsp.NeighborLqiList[i].NetworkAddress);
                ESP_LOGI(TAG, "     DeviceType: %02X",
                         zdo_mgmt_lqi_rsp.NeighborLqiList[i].DeviceType);
                ESP_LOGI(TAG, "     RxOnWhenIdle: %02X",
                         zdo_mgmt_lqi_rsp.NeighborLqiList[i].RxOnWhenIdle);
                ESP_LOGI(TAG, "     Relationship: %02X",
                         zdo_mgmt_lqi_rsp.NeighborLqiList[i].Relationship);
                ESP_LOGI(TAG, "     PermitJoining: %02X",
                         zdo_mgmt_lqi_rsp.NeighborLqiList[i].PermitJoining);
                ESP_LOGI(TAG, "     Depth: %02X",
                         zdo_mgmt_lqi_rsp.NeighborLqiList[i].Depth);
                ESP_LOGI(TAG, "     LQI: %02X",
                         zdo_mgmt_lqi_rsp.NeighborLqiList[i].LQI);
            }
        } break;

        case ZDO_MGMT_PERMIT_JOIN_RSP: {
            ESP_LOGI(TAG, "ZDO_MGMT_PERMIT_JOIN_RSP");
            ESP_LOGI(TAG, "SrcAddr: 0x%02X%02X", data[1], data[0]);
            ESP_LOGI(TAG, "Status: " Z_CODE_NAME_STR, ZSTATUS2STR(data[2]));
        } break;

        case ZDO_STATE_CHANGE_IND: {
            ESP_LOGI(TAG, "ZDO_STATE_CHANGE_IND");
            ESP_LOGI(TAG, "State: " Z_CODE_NAME_STR, ZDEVSTATE2STR(data[0]));
        } break;

        case SYS_RESET_IND:
            ESP_LOGI(TAG, "SYS_RESET_IND");
            break;

        default:
            break;
    }
    xEventGroupClearBits(event_group, VALID_ZNP_MSG_RECEIVED);
}

static void znp_frame_parser(uint8_t* data, uint16_t len) {
    enum {
        SOP_STATE,
        LEN_STATE,
        CMD0_STATE,
        CMD1_STATE,
        DATA_STATE,
        FCS_STATE
    } znp_state = SOP_STATE;

    uint8_t ch, data_index = 0;

    while (len--) {
        ch = *data++;
        switch (znp_state) {
            case SOP_STATE:
                if (ch == ZNP_SOF) znp_state = LEN_STATE;
                break;
            case LEN_STATE:
                znp_msg_rcv.len = ch;
                data_index      = 0;
                znp_state       = CMD0_STATE;
                break;
            case CMD0_STATE:
                znp_msg_rcv.cmd0 = ch;
                znp_state        = CMD1_STATE;
                break;
            case CMD1_STATE:
                znp_msg_rcv.cmd1 = ch;
                znp_state = (znp_msg_rcv.len > 0) ? DATA_STATE : FCS_STATE;
                break;
            case DATA_STATE:
                znp_msg_rcv.data[data_index++] = ch;
                if (data_index == znp_msg_rcv.len) znp_state = FCS_STATE;
                break;
            case FCS_STATE:
                if (ch == znp_calc_fcs(znp_msg_rcv.raw, znp_msg_rcv.len + 3))
                    _znp_msg_handler(
                        BUILD_UINT16(znp_msg_rcv.cmd1, znp_msg_rcv.cmd0),
                        znp_msg_rcv.data, znp_msg_rcv.len);
                vTaskDelay(10 / portTICK_PERIOD_MS);
                znp_state = SOP_STATE;
                break;
            default:
                break;
        }
    }
}

static void znp_msg_handle_task(void* arg) {
    uart_event_t event;
    uint8_t*     dtmp = (uint8_t*)malloc(BUF_SIZE);
    while (1) {
        if (xQueueReceive(_uart_queue, &event, portMAX_DELAY)) {
            bzero(dtmp, BUF_SIZE);
            switch (event.type) {
                case UART_DATA:
                    uart_read_bytes(ZNP_UART_NUM, dtmp, event.size,
                                    portMAX_DELAY);
                    znp_frame_parser(dtmp, event.size);
                    break;
                default:
                    break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}

static void permit_join_timeout(void* arg) {
    znp_zdo_permit_join_req(_permit_join_dstaddr, PERMIT_JOIN_DISABLE, 0x00);
}

static void wait_msg_timeout(void* arg) {
    xEventGroupSetBits(event_group, WAIT_ZNP_MSG_TIMEOUT);
}

void znp_init(znp_device_config_t* znp_dev_cfg) {
    const esp_timer_create_args_t permit_join_timer_args = {
        .callback = &permit_join_timeout,
    };
    esp_timer_create(&permit_join_timer_args, &_permit_join_timer);

    const esp_timer_create_args_t wait_msg_timer_args = {
        .callback = &wait_msg_timeout,
    };
    esp_timer_create(&wait_msg_timer_args, &_wait_msg_timer);

    znp_send_semphr = xSemaphoreCreateMutex();

    _rst_io_num = znp_dev_cfg->rst_io_num;
    _znp_msg_cb = znp_dev_cfg->znp_msg_cb;

    uart_config_t uart_config = {
        .baud_rate  = znp_dev_cfg->baud_rate,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    uart_driver_install(ZNP_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 50,
                        &_uart_queue, 0);
    uart_param_config(ZNP_UART_NUM, &uart_config);
    uart_set_pin(ZNP_UART_NUM, znp_dev_cfg->tx_io_num, znp_dev_cfg->rx_io_num,
                 -1, -1);

    gpio_config_t rst_pin_cfg = {
        .pin_bit_mask = (1ULL << _rst_io_num),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&rst_pin_cfg);

    event_group = xEventGroupCreate();
    xTaskCreate(znp_msg_handle_task, "znp_handle", 1024 * 5, NULL, 5, NULL);

    /* Holding in reset */
    gpio_set_level(_rst_io_num, 0);
}

void znp_start_coordinator(uint8_t startopt) {
    ESP_LOGI(TAG, "Starting as ZC ...");

    znp_hard_reset();
    znp_set_startopt(startopt);

    znp_set_logical_type(COORDINATOR);
    znp_app_cnf_bdb_set_channel(1, DEFAULT_CHANNEL_MASK);
    znp_app_cnf_bdb_set_channel(0, CHANNEL_MASK_NONE);

    uint8_t direct_cb = DIRECT_CB_ENABLE;
    znp_sys_osal_nv_write(ZCD_NV_ZDO_DIRECT_CB, 1, &direct_cb);

    // znp_zdo_startup_from_app(DEV_ZB_COORD);
    znp_app_cnf_bdb_start_commissioning(0x04);
    WAIT_FOR_STATUS(ZDO_STATE_CHANGE_IND, 0, DEV_ZB_COORD);
    WAIT_FOR_CMD(APP_CNF_BDB_COMMISSIONING_NOTIFICATION);
}

// void znp_start_router(void) {
//     ESP_LOGI(TAG, "Starting as ZR ...");

//     znp_hard_reset();
//     // uint8_t startopt = STARTOPT_CLEAR_STATE;
//     uint8_t startopt = STARTOPT_CLEAR_ALL;
//     znp_sys_osal_nv_write(ZCD_NV_STARTUP_OPTION, 1, &startopt);
//     znp_hard_reset();

//     uint8_t logical_type = ROUTER;
//     znp_sys_osal_nv_write(ZCD_NV_LOGICAL_TYPE, 1, &logical_type);
//     znp_app_cnf_bdb_set_channel(1, DEFAULT_CHANNEL_MASK);
//     znp_app_cnf_bdb_set_channel(0, CHANNEL_MASK_NONE);

//     znp_zdo_startup_from_app(DEV_ROUTER);

//     uint8_t direct_cb = DIRECT_CB_ENABLE;
//     znp_sys_osal_nv_write(ZCD_NV_ZDO_DIRECT_CB, 1, &direct_cb);
// }

void znp_hard_reset(void) {
    gpio_set_level(_rst_io_num, 0);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    gpio_set_level(_rst_io_num, 1);
    WAIT_FOR_CMD(SYS_RESET_IND);
}

void znp_sys_osal_nv_write(uint16_t Id, uint8_t Len, uint8_t* Value) {
    znp_msg_t znp_msg;
    znp_msg.len  = Len + 4;
    znp_msg.cmd0 = MSB(SYS_OSAL_NV_WRITE);
    znp_msg.cmd1 = LSB(SYS_OSAL_NV_WRITE);

    uint8_t i         = 0;
    znp_msg.data[i++] = LSB(Id);
    znp_msg.data[i++] = MSB(Id);
    znp_msg.data[i++] = 0x00;  // Offset
    znp_msg.data[i++] = Len;
    memcpy(znp_msg.data + i, Value, Len);
    znp_general_send(znp_msg);
    WAIT_FOR_STATUS(SYS_OSAL_NV_WRITE_SRSP, 0, ZSuccess);
}

void znp_app_cnf_bdb_start_commissioning(uint8_t Commissioning_mode) {
    znp_msg_t znp_msg;
    znp_msg.len     = 1;
    znp_msg.cmd0    = MSB(APP_CNF_BDB_START_COMMISSIONING);
    znp_msg.cmd1    = LSB(APP_CNF_BDB_START_COMMISSIONING);
    znp_msg.data[0] = Commissioning_mode;
    znp_general_send(znp_msg);
    WAIT_FOR_STATUS(APP_CNF_BDB_START_COMMISSIONING_SRSP, 0, ZSuccess);
}

void znp_app_cnf_bdb_set_channel(uint8_t isPrimary, uint32_t Channel) {
    znp_msg_t znp_msg;
    znp_msg.len  = 5;
    znp_msg.cmd0 = MSB(APP_CNF_BDB_SET_CHANNEL);
    znp_msg.cmd1 = LSB(APP_CNF_BDB_SET_CHANNEL);

    uint8_t i         = 0;
    znp_msg.data[i++] = isPrimary;
    znp_msg.data[i++] = LSB(Channel);
    znp_msg.data[i++] = LSB(Channel >> 8);
    znp_msg.data[i++] = LSB(Channel >> 16);
    znp_msg.data[i++] = LSB(Channel >> 24);
    znp_general_send(znp_msg);
    WAIT_FOR_STATUS(APP_CNF_BDB_SET_CHANNEL_SRSP, 0, ZSuccess);
}

void znp_zdo_nwk_addr_req(uint64_t IEEEAddress) {
    znp_msg_t znp_msg;
    znp_msg.len  = 10;
    znp_msg.cmd0 = MSB(ZDO_NWK_ADDR_REQ);
    znp_msg.cmd1 = LSB(ZDO_NWK_ADDR_REQ);

    uint8_t i         = 0;
    znp_msg.data[i++] = LSB(IEEEAddress);
    znp_msg.data[i++] = LSB(IEEEAddress >> 8);
    znp_msg.data[i++] = LSB(IEEEAddress >> 16);
    znp_msg.data[i++] = LSB(IEEEAddress >> 24);
    znp_msg.data[i++] = LSB(IEEEAddress >> 32);
    znp_msg.data[i++] = LSB(IEEEAddress >> 40);
    znp_msg.data[i++] = LSB(IEEEAddress >> 48);
    znp_msg.data[i++] = LSB(IEEEAddress >> 56);
    znp_msg.data[i++] = 0x00;  // ReqType
    znp_msg.data[i++] = 0x00;  // StartIndex
    znp_general_send(znp_msg);
    WAIT_FOR_STATUS(ZDO_NWK_ADDR_REQ_SRSP, 0, ZSuccess);
}

void znp_zdo_ieee_addr_req(uint16_t ShortAddr) {
    znp_msg_t znp_msg;
    znp_msg.len  = 4;
    znp_msg.cmd0 = MSB(ZDO_IEEE_ADDR_REQ);
    znp_msg.cmd1 = LSB(ZDO_IEEE_ADDR_REQ);

    uint8_t i         = 0;
    znp_msg.data[i++] = LSB(ShortAddr);
    znp_msg.data[i++] = MSB(ShortAddr);
    znp_msg.data[i++] = 0x00;  // ReqType
    znp_msg.data[i++] = 0x00;  // StartIndex
    znp_general_send(znp_msg);
    WAIT_FOR_STATUS(ZDO_IEEE_ADDR_REQ_SRSP, 0, ZSuccess);
}

void znp_zdo_mgmt_lqi_req(uint16_t DstAddr, uint8_t StartIndex) {
    znp_msg_t znp_msg;
    znp_msg.len  = 3;
    znp_msg.cmd0 = MSB(ZDO_MGMT_LQI_REQ);
    znp_msg.cmd1 = LSB(ZDO_MGMT_LQI_REQ);

    uint8_t i         = 0;
    znp_msg.data[i++] = LSB(DstAddr);
    znp_msg.data[i++] = MSB(DstAddr);
    znp_msg.data[i++] = StartIndex;
    znp_general_send(znp_msg);
    WAIT_FOR_STATUS(ZDO_MGMT_LQI_REQ_SRSP, 0, ZSuccess);
}

void znp_zdo_mgmt_leave_req(uint16_t DstAddr, uint64_t DeviceAddr,
                            uint8_t RemoveChildren_Rejoin) {
    znp_msg_t znp_msg;
    znp_msg.len  = 11;
    znp_msg.cmd0 = MSB(ZDO_MGMT_LEAVE_REQ);
    znp_msg.cmd1 = LSB(ZDO_MGMT_LEAVE_REQ);

    uint8_t i         = 0;
    znp_msg.data[i++] = LSB(DstAddr);
    znp_msg.data[i++] = MSB(DstAddr);
    znp_msg.data[i++] = LSB(DeviceAddr);
    znp_msg.data[i++] = LSB(DeviceAddr >> 8);
    znp_msg.data[i++] = LSB(DeviceAddr >> 16);
    znp_msg.data[i++] = LSB(DeviceAddr >> 24);
    znp_msg.data[i++] = RemoveChildren_Rejoin;
    znp_general_send(znp_msg);
    WAIT_FOR_STATUS(ZDO_MGMT_LEAVE_REQ_SRSP, 0, ZSuccess);
}

void znp_zdo_permit_join_req(uint16_t DstAddr, uint8_t Duration,
                             uint8_t TCSignificance) {
    znp_msg_t znp_msg;
    znp_msg.len  = 4;
    znp_msg.cmd0 = MSB(ZDO_MGMT_PERMIT_JOIN_REQ);
    znp_msg.cmd1 = LSB(ZDO_MGMT_PERMIT_JOIN_REQ);

    uint8_t i         = 0;
    znp_msg.data[i++] = LSB(DstAddr);
    znp_msg.data[i++] = MSB(DstAddr);
    znp_msg.data[i++] = Duration;
    znp_msg.data[i++] = TCSignificance;
    znp_general_send(znp_msg);
    WAIT_FOR_STATUS(ZDO_MGMT_PERMIT_JOIN_REQ_SRSP, 0, ZSuccess);
}

void znp_zdo_startup_from_app(znp_dev_state_t state_to_wait) {
    znp_msg_t znp_msg;
    znp_msg.len     = 1;
    znp_msg.cmd0    = MSB(ZDO_STARTUP_FROM_APP);
    znp_msg.cmd1    = LSB(ZDO_STARTUP_FROM_APP);
    znp_msg.data[0] = 0;
    znp_general_send(znp_msg);
    WAIT_FOR_STATUS(ZDO_STATE_CHANGE_IND, 0, state_to_wait);
    WAIT_FOR_CMD(APP_CNF_BDB_COMMISSIONING_NOTIFICATION);
}

void znp_af_register(uint8_t EndPoint) {
    znp_msg_t znp_msg;
    znp_msg.len  = 9;
    znp_msg.cmd0 = MSB(AF_REGISTER);
    znp_msg.cmd1 = LSB(AF_REGISTER);

    uint8_t i         = 0;
    znp_msg.data[i++] = EndPoint;
    znp_msg.data[i++] = LSB(DEFAULT_PROFILE_ID);
    znp_msg.data[i++] = MSB(DEFAULT_PROFILE_ID);
    znp_msg.data[i++] = LSB(DEFAULT_DEVICE_ID);
    znp_msg.data[i++] = MSB(DEFAULT_DEVICE_ID);
    znp_msg.data[i++] = DEFAULT_DEVICE_VERSION;
    znp_msg.data[i++] = 0x00;
    znp_msg.data[i++] = 0x00;
    znp_msg.data[i++] = 0x00;

    znp_general_send(znp_msg);
    WAIT_FOR_STATUS(AF_REGISTER_SRSP, 0, ZSuccess);
    ESP_LOGI(TAG, "Registered to EndPoint 0x%02X", EndPoint);
}

void znp_af_data_request(af_data_request_t af_data_request) {
    znp_msg_t znp_msg;
    if (af_data_request.Len > 128) af_data_request.Len = 128;

    znp_msg.len  = 10 + af_data_request.Len;
    znp_msg.cmd0 = MSB(AF_DATA_REQUEST);
    znp_msg.cmd1 = LSB(AF_DATA_REQUEST);

    uint8_t i         = 0;
    znp_msg.data[i++] = LSB(af_data_request.DstAddr);
    znp_msg.data[i++] = MSB(af_data_request.DstAddr);
    znp_msg.data[i++] = af_data_request.DestEndpoint;
    znp_msg.data[i++] = af_data_request.SrcEndpoint;
    znp_msg.data[i++] = LSB(af_data_request.ClusterID);
    znp_msg.data[i++] = MSB(af_data_request.ClusterID);
    znp_msg.data[i++] = af_data_request.TransID;
    znp_msg.data[i++] = af_data_request.Options;
    znp_msg.data[i++] = af_data_request.Radius;
    znp_msg.data[i++] = af_data_request.Len;
    memcpy(znp_msg.data + i, af_data_request.Data, af_data_request.Len);
    znp_general_send(znp_msg);
    WAIT_FOR_STATUS(AF_DATA_REQUEST_SRSP, 0, ZSuccess);

    printf("Data send to %04X - endpoint %02X: ", af_data_request.DstAddr,
           af_data_request.DestEndpoint);
    for (uint8_t i = 0; i < af_data_request.Len; i++)
        printf("%02X ", af_data_request.Data[i]);
    printf("\n");
}

void znp_set_startopt(uint8_t startopt) {
    znp_sys_osal_nv_write(ZCD_NV_STARTUP_OPTION, 1, &startopt);
    znp_hard_reset();
}

void znp_set_logical_type(uint8_t logical_type) {
    znp_sys_osal_nv_write(ZCD_NV_LOGICAL_TYPE, 1, &logical_type);
}

void znp_permit_join(uint16_t DstAddr) {
    if (esp_timer_is_active(_permit_join_timer))
        esp_timer_stop(_permit_join_timer);
    _permit_join_dstaddr = DstAddr;
    znp_zdo_permit_join_req(DstAddr, PERMIT_JOIN_ENABLE, 0x00);
    esp_timer_start_once(_permit_join_timer,
                         PERMIT_JOIN_DEFAULT_DURATION * 1000 * 1000);
}

#define ZSTATUS_TBL_IT(zStatus)                                                \
    { zStatus, #zStatus }

DRAM_ATTR static const struct {
    znp_status_t code;
    const char*  msg;
} znp_status_msg_table[] = {
    ZSTATUS_TBL_IT(ZSuccess),
    ZSTATUS_TBL_IT(ZFailure),
    ZSTATUS_TBL_IT(ZInvalidParameter),
    ZSTATUS_TBL_IT(NV_ITEM_UNINIT),
    ZSTATUS_TBL_IT(NV_OPER_FAILED),
    ZSTATUS_TBL_IT(NV_BAD_ITEM_LEN),
    ZSTATUS_TBL_IT(ZMemError),
    ZSTATUS_TBL_IT(ZBufferFull),
    ZSTATUS_TBL_IT(ZUnsupportedMode),
    ZSTATUS_TBL_IT(ZMacMemError),
    ZSTATUS_TBL_IT(zdoInvalidRequestType),
    ZSTATUS_TBL_IT(zdoInvalidEndpoint),
    ZSTATUS_TBL_IT(zdoUnsupported),
    ZSTATUS_TBL_IT(zdoTimeout),
    ZSTATUS_TBL_IT(zdoNoMatch),
    ZSTATUS_TBL_IT(zdoTableFull),
    ZSTATUS_TBL_IT(zdoNoBindEntry),
    ZSTATUS_TBL_IT(ZSecNoKey),
    ZSTATUS_TBL_IT(ZSecMaxFrmCount),
    ZSTATUS_TBL_IT(ZApsFail),
    ZSTATUS_TBL_IT(ZApsTableFull),
    ZSTATUS_TBL_IT(ZApsIllegalRequest),
    ZSTATUS_TBL_IT(ZApsInvalidBinding),
    ZSTATUS_TBL_IT(ZApsUnsupportedAttrib),
    ZSTATUS_TBL_IT(ZApsNotSupported),
    ZSTATUS_TBL_IT(ZApsNoAck),
    ZSTATUS_TBL_IT(ZApsDuplicateEntry),
    ZSTATUS_TBL_IT(ZApsNoBoundDevice),
    ZSTATUS_TBL_IT(ZNwkInvalidParam),
    ZSTATUS_TBL_IT(ZNwkInvalidRequest),
    ZSTATUS_TBL_IT(ZNwkNotPermitted),
    ZSTATUS_TBL_IT(ZNwkStartupFailure),
    ZSTATUS_TBL_IT(ZNwkTableFull),
    ZSTATUS_TBL_IT(ZNwkUnknownDevice),
    ZSTATUS_TBL_IT(ZNwkUnsupportedAttribute),
    ZSTATUS_TBL_IT(ZNwkNoNetworks),
    ZSTATUS_TBL_IT(ZNwkLeaveUnconfirmed),
    ZSTATUS_TBL_IT(ZNwkNoAck),
    ZSTATUS_TBL_IT(ZNwkNoRoute),
    ZSTATUS_TBL_IT(ZMacNoACK),
};

const char* znp_status_to_name(znp_status_t status_code) {
    for (uint8_t i = 0;
         i < sizeof(znp_status_msg_table) / sizeof(znp_status_msg_table[0]);
         i++) {
        if (znp_status_msg_table[i].code == status_code)
            return znp_status_msg_table[i].msg;
    }
    return "UNKNOWN";
}

#define ZDEV_STATE_TBL_IT(zDevState)                                           \
    { zDevState, #zDevState }

DRAM_ATTR static const struct {
    znp_dev_state_t code;
    const char*     state;
} znp_dev_state_table[] = {
    ZDEV_STATE_TBL_IT(DEV_HOLD),
    ZDEV_STATE_TBL_IT(DEV_INIT),
    ZDEV_STATE_TBL_IT(DEV_NWK_DISC),
    ZDEV_STATE_TBL_IT(DEV_NWK_JOINING),
    ZDEV_STATE_TBL_IT(DEV_NWK_REJOIN),
    ZDEV_STATE_TBL_IT(DEV_END_DEVICE_UNAUTH),
    ZDEV_STATE_TBL_IT(DEV_END_DEVICE),
    ZDEV_STATE_TBL_IT(DEV_ROUTER),
    ZDEV_STATE_TBL_IT(DEV_COORD_STARTING),
    ZDEV_STATE_TBL_IT(DEV_ZB_COORD),
    ZDEV_STATE_TBL_IT(DEV_NWK_ORPHAN),
};

const char* znp_dev_state_to_name(znp_dev_state_t state_code) {
    for (uint8_t i = 0;
         i < sizeof(znp_dev_state_table) / sizeof(znp_dev_state_table[0]);
         i++) {
        if (znp_dev_state_table[i].code == state_code)
            return znp_dev_state_table[i].state;
    }
    return "UNKNOWN";
}
