/********************************** (C) COPYRIGHT *******************************
 * File Name          : peripheral.C
 * Author             : zhangxiyi @WCH
 * Version            : v0.1
 * Date               : 2020/11/26
 * Description        :
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "CONFIG.h"
#include "devinfoservice.h"
#include "gattprofile.h"
#include "peripheral.h"

#include "ble_uart_service.h"
#include "app_drv_fifo.h"
#include <string.h>

uint8_t Peripheral_TaskID = INVALID_TASK_ID; // Task ID for internal task/event processing

//
static uint8_t to_test_buffer[BLE_BUFF_MAX_LEN - 4 - 3];

//The buffer length should be a power of 2
#define APP_UART_TX_BUFFER_LENGTH    512U
#define APP_UART_RX_BUFFER_LENGTH    2048U

//The tx buffer and rx buffer for app_drv_fifo
//length should be a power of 2
static uint8_t app_uart_tx_buffer[APP_UART_TX_BUFFER_LENGTH] = {0};
static uint8_t app_uart_rx_buffer[APP_UART_RX_BUFFER_LENGTH] = {0};

static app_drv_fifo_t app_uart_tx_fifo;
static app_drv_fifo_t app_uart_rx_fifo;

//interupt uart rx flag ,clear at main loop
bool uart_rx_flag = false;

//for interrupt rx blcak hole ,when uart rx fifo full
uint8_t for_uart_rx_black_hole = 0;

//fifo length less that MTU-3, retry times
uint32_t uart_to_ble_send_evt_cnt = 0;

void app_uart_process(void)
{
    UINT32 irq_status;
    SYS_DisableAllIrq(&irq_status);
    if(uart_rx_flag)
    {
        tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
        uart_rx_flag = false;
    }
    SYS_RecoverIrq(irq_status);

    //tx process
    if(R8_UART1_TFC < UART_FIFO_SIZE)
    {
        app_drv_fifo_read_to_same_addr(&app_uart_tx_fifo, (uint8_t *)&R8_UART1_THR, UART_FIFO_SIZE - R8_UART1_TFC);
    }
}

void app_uart_init()
{
    //tx fifo and tx fifo
    //The buffer length should be a power of 2
    app_drv_fifo_init(&app_uart_tx_fifo, app_uart_tx_buffer, APP_UART_TX_BUFFER_LENGTH);
    app_drv_fifo_init(&app_uart_rx_fifo, app_uart_rx_buffer, APP_UART_RX_BUFFER_LENGTH);

    // Remap UART1 from PA8/PA9 to PB12/PB13 for the H417 bridge link.
    GPIOPinRemap(ENABLE, RB_PIN_UART1);

    //uart tx io
    GPIOB_SetBits(bTXD1_);
    GPIOB_ModeCfg(bTXD1_, GPIO_ModeOut_PP_5mA);

    //uart rx io
    GPIOB_SetBits(bRXD1_);
    GPIOB_ModeCfg(bRXD1_, GPIO_ModeIN_PU);

    //uart1 init
    UART1_DefInit();

    //enable interupt
    UART1_INTCfg(ENABLE, RB_IER_RECV_RDY | RB_IER_LINE_STAT);
    PFIC_EnableIRQ(UART1_IRQn);
}

void app_uart_tx_data(uint8_t *data, uint16_t length)
{
    uint16_t write_length = length;
    app_drv_fifo_write(&app_uart_tx_fifo, data, &write_length);
}
//
//Not every uart reception will end with a UART_II_RECV_TOUT
//UART_II_RECV_TOUT can only be triggered when R8_UARTx_RFC is not 0
//Here we cannot rely UART_II_RECV_TOUT as the end of a uart reception

__INTERRUPT
__HIGH_CODE
void UART1_IRQHandler(void)
{
    uint16_t error;
    switch(UART1_GetITFlag())
    {
        case UART_II_LINE_STAT:
            UART1_GetLinSTA();
            break;

        case UART_II_RECV_RDY:
        case UART_II_RECV_TOUT:
            error = app_drv_fifo_write_from_same_addr(&app_uart_rx_fifo, (uint8_t *)&R8_UART1_RBR, R8_UART1_RFC);
            if(error != APP_DRV_FIFO_RESULT_SUCCESS)
            {
                for(uint8_t i = 0; i < R8_UART1_RFC; i++)
                {
                    //fifo full,put to fifo black hole
                    for_uart_rx_black_hole = R8_UART1_RBR;
                }
            }
            uart_rx_flag = true;
            break;

        case UART_II_THR_EMPTY:
            break;
        case UART_II_MODEM_CHG:
            break;
        default:
            break;
    }
}

#define H417_PACKET_END                 0x5A
#define H417_GROUP_ACTUATOR             0xA5
#define H417_GROUP_FEEDER               0xA4
#define H417_GROUP_LIGHT                0xA3

#define H417_CMD_PUMP_ON                0x01
#define H417_CMD_PUMP_OFF               0x02
#define H417_CMD_FAN_ON                 0x03
#define H417_CMD_FAN_OFF                0x04
#define H417_CMD_UVB_ON                 0x05
#define H417_CMD_UVB_OFF                0x06
#define H417_CMD_FEEDER_OPEN            0x01
#define H417_CMD_FEEDER_RESET           0x02
#define H417_CMD_FEEDER_OPEN_RESET      0x03
#define H417_CMD_LIGHTEAST_SET          0x11
#define H417_CMD_LIGHTWEST_SET          0x12

static void ble_cmd_trim(char *text)
{
    uint16_t length = (uint16_t)strlen(text);
    uint16_t start = 0;
    uint16_t end = length;
    uint16_t i = 0;

    while((start < length) &&
          ((text[start] == ' ') || (text[start] == '\r') || (text[start] == '\n') || (text[start] == '\t')))
    {
        start++;
    }

    while((end > start) &&
          ((text[end - 1] == ' ') || (text[end - 1] == '\r') || (text[end - 1] == '\n') || (text[end - 1] == '\t')))
    {
        end--;
    }

    if(start > 0)
    {
        for(i = 0; (start + i) < end; i++)
        {
            text[i] = text[start + i];
        }
        text[i] = '\0';
    }
    else
    {
        text[end] = '\0';
    }
}

static void ble_cmd_to_lowercase(char *text)
{
    while(*text != '\0')
    {
        if((*text >= 'A') && (*text <= 'Z'))
        {
            *text = (char)(*text - 'A' + 'a');
        }
        text++;
    }
}

static uint8_t ble_cmd_parse_u8(const char *text, uint8_t *value)
{
    uint16_t parsed = 0;
    uint8_t has_digit = FALSE;

    while(*text != '\0')
    {
        if((*text < '0') || (*text > '9'))
        {
            return FALSE;
        }

        parsed = (uint16_t)(parsed * 10u + (uint16_t)(*text - '0'));
        if(parsed > 255u)
        {
            return FALSE;
        }

        has_digit = TRUE;
        text++;
    }

    if(!has_digit)
    {
        return FALSE;
    }

    *value = (uint8_t)parsed;
    return TRUE;
}

static void h417_send_packet(const uint8_t *packet, uint8_t length)
{
    uint8_t i = 0;

    PRINT("UART1 CMD OUT:");
    for(i = 0; i < length; i++)
    {
        PRINT(" %02X", packet[i]);
    }
    PRINT("\r\n");

    app_uart_tx_data((uint8_t *)packet, length);
}

static uint8_t h417_build_packet_from_ble(const uint8_t *ble_data, uint16_t ble_length,
                                          uint8_t *packet, uint8_t *packet_length)
{
    char command[64];
    uint16_t copy_length = ble_length;
    uint8_t value = 0;

    if(copy_length >= sizeof(command))
    {
        copy_length = sizeof(command) - 1;
    }

    tmos_memcpy(command, ble_data, copy_length);
    command[copy_length] = '\0';

    ble_cmd_trim(command);
    ble_cmd_to_lowercase(command);

    if(command[0] == '\0')
    {
        return FALSE;
    }

    if(strcmp(command, "pump:on") == 0)
    {
        packet[0] = H417_GROUP_ACTUATOR;
        packet[1] = H417_CMD_PUMP_ON;
        packet[2] = H417_PACKET_END;
        *packet_length = 3;
        return TRUE;
    }

    if(strcmp(command, "pump:off") == 0)
    {
        packet[0] = H417_GROUP_ACTUATOR;
        packet[1] = H417_CMD_PUMP_OFF;
        packet[2] = H417_PACKET_END;
        *packet_length = 3;
        return TRUE;
    }

    if(strcmp(command, "fan:on") == 0)
    {
        packet[0] = H417_GROUP_ACTUATOR;
        packet[1] = H417_CMD_FAN_ON;
        packet[2] = H417_PACKET_END;
        *packet_length = 3;
        return TRUE;
    }

    if(strcmp(command, "fan:off") == 0)
    {
        packet[0] = H417_GROUP_ACTUATOR;
        packet[1] = H417_CMD_FAN_OFF;
        packet[2] = H417_PACKET_END;
        *packet_length = 3;
        return TRUE;
    }

    if(strcmp(command, "uvb:on") == 0)
    {
        packet[0] = H417_GROUP_ACTUATOR;
        packet[1] = H417_CMD_UVB_ON;
        packet[2] = H417_PACKET_END;
        *packet_length = 3;
        return TRUE;
    }

    if(strcmp(command, "uvb:off") == 0)
    {
        packet[0] = H417_GROUP_ACTUATOR;
        packet[1] = H417_CMD_UVB_OFF;
        packet[2] = H417_PACKET_END;
        *packet_length = 3;
        return TRUE;
    }

    if((strcmp(command, "feeder:open") == 0) || (strcmp(command, "feeder:on") == 0))
    {
        packet[0] = H417_GROUP_FEEDER;
        packet[1] = H417_CMD_FEEDER_OPEN;
        packet[2] = H417_PACKET_END;
        *packet_length = 3;
        return TRUE;
    }

    if((strcmp(command, "feeder:reset") == 0) || (strcmp(command, "feeder:off") == 0))
    {
        packet[0] = H417_GROUP_FEEDER;
        packet[1] = H417_CMD_FEEDER_RESET;
        packet[2] = H417_PACKET_END;
        *packet_length = 3;
        return TRUE;
    }

    if((strcmp(command, "feeder:feed") == 0) ||
       (strcmp(command, "feeder:feed:1") == 0) ||
       (strcmp(command, "turtle:hungry") == 0) ||
       (strcmp(command, "wugui:ele") == 0) ||
       (strcmp(command, "乌龟饿了") == 0))
    {
        packet[0] = H417_GROUP_FEEDER;
        packet[1] = H417_CMD_FEEDER_OPEN_RESET;
        packet[2] = H417_PACKET_END;
        *packet_length = 3;
        return TRUE;
    }

    if(strncmp(command, "feeder:feed:", 12) == 0)
    {
        if(!ble_cmd_parse_u8(command + 12, &value))
        {
            return FALSE;
        }

        packet[0] = H417_GROUP_FEEDER;
        packet[1] = H417_CMD_FEEDER_OPEN_RESET;
        packet[2] = value;
        packet[3] = H417_PACKET_END;
        *packet_length = 4;
        return TRUE;
    }

    if((strcmp(command, "lighteast:on") == 0) || (strcmp(command, "le:on") == 0))
    {
        packet[0] = H417_GROUP_LIGHT;
        packet[1] = H417_CMD_LIGHTEAST_SET;
        packet[2] = 100;
        packet[3] = H417_PACKET_END;
        *packet_length = 4;
        return TRUE;
    }

    if((strcmp(command, "lighteast:off") == 0) || (strcmp(command, "le:off") == 0))
    {
        packet[0] = H417_GROUP_LIGHT;
        packet[1] = H417_CMD_LIGHTEAST_SET;
        packet[2] = 0;
        packet[3] = H417_PACKET_END;
        *packet_length = 4;
        return TRUE;
    }

    if(strncmp(command, "lighteast:brightness:", 21) == 0)
    {
        if(!ble_cmd_parse_u8(command + 21, &value))
        {
            return FALSE;
        }

        packet[0] = H417_GROUP_LIGHT;
        packet[1] = H417_CMD_LIGHTEAST_SET;
        packet[2] = value;
        packet[3] = H417_PACKET_END;
        *packet_length = 4;
        return TRUE;
    }

    if(strncmp(command, "le:", 3) == 0)
    {
        if(!ble_cmd_parse_u8(command + 3, &value))
        {
            return FALSE;
        }

        packet[0] = H417_GROUP_LIGHT;
        packet[1] = H417_CMD_LIGHTEAST_SET;
        packet[2] = value;
        packet[3] = H417_PACKET_END;
        *packet_length = 4;
        return TRUE;
    }

    if((strcmp(command, "lightwest:on") == 0) || (strcmp(command, "lw:on") == 0))
    {
        packet[0] = H417_GROUP_LIGHT;
        packet[1] = H417_CMD_LIGHTWEST_SET;
        packet[2] = 100;
        packet[3] = H417_PACKET_END;
        *packet_length = 4;
        return TRUE;
    }

    if((strcmp(command, "lightwest:off") == 0) || (strcmp(command, "lw:off") == 0))
    {
        packet[0] = H417_GROUP_LIGHT;
        packet[1] = H417_CMD_LIGHTWEST_SET;
        packet[2] = 0;
        packet[3] = H417_PACKET_END;
        *packet_length = 4;
        return TRUE;
    }

    if(strncmp(command, "lightwest:brightness:", 21) == 0)
    {
        if(!ble_cmd_parse_u8(command + 21, &value))
        {
            return FALSE;
        }

        packet[0] = H417_GROUP_LIGHT;
        packet[1] = H417_CMD_LIGHTWEST_SET;
        packet[2] = value;
        packet[3] = H417_PACKET_END;
        *packet_length = 4;
        return TRUE;
    }

    if(strncmp(command, "lw:", 3) == 0)
    {
        if(!ble_cmd_parse_u8(command + 3, &value))
        {
            return FALSE;
        }

        packet[0] = H417_GROUP_LIGHT;
        packet[1] = H417_CMD_LIGHTWEST_SET;
        packet[2] = value;
        packet[3] = H417_PACKET_END;
        *packet_length = 4;
        return TRUE;
    }

    return FALSE;
}

//ble uart service callback handler
void on_bleuartServiceEvt(uint16_t connection_handle, ble_uart_evt_t *p_evt)
{
    uint8_t h417_packet[4] = {0};
    uint8_t h417_packet_length = 0;
    uint16_t i = 0;
    uint16_t to_write_length = 0;

    switch(p_evt->type)
    {
        case BLE_UART_EVT_TX_NOTI_DISABLED:
            PRINT("BLE notify disabled, conn=%02x\r\n", connection_handle);
            break;
        case BLE_UART_EVT_TX_NOTI_ENABLED:
            PRINT("BLE notify enabled, conn=%02x\r\n", connection_handle);
            break;
        case BLE_UART_EVT_BLE_DATA_RECIEVED:
            PRINT("BLE RX DATA len:%d\r\n", p_evt->data.length);

            PRINT("BLE RX HEX:");
            for(i = 0; i < p_evt->data.length; i++)
            {
                PRINT(" %02X", p_evt->data.p_data[i]);
            }
            PRINT("\r\n");

            //for notify back test
            //to ble
            to_write_length = p_evt->data.length;
            app_drv_fifo_write(&app_uart_rx_fifo, (uint8_t *)p_evt->data.p_data, &to_write_length);
            tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
            //end of nofify back test

            if(h417_build_packet_from_ble((uint8_t *)p_evt->data.p_data, p_evt->data.length,
                                          h417_packet, &h417_packet_length))
            {
                h417_send_packet(h417_packet, h417_packet_length);
            }
            else
            {
                PRINT("Unsupported BLE command for H417 bridge\r\n");
            }

            break;
        default:
            break;
    }
}

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * CONSTANTS
 */

