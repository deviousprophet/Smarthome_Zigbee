#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifndef MSB
#define MSB(a) (((a) >> 8) & 0xFF)
#endif

#ifndef LSB
#define LSB(a) ((a)&0xFF)
#endif

#ifndef BUILD_UINT16
#define BUILD_UINT16(loByte, hiByte)                                           \
    ((uint16_t)(((loByte)&0x00FF) + (((hiByte)&0x00FF) << 8)))
#endif

/* ZCD */
#define ZCD_NV_STARTUP_OPTION                  0x0003
#define ZCD_NV_PANID                           0x0083
#define ZCD_NV_CHANLIST                        0x0084
#define ZCD_NV_LOGICAL_TYPE                    0x0087
#define ZCD_NV_ZDO_DIRECT_CB                   0x008F

/* ZCD_NV_STARTUP_OPTION */
#define STARTOPT_CLEAR_CONFIG                  0x01
#define STARTOPT_CLEAR_STATE                   0x02
#define STARTOPT_CLEAR_ALL                     (STARTOPT_CLEAR_CONFIG | STARTOPT_CLEAR_STATE)
#define STARTOPT_CLEAR_NONE                    0x00

/* ZCD_NV_PANID */
#define DEFAULT_PANID                          0xABCD

/* ZCD_NV_CHANLIST */
#define CHANNEL_PRIMARY                        0x1
#define CHANNEL_SECONDARY                      0x0
#define CHANNEL_MASK_11                        0x00000800
#define CHANNEL_MASK_12                        0x00001000
#define CHANNEL_MASK_13                        0x00002000
#define CHANNEL_MASK_14                        0x00004000
#define CHANNEL_MASK_15                        0x00008000
#define CHANNEL_MASK_16                        0x00010000
#define CHANNEL_MASK_17                        0x00020000
#define CHANNEL_MASK_18                        0x00040000
#define CHANNEL_MASK_19                        0x00080000
#define CHANNEL_MASK_20                        0x00100000
#define CHANNEL_MASK_21                        0x00200000
#define CHANNEL_MASK_22                        0x00400000
#define CHANNEL_MASK_23                        0x00800000
#define CHANNEL_MASK_24                        0x01000000
#define CHANNEL_MASK_25                        0x02000000
#define CHANNEL_MASK_26                        0x04000000
#define CHANNEL_MASK_NONE                      0x00000000
#define CHANNEL_MASK_ALL                       0x07FFF800
#define DEFAULT_CHANNEL_MASK                   CHANNEL_MASK_13

/* ZCD_NV_LOGICAL_TYPE */
#define COORDINATOR                            0x00
#define ROUTER                                 0x01
#define END_DEVICE                             0x02

/* ZCD_NV_ZDO_DIRECT_CB */
#define DIRECT_CB_ENABLE                       0x01
#define DIRECT_CB_DISABLE                      0x00

#define ALL_ROUTERS_AND_COORDINATOR            0xFFFC

/* SYS Interface */
#define SYS_OSAL_NV_WRITE                      0x2109
#define SYS_OSAL_NV_WRITE_SRSP                 0x6109
#define SYS_RESET_REQ                          0x4100
#define SYS_RESET_IND                          0x4180

#define APP_CNF_BDB_START_COMMISSIONING        0x2F05
#define APP_CNF_BDB_START_COMMISSIONING_SRSP   0x6F05
#define APP_CNF_BDB_SET_CHANNEL                0x2F08
#define APP_CNF_BDB_SET_CHANNEL_SRSP           0x6F08
#define APP_CNF_BDB_COMMISSIONING_NOTIFICATION 0x4F80

