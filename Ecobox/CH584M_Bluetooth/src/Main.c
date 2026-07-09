/********************************** (C) COPYRIGHT *******************************
 * File Name          : Main.c
 * Author             : WCH
 * Version            : V1.0
 * Date               : 2020/08/06
 * Description        : 串口1收发演示
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

#include "CH58x_common.h"
#include "ble_uart_module.h"
#include <string.h>

#define STATUS_LED_PIN             GPIO_Pin_1

static void DebugUart_SendText(const char *text)
{
    UART1_SendString((uint8_t *)text, (uint16_t)strlen(text));
}

/*********************************************************************
 * @fn      main
 *
 * @brief   主函数
 *
 * @return  none
 */
int main()
{
    uint32_t heartbeat_counter = 0;

    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);

    BleUartModule_Init();

    GPIOB_ModeCfg(STATUS_LED_PIN, GPIO_ModeOut_PP_5mA);
    GPIOB_SetBits(STATUS_LED_PIN);

    /* 把 UART1 重映射到 PB12/PB13 */
    GPIOPinRemap(ENABLE, RB_PIN_UART1);
    GPIOB_SetBits(GPIO_Pin_13);
    GPIOB_ModeCfg(GPIO_Pin_12, GPIO_ModeIN_PU);      // RXD1_ -> PB12
    GPIOB_ModeCfg(GPIO_Pin_13, GPIO_ModeOut_PP_5mA); // TXD1_ -> PB13
    UART1_DefInit();

    DebugUart_SendText("\r\nCH584 DEBUG UART BOOT OK\r\n");
    DebugUart_SendText("DEBUG UART: PB12=RX, PB13=TX, 115200\r\n");
    DebugUart_SendText("BLE bridge firmware is running...\r\n");
    DebugUart_SendText("Command output is mirrored on UART1 as hex text.\r\n");

    while(1)
    {
        BleUartModule_Process();

        heartbeat_counter++;
        if(heartbeat_counter >= 2000000UL)
        {
            heartbeat_counter = 0;
            DebugUart_SendText("CH584 HEARTBEAT\r\n");
        }
    }
}