// How often to perform periodic event
#define SBP_PERIODIC_EVT_PERIOD              1600

// How often to perform read rssi event
#define SBP_READ_RSSI_EVT_PERIOD             3200

// Parameter update delay
#define SBP_PARAM_UPDATE_DELAY               6400

// What is the advertising interval when device is discoverable (units of 625us, 80=50ms)
#define DEFAULT_ADVERTISING_INTERVAL         160

// Limited discoverable mode advertises for 30.72s, and then stops
// General discoverable mode advertises indefinitely
#define DEFAULT_DISCOVERABLE_MODE            GAP_ADTYPE_FLAGS_GENERAL

// Minimum connection interval (units of 1.25ms, 10=12.5ms)
#define DEFAULT_DESIRED_MIN_CONN_INTERVAL    8

// Maximum connection interval (units of 1.25ms, 100=125ms)
#define DEFAULT_DESIRED_MAX_CONN_INTERVAL    20

// Slave latency to use parameter update
#define DEFAULT_DESIRED_SLAVE_LATENCY        0

// Supervision timeout value (units of 10ms, 100=1s)
#define DEFAULT_DESIRED_CONN_TIMEOUT         100

// Company Identifier: WCH
#define WCH_COMPANY_ID                       0x07D7

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL VARIABLES
 */