/* ZDO Interface */
#define ZDO_NWK_ADDR_REQ                       0x2500
#define ZDO_NWK_ADDR_REQ_SRSP                  0x6500
#define ZDO_NWK_ADDR_RSP                       0x4580
#define ZDO_IEEE_ADDR_REQ                      0x2501
#define ZDO_IEEE_ADDR_REQ_SRSP                 0x6501
#define ZDO_IEEE_ADDR_RSP                      0x4581
#define ZDO_ACTIVE_EP_REQ                      0x2505
#define ZDO_ACTIVE_EP_REQ_SRSP                 0x6505
#define ZDO_ACTIVE_EP_RSP                      0x4585
#define ZDO_MGMT_LQI_REQ                       0x2531
#define ZDO_MGMT_LQI_REQ_SRSP                  0x6531
#define ZDO_MGMT_LQI_RSP                       0x45B1
#define ZDO_MGMT_RTG_REQ                       0x2532
#define ZDO_MGMT_RTG_REQ_SRSP                  0x6532
#define ZDO_MGMT_RTG_RSP                       0x45B2
#define ZDO_MGMT_LEAVE_REQ                     0x2534
#define ZDO_MGMT_LEAVE_REQ_SRSP                0x6534
#define ZDO_MGMT_LEAVE_RSP                     0x45B4
#define ZDO_MGMT_PERMIT_JOIN_REQ               0x2536
#define ZDO_MGMT_PERMIT_JOIN_REQ_SRSP          0x6536
#define ZDO_MGMT_PERMIT_JOIN_RSP               0x45B6
#define ZDO_STARTUP_FROM_APP                   0x2540
#define ZDO_STATE_CHANGE_IND                   0x45C0
#define ZDO_LEAVE_IND                          0x45C9
#define ZDO_TC_DEV_IND                         0x45CA

/* AF Interface */
#define AF_REGISTER                            0x2400
#define AF_REGISTER_SRSP                       0x6400
#define AF_DATA_REQUEST                        0x2401
#define AF_DATA_REQUEST_SRSP                   0x6401
#define AF_DATA_CONFIRM                        0x4480
#define AF_INCOMING_MSG                        0x4481

#define ZB_PERMIT_JOINING_REQUEST              0x2608
#define ZB_PERMIT_JOINING_REQUEST_SRSP         0x6608

#define PERMIT_JOIN_ENABLE                     0xFF
#define PERMIT_JOIN_DISABLE                    0x00
#define PERMIT_JOIN_DEFAULT_DURATION           20
#define ALL_ROUTERS_AND_COORDINATOR            0xFFFC

#define DEFAULT_PROFILE_ID                     0x0104  // Home Automation
#define DEFAULT_DEVICE_ID                      0x0123
#define DEFAULT_DEVICE_VERSION                 0x00
#define DEFAULT_ENDPOINT                       0x0F

#define ZNP_SOF                                0xFE

#define Z_CODE_NAME_STR                        "0x%02X (%s)"

typedef enum {
    ZSuccess                 = 0x00,
    ZFailure                 = 0x01,
    ZInvalidParameter        = 0x02,
    NV_ITEM_UNINIT           = 0x09,
    NV_OPER_FAILED           = 0x0a,
    NV_BAD_ITEM_LEN          = 0x0c,
    ZMemError                = 0x10,
    ZBufferFull              = 0x11,
    ZUnsupportedMode         = 0x12,
    ZMacMemError             = 0x13,
    zdoInvalidRequestType    = 0x80,
    zdoInvalidEndpoint       = 0x82,
    zdoUnsupported           = 0x84,
    zdoTimeout               = 0x85,
    zdoNoMatch               = 0x86,
    zdoTableFull             = 0x87,
    zdoNoBindEntry           = 0x88,
    ZSecNoKey                = 0xa1,
    ZSecMaxFrmCount          = 0xa3,
    ZApsFail                 = 0xb1,
    ZApsTableFull            = 0xb2,
    ZApsIllegalRequest       = 0xb3,
    ZApsInvalidBinding       = 0xb4,
    ZApsUnsupportedAttrib    = 0xb5,
    ZApsNotSupported         = 0xb6,
    ZApsNoAck                = 0xb7,
    ZApsDuplicateEntry       = 0xb8,
    ZApsNoBoundDevice        = 0xb9,
    ZNwkInvalidParam         = 0xc1,
    ZNwkInvalidRequest       = 0xc2,
    ZNwkNotPermitted         = 0xc3,
    ZNwkStartupFailure       = 0xc4,
    ZNwkTableFull            = 0xc7,
    ZNwkUnknownDevice        = 0xc8,
    ZNwkUnsupportedAttribute = 0xc9,
    ZNwkNoNetworks           = 0xca,
    ZNwkLeaveUnconfirmed     = 0xcb,
    ZNwkNoAck                = 0xcc,
    ZNwkNoRoute              = 0xcd,
    ZMacNoACK                = 0xe9,
} znp_status_t;

const char* znp_status_to_name(znp_status_t status_code);

#define ZSTATUS2STR(status_code) status_code, znp_status_to_name(status_code)

