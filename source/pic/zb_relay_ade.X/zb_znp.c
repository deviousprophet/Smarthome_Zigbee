#include "zb_znp.h"
#include "it_handle.h"

#define ZNP_RST_TRIS	TRISDbits.TRISD4
#define ZNP_RST_PORT	PORTDbits.RD4

static znp_msg_cb_t _zb_znp_cb;

enum {
    SOP_STATE,
    LEN_STATE,
    CMD0_STATE,
    CMD1_STATE,
    DATA_STATE,
    FCS_STATE
} znp_state = SOP_STATE;

uint8_t znp_len;
uint8_t znp_cmd0;
uint8_t znp_cmd1;
uint8_t znp_fcs;

volatile uint8_t znp_data_rcv[30];
volatile uint8_t znp_data_rcv_index = 0;

volatile uint16_t last_cmd_rcv;

void wait_for_msg(uint16_t cmd) {
    while (last_cmd_rcv != cmd);
    last_cmd_rcv = 0x00;
}

void wait_for_status(uint16_t cmd, uint8_t offset, uint8_t status_to_wait) {
    do {
        wait_for_msg(cmd);
    } while (znp_data_rcv[offset] != status_to_wait);
}

void znp_frame_parser(uint8_t ch) {
    switch (znp_state) {
        case SOP_STATE:
            if (ch == ZNP_SOF) {
                znp_fcs = 0;
                znp_data_rcv_index = 0;
                znp_state = LEN_STATE;
            }
            break;
        case LEN_STATE:
            znp_len = ch;
            znp_fcs ^= ch;
            znp_state = CMD0_STATE;
            break;
        case CMD0_STATE:
            znp_cmd0 = ch;
            znp_fcs ^= ch;
            znp_state = CMD1_STATE;
            break;
        case CMD1_STATE:
            znp_cmd1 = ch;
            znp_fcs ^= ch;
            znp_state = (znp_len > 0) ? DATA_STATE : FCS_STATE;
            break;
        case DATA_STATE:
            znp_data_rcv[znp_data_rcv_index++] = ch;
            znp_fcs ^= ch;
            if (znp_data_rcv_index == znp_len)
                znp_state = FCS_STATE;
            break;
        case FCS_STATE:
            if (znp_fcs == ch) {
                last_cmd_rcv = BUILD_UINT16(znp_cmd1, znp_cmd0);
                _zb_znp_cb(
                        last_cmd_rcv,
                        (uint8_t*) znp_data_rcv,
                        znp_len
                        );
            }
            znp_state = SOP_STATE;
            break;
        default:
            break;
    }
}

void znp_usart_event_handler(usart_event_t rx_event, void* event_data) {
    switch (rx_event) {
        case USART_RX_DATA:
            znp_frame_parser(*(uint8_t*) event_data);
            break;
        case USART_RX_CMPLT:
            znp_state = SOP_STATE;
            break;
        default:
            break;
    }
}

void znp_set_startopt(uint8_t startopt) {
    usart_send(ZNP_SOF);
    usart_send(0x05);
    usart_send(0x21);
    usart_send(0x09);
    usart_send(0x03);
    usart_send(0x00);
    usart_send(0x00);
    usart_send(0x01);
    usart_send(startopt);
    usart_send(0x2F ^ startopt);
    wait_for_status(SYS_OSAL_NV_WRITE_SRSP, 0, 0x00);

    znp_hard_reset();
}

void znp_set_logical_type(uint8_t logical_type) {
    usart_send(ZNP_SOF);
    usart_send(0x05);
    usart_send(0x21);
    usart_send(0x09);
    usart_send(0x87);
    usart_send(0x00);
    usart_send(0x00);
    usart_send(0x01);
    usart_send(logical_type);
    usart_send(0xAB ^ logical_type);
    wait_for_status(SYS_OSAL_NV_WRITE_SRSP, 0, 0x00);
}