/*********************************************************************
 * EXTERNAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */

//for send to ble
typedef enum
{
    SEND_TO_BLE_TO_SEND = 1,
    SEND_TO_BLE_ALLOC_FAILED,
    SEND_TO_BLE_SEND_FAILED,
} send_to_ble_state_t;
send_to_ble_state_t send_to_ble_state = SEND_TO_BLE_TO_SEND;

blePaControlConfig_t pa_lna_ctl;

//static uint8_t Peripheral_TaskID = INVALID_TASK_ID;   // Task ID for internal task/event processing

// GAP - SCAN RSP data (max size = 31 bytes)
static uint8_t scanRspData[] = {
    // complete name
    15, // length of this data
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'C', 'H', '5', '8', '4', '_', 'B', 'L', 'E', '_', 'C', 'T', 'R', 'L',
    // connection interval range
    0x05, // length of this data
    GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE,
    LO_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL), // 100ms
    HI_UINT16(DEFAULT_DESIRED_MIN_CONN_INTERVAL),
    LO_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL), // 1s
    HI_UINT16(DEFAULT_DESIRED_MAX_CONN_INTERVAL),

    // Tx power level
    0x02, // length of this data
    GAP_ADTYPE_POWER_LEVEL,
    0 // 0dBm
};

// GAP - Advertisement data (max size = 31 bytes, though this is
// best kept short to conserve power while advertisting)
static uint8_t advertData[] = {
    // Flags; this sets the device to use limited discoverable
    // mode (advertises for 30 seconds at a time) instead of general
    // discoverable mode (advertises indefinitely)
    0x02, // length of this data
    GAP_ADTYPE_FLAGS,
    DEFAULT_DISCOVERABLE_MODE | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,

    // service UUID, to notify central devices what services are included
    // in this peripheral
    0x03,                  // length of this data
    GAP_ADTYPE_16BIT_MORE, // some of the UUID's, but not all
    LO_UINT16(SIMPLEPROFILE_SERV_UUID),
    HI_UINT16(SIMPLEPROFILE_SERV_UUID)};

