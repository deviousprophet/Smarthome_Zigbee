/* 
 * File:   zb_znp.h
 * Author: deviousprophet
 *
 * Created on January 13, 2022, 8:51 AM
 */

#ifndef ZB_ZNP_H
#define	ZB_ZNP_H

#ifdef	__cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "main.h"
#include "usart.h"

#define ENDPOINT_PROVISION          0x01
#define ENDPOINT_TELEMETRY          0x02
#define ENDPOINT_COMMAND            0x03
#define ENDPOINT_KEEPALIVE          0x04

    /* ZCD */
#define ZCD_NV_STARTUP_OPTION						0x0003
#define ZCD_NV_PANID								0x0083
#define ZCD_NV_CHANLIST								0x0084
#define ZCD_NV_LOGICAL_TYPE							0x0087
#define ZCD_NV_ZDO_DIRECT_CB						0x008F

    /* ZCD_NV_STARTUP_OPTION */
#define STARTOPT_CLEAR_CONFIG   BIT0
#define STARTOPT_CLEAR_STATE    BIT1
#define STARTOPT_CLEAR_ALL      (BIT0 | BIT1)
#define STARTOPT_CLEAR_NONE     0x00

    /* ZCD_NV_PANID */
#define DEFAULT_PANID								0x00F1

    /* ZCD_NV_CHANLIST */
#define CHANNEL_PRIMARY								0x1
#define CHANNEL_SECONDARY							0x0
#define CHANNEL_MASK_11								0x00000800
#define CHANNEL_MASK_12								0x00001000
#define CHANNEL_MASK_13								0x00002000
#define CHANNEL_MASK_14								0x00004000
#define CHANNEL_MASK_15								0x00008000
#define CHANNEL_MASK_16								0x00010000
#define CHANNEL_MASK_17								0x00020000
#define CHANNEL_MASK_18								0x00040000
#define CHANNEL_MASK_19								0x00080000
#define CHANNEL_MASK_20								0x00100000
#define CHANNEL_MASK_21								0x00200000
#define CHANNEL_MASK_22								0x00400000
#define CHANNEL_MASK_23								0x00800000
#define CHANNEL_MASK_24								0x01000000
#define CHANNEL_MASK_25								0x02000000
#define CHANNEL_MASK_26								0x04000000
#define CHANNEL_MASK_NONE							0x00000000
#define CHANNEL_MASK_ALL							0x07FFF800
#define DEFAULT_CHANNEL_MASK						CHANNEL_MASK_13

    /* ZCD_NV_LOGICAL_TYPE */
#define COORDINATOR									0x00
#define ROUTER										0x01
#define END_DEVICE									0x02

    /* ZCD_NV_ZDO_DIRECT_CB */
#define DIRECT_CB_ENABLE							0x01
#define DIRECT_CB_DISABLE							0x00

#define ALL_ROUTERS_AND_COORDINATOR					0xFFFC

    /* SYS Interface */
#define SYS_OSAL_NV_WRITE							0x2109
#define SYS_OSAL_NV_WRITE_SRSP						0x6109
#define SYS_RESET_REQ								0x4100
#define SYS_RESET_IND								0x4180

#define APP_CNF_BDB_SET_CHANNEL						0x2F08
#define APP_CNF_BDB_SET_CHANNEL_SRSP				0x6F08
#define APP_CNF_BDB_COMMISSIONING_NOTIFICATION		0x4F80

#define AF_REGISTER									0x2400
#define AF_REGISTER_SRSP							0x6400
#define AF_DATA_REQUEST                             0x2401
#define AF_DATA_REQUEST_SRSP                        0x6401
#define AF_DATA_CONFIRM								0x4480
#define AF_INCOMING_MSG                             0x4481

    /* ZDO Interface */
#define ZDO_STARTUP_FROM_APP						0x2540
#define ZDO_STARTUP_FROM_APP_SRSP					0x6540
#define ZDO_MGMT_PERMIT_JOIN_RSP					0x45B6
#define ZDO_STATE_CHANGE_IND						0x45C0

#define ZB_PERMIT_JOINING_REQUEST					0x2608
#define ZB_PERMIT_JOINING_REQUEST_SRSP				0x6608
#define PERMIT_JOINING_ENABLE						0xFF
#define PERMIT_JOINING_DISABLE						0x00
#define PERMIT_JOINING_TIMEOUT_DEFAULT				0x05

#define DEFAULT_PROFILE_ID							0x0104	// Home Automation
#define DEFAULT_DEVICE_ID							0x0123
#define DEFAULT_ENDPOINT                            0x0A

#define ZNP_SOF										0xFE

    typedef enum {
        ROUTER_START_NEW_NETWORK,
        ROUTER_START_REJOIN,
    } znp_router_start_opt_t;

    typedef void (*znp_msg_cb_t)(uint16_t cmd, uint8_t* data, uint8_t len);

    void znp_init(znp_msg_cb_t callback);
    void znp_hard_reset(void);
    void znp_af_register(uint8_t endpoint);
    void znp_zdo_startup_from_app(void);
    void znp_af_data_request(
            uint8_t endpoint,
            uint16_t cluster_id,
            uint8_t* data,
            uint8_t len,
            bool wait_for_rsps);

    void znp_router_start(znp_router_start_opt_t start_opt);

#ifdef	__cplusplus
}
#endif

#endif	/* ZB_ZNP_H */