void znp_set_channels(void) {
    usart_send(ZNP_SOF);
    usart_send(0x05);
    usart_send(0x2F);
    usart_send(0x08);
    usart_send(0x01);
    usart_send(0x00);
    usart_send(0x20);
    usart_send(0x00);
    usart_send(0x00);
    usart_send(0x03);
    wait_for_status(APP_CNF_BDB_SET_CHANNEL_SRSP, 0, 0x00);

    usart_send(ZNP_SOF);
    usart_send(0x05);
    usart_send(0x2F);
    usart_send(0x08);
    usart_send(0x00);
    usart_send(0x00);
    usart_send(0x00);
    usart_send(0x00);
    usart_send(0x00);
    usart_send(0x22);
    wait_for_status(APP_CNF_BDB_SET_CHANNEL_SRSP, 0, 0x00);
}

void znp_init(znp_msg_cb_t callback) {
    _zb_znp_cb = callback;

    //    RST pin
    ZNP_RST_TRIS = 0;
    ZNP_RST_PORT = 0;

    usart_init();
    usart_add_isr_handler(znp_usart_event_handler);
}

void znp_hard_reset(void) {
    ZNP_RST_PORT = 0;
    __delay_ms(10);
    ZNP_RST_PORT = 1;
    wait_for_msg(SYS_RESET_IND);
}

void znp_af_register(uint8_t endpoint) {
    usart_send(ZNP_SOF);
    usart_send(0x09);
    usart_send(0x24);
    usart_send(0x00);
    usart_send(endpoint);
    usart_send(0x04);
    usart_send(0x01);
    usart_send(0x23);
    usart_send(0x01);
    usart_send(0x89);
    usart_send(0x00);
    usart_send(0x00);
    usart_send(0x00);
    usart_send(0x83 ^ endpoint);
    wait_for_status(AF_REGISTER_SRSP, 0, 0x00);
}

void znp_zdo_startup_from_app(void) {
    usart_send(ZNP_SOF);
    usart_send(0x01);
    usart_send(0x25);
    usart_send(0x40);
    usart_send(0x00);
    usart_send(0x64);
    wait_for_msg(APP_CNF_BDB_COMMISSIONING_NOTIFICATION);
}

void znp_af_data_request(
        uint8_t endpoint,
        uint16_t cluster_id,
        uint8_t* data,
        uint8_t len,
        bool wait_for_rsps) {
    uint8_t fcs_calc = 0;
    if (data == NULL) len = 0;
    for (uint8_t i = 0; i < len; i++)
        fcs_calc ^= data[i];
    fcs_calc ^= 10 + len;
    fcs_calc ^= 0x24;
    fcs_calc ^= 0x01;
    fcs_calc ^= 0x0F;
    fcs_calc ^= len;
    fcs_calc ^= LSB(cluster_id);
    fcs_calc ^= MSB(cluster_id);

    usart_send(ZNP_SOF);
    usart_send(10 + len);
    usart_send(0x24); // cmd0
    usart_send(0x01); // cmd1
    usart_send(0x00); // DstAddr
    usart_send(0x00); // DstAddr
    usart_send(endpoint);
    usart_send(endpoint);
    usart_send(LSB(cluster_id)); // ClusterID
    usart_send(MSB(cluster_id)); // ClusterID
    usart_send(0x00); // TransID
    usart_send(0x00); // Options
    usart_send(0x0F); // Radius
    usart_send(len);
    for (uint8_t i = 0; i < len; i++)
        usart_send(data[i]);
    usart_send(fcs_calc);

    if (wait_for_rsps)
        wait_for_status(AF_DATA_REQUEST_SRSP, 0, 0x00);
}

void znp_router_start(znp_router_start_opt_t start_opt) {
    znp_hard_reset();
    switch (start_opt) {
        case ROUTER_START_NEW_NETWORK:
            znp_set_startopt(STARTOPT_CLEAR_ALL);
            znp_set_logical_type(ROUTER);
            znp_set_channels();
        case ROUTER_START_REJOIN:
            znp_zdo_startup_from_app();
            break;
        default:
            break;
    }
    znp_af_register(ENDPOINT_PROVISION);
    znp_af_register(ENDPOINT_TELEMETRY);
    znp_af_register(ENDPOINT_COMMAND);
    znp_af_register(ENDPOINT_KEEPALIVE);
}