// GAP GATT Attributes
static uint8_t attDeviceName[GAP_DEVICE_NAME_LEN] = "CH584_BLE_CTRL";

// Connection item list
static peripheralConnItem_t peripheralConnList;

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void Peripheral_ProcessTMOSMsg(tmos_event_hdr_t *pMsg);
static void peripheralStateNotificationCB(gapRole_States_t newState, gapRoleEvent_t *pEvent);

static void peripheralParamUpdateCB(uint16_t connHandle, uint16_t connInterval,
                                    uint16_t connSlaveLatency, uint16_t connTimeout);
static void peripheralInitConnItem(peripheralConnItem_t *peripheralConnList);
static void peripheralRssiCB(uint16_t connHandle, int8_t rssi);

/*********************************************************************
 * PROFILE CALLBACKS
 */

// GAP Role Callbacks
static gapRolesCBs_t Peripheral_PeripheralCBs = {
    peripheralStateNotificationCB, // Profile State Change Callbacks
    peripheralRssiCB,              // When a valid RSSI is read from controller (not used by application)
    peripheralParamUpdateCB};

// Broadcast Callbacks
static gapRolesBroadcasterCBs_t Broadcaster_BroadcasterCBs = {
    NULL, // Not used in peripheral role
    NULL  // Receive scan request callback
};

