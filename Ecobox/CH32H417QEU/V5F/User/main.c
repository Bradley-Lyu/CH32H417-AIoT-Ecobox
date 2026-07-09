/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2025/03/01
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/

/*
 *@Note
 polling transceiver mode, master/slave transceiver routine:
 Master:USART2_Tx(PD5)\USART2_Rx(PD6).
 Slave:USART3_Tx(PB10)\USART3_Rx(PB11).
 This example demonstrates sending from USART2 and receiving from USART3.

   Hardware connection:
			   PD5 -- PB11
               PD6 -- PB10

*/

#include "debug.h"
#include "hardware.h"

#define HEARTBEAT_GPIO_PORT GPIOB
#define HEARTBEAT_GPIO_CLK  RCC_HB2Periph_GPIOB
#define HEARTBEAT_GPIO_PIN  GPIO_Pin_0

static void Heartbeat_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_HB2PeriphClockCmd(HEARTBEAT_GPIO_CLK, ENABLE);

    GPIO_InitStructure.GPIO_Pin = HEARTBEAT_GPIO_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(HEARTBEAT_GPIO_PORT, &GPIO_InitStructure);

    GPIO_ResetBits(HEARTBEAT_GPIO_PORT, HEARTBEAT_GPIO_PIN);
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    uint32_t heartbeat_cnt = 0;

	SystemAndCoreClockUpdate();
	Delay_Init();
	USART_Printf_Init(115200);
    Heartbeat_GPIO_Init();
	printf("V5F SystemCoreClk:%d\r\n", SystemCoreClock);
	printf("V5F heartbeat demo started. PB0 will toggle every 500 ms.\r\n");

	while(1)
	{
        GPIO_WriteBit(HEARTBEAT_GPIO_PORT,
                      HEARTBEAT_GPIO_PIN,
                      (heartbeat_cnt & 0x01U) ? Bit_SET : Bit_RESET);
		printf("V5F heartbeat %lu, PB0=%s\r\n",
               (unsigned long)heartbeat_cnt,
               (heartbeat_cnt & 0x01U) ? "HIGH" : "LOW");
		heartbeat_cnt++;
		Delay_Ms(500);
	}
}