typedef enum {
    DEV_HOLD = 0,
    DEV_INIT,
    DEV_NWK_DISC,
    DEV_NWK_JOINING,
    DEV_NWK_REJOIN,
    DEV_END_DEVICE_UNAUTH,
    DEV_END_DEVICE,
    DEV_ROUTER,
    DEV_COORD_STARTING,
    DEV_ZB_COORD,
    DEV_NWK_ORPHAN,
} znp_dev_state_t;

const char* znp_dev_state_to_name(znp_dev_state_t state_code);

#define ZDEVSTATE2STR(state_code) state_code, znp_dev_state_to_name(state_code)

typedef union {
    uint8_t raw[253];
    struct {
        uint8_t len;
        uint8_t cmd0;
        uint8_t cmd1;
        uint8_t data[250];
    };
} znp_msg_t;

typedef struct __attribute__((packed)) {
    uint16_t SrcAddr;
    uint8_t  Status;
    uint8_t  NeighborTableEntries;
    uint8_t  StartIndex;
    uint8_t  NeighborTableListCount;
    union {
        uint8_t NeighborTableListRecords[66];
        struct __attribute__((packed)) {
            uint64_t ExtendedPanID;
            uint64_t ExtendedAddress;
            uint16_t NetworkAddress;
            uint8_t  DeviceType   : 2;
            uint8_t  RxOnWhenIdle : 2;
            uint8_t  Relationship : 3;
            uint8_t  Reserved     : 1;
            uint8_t  PermitJoining;
            uint8_t  Depth;
            uint8_t  LQI;
        } NeighborLqiList[3];
    };
} zdo_mgmt_lqi_rsp_t;

typedef struct __attribute__((packed)) {
    uint16_t SrcNwkAddr;
    uint64_t SrcIEEEAddr;
    uint16_t ParentNwkAddr;
} zdo_tc_dev_ind_t;

typedef struct {
    uint16_t DstAddr;
    uint8_t  DestEndpoint;
    uint8_t  SrcEndpoint;
    uint16_t ClusterID;
    uint8_t  TransID;
    uint8_t  Options;
    uint8_t  Radius;
    uint8_t  Len;
    uint8_t* Data;
} af_data_request_t;

typedef struct __attribute__((packed)) {
    uint16_t GroupID;
    uint16_t ClusterID;
    uint16_t SrcAddr;
    uint8_t  SrcEndpoint;
    uint8_t  DestEndpoint;
    uint8_t  WasBroadcast;
    uint8_t  LinkQuality;
    uint8_t  SecurityUse;
    uint32_t Timestamp;
    uint8_t  TransSeqNumber;
    uint8_t  Len;
    uint8_t  Data[99];
} af_incoming_msg_t;

typedef void (*znp_msg_cb_t)(uint16_t cmd, uint8_t* data, uint8_t len);

typedef struct {
    int          baud_rate;
    int          tx_io_num;
    int          rx_io_num;
    int          rst_io_num;
    znp_msg_cb_t znp_msg_cb;
} znp_device_config_t;

void znp_init(znp_device_config_t* znp_dev_cfg);
void znp_start_coordinator(uint8_t startopt);
// void znp_start_router(void);
void znp_hard_reset(void);

void znp_sys_osal_nv_write(uint16_t Id, uint8_t Len, uint8_t* Value);

void znp_app_cnf_bdb_start_commissioning(uint8_t Commissioning_mode);
void znp_app_cnf_bdb_set_channel(uint8_t isPrimary, uint32_t Channel);

void znp_zdo_nwk_addr_req(uint64_t IEEEAddress);
void znp_zdo_ieee_addr_req(uint16_t ShortAddr);
void znp_zdo_mgmt_lqi_req(uint16_t DstAddr, uint8_t StartIndex);
void znp_zdo_mgmt_leave_req(uint16_t DstAddr, uint64_t DeviceAddr,
                            uint8_t RemoveChildren_Rejoin);
void znp_zdo_permit_join_req(uint16_t DstAddr, uint8_t Duration,
                             uint8_t TCSignificance);
void znp_zdo_startup_from_app(znp_dev_state_t state_to_wait);

void znp_af_register(uint8_t EndPoint);
void znp_af_data_request(af_data_request_t af_data_request);

void znp_set_startopt(uint8_t startopt);
void znp_set_logical_type(uint8_t logical_type);
void znp_permit_join(uint16_t DstAddr);