// GAP Bond Manager Callbacks
static gapBondCBs_t Peripheral_BondMgrCBs = {
    NULL, // Passcode callback (not used by application)
    NULL  // Pairing / Bonding state Callback (not used by application)
};

/*********************************************************************
 * PUBLIC FUNCTIONS
 */

/*********************************************************************
 * @fn      Peripheral_Init
 *
 * @brief   Initialization function for the Peripheral App Task.
 *          This is called during initialization and should contain
 *          any application specific initialization (ie. hardware
 *          initialization/setup, table initialization, power up
 *          notificaiton ... ).
 *
 * @param   task_id - the ID assigned by TMOS.  This ID should be
 *                    used to send messages and set timers.
 *
 * @return  none
 */
void Peripheral_Init()
{
    Peripheral_TaskID = TMOS_ProcessEventRegister(Peripheral_ProcessEvent);

    // Setup the GAP Peripheral Role Profile
    {
        uint8_t  initial_advertising_enable = TRUE;
        uint16_t desired_min_interval = 6;
        uint16_t desired_max_interval = 1000;

        // Set the GAP Role Parameters
        GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &initial_advertising_enable);
        GAPRole_SetParameter(GAPROLE_SCAN_RSP_DATA, sizeof(scanRspData), scanRspData);
        GAPRole_SetParameter(GAPROLE_ADVERT_DATA, sizeof(advertData), advertData);
        GAPRole_SetParameter(GAPROLE_MIN_CONN_INTERVAL, sizeof(uint16_t), &desired_min_interval);
        GAPRole_SetParameter(GAPROLE_MAX_CONN_INTERVAL, sizeof(uint16_t), &desired_max_interval);
    }

    // Set advertising interval
    {
        uint16_t advInt = DEFAULT_ADVERTISING_INTERVAL;

        GAP_SetParamValue(TGAP_DISC_ADV_INT_MIN, advInt);
        GAP_SetParamValue(TGAP_DISC_ADV_INT_MAX, advInt);
    }

    // Setup the GAP Bond Manager
    {
        uint32_t passkey = 0; // passkey "000000"
        uint8_t  pairMode = GAPBOND_PAIRING_MODE_NO_PAIRING;
        uint8_t  mitm = FALSE;
        uint8_t  bonding = FALSE;
        uint8_t  ioCap = GAPBOND_IO_CAP_NO_INPUT_NO_OUTPUT;
        GAPBondMgr_SetParameter(GAPBOND_PERI_DEFAULT_PASSCODE, sizeof(uint32_t), &passkey);
        GAPBondMgr_SetParameter(GAPBOND_PERI_PAIRING_MODE, sizeof(uint8_t), &pairMode);
        GAPBondMgr_SetParameter(GAPBOND_PERI_MITM_PROTECTION, sizeof(uint8_t), &mitm);
        GAPBondMgr_SetParameter(GAPBOND_PERI_IO_CAPABILITIES, sizeof(uint8_t), &ioCap);
        GAPBondMgr_SetParameter(GAPBOND_PERI_BONDING_ENABLED, sizeof(uint8_t), &bonding);
    }

    // Initialize GATT attributes
    GGS_AddService(GATT_ALL_SERVICES);         // GAP
    GATTServApp_AddService(GATT_ALL_SERVICES); // GATT attributes
    DevInfo_AddService();                      // Device Information Service
    ble_uart_add_service(on_bleuartServiceEvt);

    // Set the GAP Characteristics
    GGS_SetParameter(GGS_DEVICE_NAME_ATT, GAP_DEVICE_NAME_LEN, attDeviceName);

    // Init Connection Item
    peripheralInitConnItem(&peripheralConnList);

    // Register receive scan request callback
    GAPRole_BroadcasterSetCB(&Broadcaster_BroadcasterCBs);

    // Setup a delayed profile startup
    tmos_set_event(Peripheral_TaskID, SBP_START_DEVICE_EVT);
}

/*********************************************************************
 * @fn      peripheralInitConnItem
 *
 * @brief   Init Connection Item
 *
 * @param   peripheralConnList -
 *
 * @return  NULL
 */
static void peripheralInitConnItem(peripheralConnItem_t *peripheralConnList)
{
    peripheralConnList->connHandle = GAP_CONNHANDLE_INIT;
    peripheralConnList->connInterval = 0;
    peripheralConnList->connSlaveLatency = 0;
    peripheralConnList->connTimeout = 0;
}

uint32_t get_fattime(void)
{
    return 0;
}

/*********************************************************************
 * @fn      Peripheral_ProcessEvent
 *
 * @brief   Peripheral Application Task event processor.  This function
 *          is called to process all events for the task.  Events
 *          include timers, messages and any other user defined events.
 *
 * @param   task_id - The TMOS assigned task ID.
 * @param   events - events to process.  This is a bit map and can
 *                   contain more than one event.
 *
 * @return  events not processed
 */
uint16_t Peripheral_ProcessEvent(uint8_t task_id, uint16_t events)
{
    static attHandleValueNoti_t noti;
    //  VOID task_id; // TMOS required parameter that isn't used in this function

    if(events & SYS_EVENT_MSG)
    {
        uint8_t *pMsg;

        if((pMsg = tmos_msg_receive(Peripheral_TaskID)) != NULL)
        {
            Peripheral_ProcessTMOSMsg((tmos_event_hdr_t *)pMsg);
            // Release the TMOS message
            tmos_msg_deallocate(pMsg);
        }
        // return unprocessed events
        return (events ^ SYS_EVENT_MSG);
    }

    if(events & SBP_START_DEVICE_EVT)
    {
        // Start the Device
        GAPRole_PeripheralStartDevice(Peripheral_TaskID, &Peripheral_BondMgrCBs, &Peripheral_PeripheralCBs);
        return (events ^ SBP_START_DEVICE_EVT);
    }
    if(events & SBP_PARAM_UPDATE_EVT)
    {
        // Send connect param update request
        GAPRole_PeripheralConnParamUpdateReq(peripheralConnList.connHandle,
                                             DEFAULT_DESIRED_MIN_CONN_INTERVAL,
                                             DEFAULT_DESIRED_MAX_CONN_INTERVAL,
                                             DEFAULT_DESIRED_SLAVE_LATENCY,
                                             DEFAULT_DESIRED_CONN_TIMEOUT,
                                             Peripheral_TaskID);

        //        GAPRole_PeripheralConnParamUpdateReq( peripheralConnList.connHandle,
        //                                              10,
        //                                              20,
        //                                              0,
        //                                              400,
        //                                              Peripheral_TaskID);

        return (events ^ SBP_PARAM_UPDATE_EVT);
    }

    if(events & UART_TO_BLE_SEND_EVT)
    {
        static uint16_t read_length = 0;
        ;
        uint8_t result = 0xff;
        switch(send_to_ble_state)
        {
            case SEND_TO_BLE_TO_SEND:

                //notify is not enabled
                if(!ble_uart_notify_is_ready(peripheralConnList.connHandle))
                {
                    if(peripheralConnList.connHandle == GAP_CONNHANDLE_INIT)
                    {
                        //connection lost, flush rx fifo here
                        app_drv_fifo_flush(&app_uart_rx_fifo);
                    }
                    break;
                }
                read_length = ATT_GetMTU(peripheralConnList.connHandle) - 3;

                if(app_drv_fifo_length(&app_uart_rx_fifo) >= read_length)
                {
                    PRINT("FIFO_LEN:%d\r\n", app_drv_fifo_length(&app_uart_rx_fifo));
                    result = app_drv_fifo_read(&app_uart_rx_fifo, to_test_buffer, &read_length);
                    uart_to_ble_send_evt_cnt = 0;
                }
                else
                {
                    if(uart_to_ble_send_evt_cnt > 10)
                    {
                        result = app_drv_fifo_read(&app_uart_rx_fifo, to_test_buffer, &read_length);
                        uart_to_ble_send_evt_cnt = 0;
                    }
                    else
                    {
                        tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 4);
                        uart_to_ble_send_evt_cnt++;
                        PRINT("NO TIME OUT\r\n");
                    }
                }

                if(APP_DRV_FIFO_RESULT_SUCCESS == result)
                {
                    noti.len = read_length;
                    noti.pValue = GATT_bm_alloc(peripheralConnList.connHandle, ATT_HANDLE_VALUE_NOTI, noti.len, NULL, 0);
                    if(noti.pValue != NULL)
                    {
                        tmos_memcpy(noti.pValue, to_test_buffer, noti.len);
                        result = ble_uart_notify(peripheralConnList.connHandle, &noti, 0);
                        if(result != SUCCESS)
                        {
                            PRINT("R1:%02x\r\n", result);
                            send_to_ble_state = SEND_TO_BLE_SEND_FAILED;
                            GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
                            tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                        }
                        else
                        {
                            send_to_ble_state = SEND_TO_BLE_TO_SEND;
                            //app_fifo_write(&app_uart_tx_fifo,to_test_buffer,&read_length);
                            //app_drv_fifo_write(&app_uart_tx_fifo,to_test_buffer,&read_length);
                            read_length = 0;
                            tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                        }
                    }
                    else
                    {
                        send_to_ble_state = SEND_TO_BLE_ALLOC_FAILED;
                        tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                    }
                }
                else
                {
                    //send_to_ble_state = SEND_TO_BLE_FIFO_EMPTY;
                }
                break;
            case SEND_TO_BLE_ALLOC_FAILED:
            case SEND_TO_BLE_SEND_FAILED:

                noti.len = read_length;
                noti.pValue = GATT_bm_alloc(peripheralConnList.connHandle, ATT_HANDLE_VALUE_NOTI, noti.len, NULL, 0);
                if(noti.pValue != NULL)
                {
                    tmos_memcpy(noti.pValue, to_test_buffer, noti.len);
                    result = ble_uart_notify(peripheralConnList.connHandle, &noti, 0);
                    if(result != SUCCESS)
                    {
                        PRINT("R2:%02x\r\n", result);
                        send_to_ble_state = SEND_TO_BLE_SEND_FAILED;
                        GATT_bm_free((gattMsg_t *)&noti, ATT_HANDLE_VALUE_NOTI);
                        tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                    }
                    else
                    {
                        send_to_ble_state = SEND_TO_BLE_TO_SEND;
                        //app_drv_fifo_write(&app_uart_tx_fifo,to_test_buffer,&read_length);
                        read_length = 0;
                        tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                    }
                }
                else
                {
                    send_to_ble_state = SEND_TO_BLE_ALLOC_FAILED;
                    tmos_start_task(Peripheral_TaskID, UART_TO_BLE_SEND_EVT, 2);
                }
                break;
            default:
                break;
        }
        return (events ^ UART_TO_BLE_SEND_EVT);
    }
    // Discard unknown events
    return 0;
}

/*********************************************************************
 * @fn      Peripheral_ProcessTMOSMsg
 *
 * @brief   Process an incoming task message.
 *
 * @param   pMsg - message to process
 *
 * @return  none
 */
static void Peripheral_ProcessTMOSMsg(tmos_event_hdr_t *pMsg)
{
    switch(pMsg->event)
    {
        default:
            break;
    }
}

/*********************************************************************
 * @fn      Peripheral_LinkEstablished
 *
 * @brief   Process link established.
 *
 * @param   pEvent - event to process
 *
 * @return  none
 */
static void Peripheral_LinkEstablished(gapRoleEvent_t *pEvent)
{
    gapEstLinkReqEvent_t *event = (gapEstLinkReqEvent_t *)pEvent;

    // See if already connected
    if(peripheralConnList.connHandle != GAP_CONNHANDLE_INIT)
    {
        GAPRole_TerminateLink(pEvent->linkCmpl.connectionHandle);
        PRINT("Connection max...\n");
    }
    else
    {
        peripheralConnList.connHandle = event->connectionHandle;
        peripheralConnList.connInterval = event->connInterval;
        peripheralConnList.connSlaveLatency = event->connLatency;
        peripheralConnList.connTimeout = event->connTimeout;

        PRINT("Conn %x - Int %x \n", event->connectionHandle, event->connInterval);
        PRINT("BLE LINK OK, conn=%x interval=%x\r\n", event->connectionHandle, event->connInterval);
    }
}

/*********************************************************************
 * @fn      Peripheral_LinkTerminated
 *
 * @brief   Process link terminated.
 *
 * @param   pEvent - event to process
 *
 * @return  none
 */
static void Peripheral_LinkTerminated(gapRoleEvent_t *pEvent)
{
    gapTerminateLinkEvent_t *event = (gapTerminateLinkEvent_t *)pEvent;

    if(event->connectionHandle == peripheralConnList.connHandle)
    {
        peripheralConnList.connHandle = GAP_CONNHANDLE_INIT;
        peripheralConnList.connInterval = 0;
        peripheralConnList.connSlaveLatency = 0;
        peripheralConnList.connTimeout = 0;

        // Restart advertising
        {
            uint8_t advertising_enable = TRUE;
            GAPRole_SetParameter(GAPROLE_ADVERT_ENABLED, sizeof(uint8_t), &advertising_enable);
        }
    }
    else
    {
        PRINT("ERR..\n");
    }
}

/*********************************************************************
 * @fn      peripheralRssiCB
 *
 * @brief   RSSI callback.
 *
 * @param   connHandle - connection handle
 * @param   rssi - RSSI
 *
 * @return  none
 */
static void peripheralRssiCB(uint16_t connHandle, int8_t rssi)
{
    PRINT("RSSI -%d dB Conn  %x \n", -rssi, connHandle);
}

/*********************************************************************
 * @fn      peripheralParamUpdateCB
 *
 * @brief   Parameter update complete callback
 *
 * @param   connHandle - connect handle
 *          connInterval - connect interval
 *          connSlaveLatency - connect slave latency
 *          connTimeout - connect timeout
 *
 * @return  none
 */
static void peripheralParamUpdateCB(uint16_t connHandle, uint16_t connInterval,
                                    uint16_t connSlaveLatency, uint16_t connTimeout)
{
    if(connHandle == peripheralConnList.connHandle)
    {
        peripheralConnList.connInterval = connInterval;
        peripheralConnList.connSlaveLatency = connSlaveLatency;
        peripheralConnList.connTimeout = connTimeout;

        PRINT("Update %x - Int %x \n", connHandle, connInterval);
    }
    else
    {
        PRINT("peripheralParamUpdateCB err..\n");
    }
}

/*********************************************************************
 * @fn      peripheralStateNotificationCB
 *
 * @brief   Notification from the profile of a state change.
 *
 * @param   newState - new state
 *
 * @return  none
 */
static void peripheralStateNotificationCB(gapRole_States_t newState, gapRoleEvent_t *pEvent)
{
    switch(newState & GAPROLE_STATE_ADV_MASK)
    {
        case GAPROLE_STARTED:
            PRINT("BLE Initialized..\n");
            break;

        case GAPROLE_ADVERTISING:
            if(pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT)
            {
                Peripheral_LinkTerminated(pEvent);
            }
            PRINT("BLE Advertising..\n");
            break;

        case GAPROLE_CONNECTED:
            if(pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                Peripheral_LinkEstablished(pEvent);
                PRINT("BLE Connected..\n");
            }
            break;

        case GAPROLE_CONNECTED_ADV:
            PRINT("BLE Connected Advertising..\n");
            break;

        case GAPROLE_WAITING:
            if(pEvent->gap.opcode == GAP_END_DISCOVERABLE_DONE_EVENT)
            {
                PRINT("BLE Waiting for advertising..\n");
            }
            else if(pEvent->gap.opcode == GAP_LINK_TERMINATED_EVENT)
            {
                Peripheral_LinkTerminated(pEvent);
                PRINT("BLE Disconnected.. Reason:%x\n", pEvent->linkTerminate.reason);
            }
            else if(pEvent->gap.opcode == GAP_LINK_ESTABLISHED_EVENT)
            {
                if(pEvent->gap.hdr.status != SUCCESS)
                {
                    PRINT("BLE Waiting for advertising..\n");
                }
                else
                {
                    PRINT("BLE Error..\n");
                }
            }
            else
            {
                PRINT("BLE Error..%x\n", pEvent->gap.opcode);
            }
            break;

        case GAPROLE_ERROR:
            PRINT("BLE Error..\n");
            break;

        default:
            break;
    }
}

/*********************************************************************
*********************************************************************/

