/********************************** (C) COPYRIGHT  *******************************
* File Name          : hardware.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2025/03/01
* Description        : This file provides all the hardware firmware functions.
*********************************************************************************
* Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.
* Attention: This software (modified or not) and binary are used for
* microcontroller manufactured by Nanjing Qinheng Microelectronics.
*******************************************************************************/
#include "hardware.h"

#define SHT31_I2C                I2C2
#define SHT31_I2C_CLK            RCC_HB1Periph_I2C2
#define SHT31_GPIO_CLK           (RCC_HB2Periph_GPIOC | RCC_HB2Periph_AFIO)
#define SHT31_GPIO_PORT          GPIOC
#define SHT31_SCL_PIN            GPIO_Pin_0
#define SHT31_SDA_PIN            GPIO_Pin_1
#define SHT31_SCL_SOURCE         GPIO_PinSource0
#define SHT31_SDA_SOURCE         GPIO_PinSource1
#define SHT31_GPIO_AF            GPIO_AF9

#define SHT31_ADDR_44            0x44
#define SHT31_ADDR_45            0x45
#define SHT31_CMD_MEASURE_H      0x24
#define SHT31_CMD_MEASURE_L      0x00
#define SHT31_TIMEOUT            0x20000UL

typedef enum
{
    I2C_OK = 0,
    I2C_NO_MEM = 1,
    I2C_BUSY,
    I2C_START,
    I2C_ADDR,
    I2C_TX,
    I2C_RX,
    I2C_CRC
} SHT31_I2C_Error;

static void IIC_Init(u32 bound, u16 address)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2C_InitTypeDef  I2C_InitTSturcture = {0};

    RCC_HB2PeriphClockCmd(SHT31_GPIO_CLK, ENABLE);
    RCC_HB1PeriphClockCmd(SHT31_I2C_CLK, ENABLE);

    GPIO_PinAFConfig(SHT31_GPIO_PORT, SHT31_SCL_SOURCE, SHT31_GPIO_AF);
    GPIO_InitStructure.GPIO_Pin   = SHT31_SCL_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_Init(SHT31_GPIO_PORT, &GPIO_InitStructure);

    GPIO_PinAFConfig(SHT31_GPIO_PORT, SHT31_SDA_SOURCE, SHT31_GPIO_AF);
    GPIO_InitStructure.GPIO_Pin   = SHT31_SDA_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_Init(SHT31_GPIO_PORT, &GPIO_InitStructure);

    I2C_InitTSturcture.I2C_ClockSpeed          = bound;
    I2C_InitTSturcture.I2C_Mode                = I2C_Mode_I2C;
    I2C_InitTSturcture.I2C_DutyCycle           = I2C_DutyCycle_2;
    I2C_InitTSturcture.I2C_OwnAddress1         = address;
    I2C_InitTSturcture.I2C_Ack                 = I2C_Ack_Enable;
    I2C_InitTSturcture.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(SHT31_I2C, &I2C_InitTSturcture);

    I2C_Cmd(SHT31_I2C, ENABLE);
}

static u8 SHT31_CRC8(const u8 *data, u8 len)
{
    u8 crc = 0xFF;
    u8 i;
    u8 bit;

    for(i = 0; i < len; i++)
    {
        crc ^= data[i];
        for(bit = 0; bit < 8; bit++)
        {
            if((crc & 0x80) != 0)
            {
                crc = (u8)((crc << 1) ^ 0x31);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static int I2C2_WaitFlagState(uint32_t flag, FlagStatus state)
{
    uint32_t timeout = SHT31_TIMEOUT;

    while(I2C_GetFlagStatus(SHT31_I2C, flag) != state)
    {
        if(--timeout == 0)
        {
            return 0;
        }
    }

    return 1;
}

static int I2C2_WaitEvent(uint32_t event)
{
    uint32_t timeout = SHT31_TIMEOUT;

    while(!I2C_CheckEvent(SHT31_I2C, event))
    {
        if(I2C_GetFlagStatus(SHT31_I2C, I2C_FLAG_AF) != RESET)
        {
            I2C_ClearFlag(SHT31_I2C, I2C_FLAG_AF);
            I2C_GenerateSTOP(SHT31_I2C, ENABLE);
            return -I2C_ADDR;
        }

        if(--timeout == 0)
        {
            I2C_GenerateSTOP(SHT31_I2C, ENABLE);
            return 0;
        }
    }

    return 1;
}

static int SHT31_StartTx(u8 addr_7bit)
{
    if(!I2C2_WaitFlagState(I2C_FLAG_BUSY, RESET))
    {
        return -I2C_BUSY;
    }

    I2C_GenerateSTART(SHT31_I2C, ENABLE);
    if(I2C2_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) <= 0)
    {
        return -I2C_START;
    }

    I2C_Send7bitAddress(SHT31_I2C, (u8)(addr_7bit << 1), I2C_Direction_Transmitter);
    if(I2C2_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) <= 0)
    {
        return -I2C_ADDR;
    }

    return I2C_OK;
}

static int SHT31_StartRx(u8 addr_7bit)
{
    if(!I2C2_WaitFlagState(I2C_FLAG_BUSY, RESET))
    {
        return -I2C_BUSY;
    }

    I2C_AcknowledgeConfig(SHT31_I2C, ENABLE);
    I2C_GenerateSTART(SHT31_I2C, ENABLE);
    if(I2C2_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT) <= 0)
    {
        return -I2C_START;
    }

    I2C_Send7bitAddress(SHT31_I2C, (u8)(addr_7bit << 1), I2C_Direction_Receiver);
    if(I2C2_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) <= 0)
    {
        return -I2C_ADDR;
    }

    return I2C_OK;
}

static int SHT31_Probe(u8 addr_7bit)
{
    int ret = SHT31_StartTx(addr_7bit);

    I2C_GenerateSTOP(SHT31_I2C, ENABLE);
    return ret;
}

static void I2C2_ScanBus(void)
{
    u8 addr;
    int ret;
    int found = 0;

    printf("I2C scan start\r\n");
    for(addr = 0x08; addr < 0x78; addr++)
    {
        ret = SHT31_Probe(addr);
        if(ret == I2C_OK)
        {
            printf("I2C ACK @ 0x%02X\r\n", addr);
            found = 1;
        }
        Delay_Ms(2);
    }

    if(found == 0)
    {
        printf("I2C scan found no device\r\n");
    }
}

static int SHT31_WriteCommand(u8 addr_7bit, u8 cmd_h, u8 cmd_l)
{
    int ret = SHT31_StartTx(addr_7bit);

    if(ret < 0)
    {
        return ret;
    }

    I2C_SendData(SHT31_I2C, cmd_h);
    if(I2C2_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) <= 0)
    {
        return -I2C_TX;
    }

    I2C_SendData(SHT31_I2C, cmd_l);
    if(I2C2_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) <= 0)
    {
        return -I2C_TX;
    }

    I2C_GenerateSTOP(SHT31_I2C, ENABLE);
    return I2C_OK;
}

static int SHT31_ReadData(u8 addr_7bit, u8 *data, u8 length)
{
    u8 i;
    int ret;

    ret = SHT31_StartRx(addr_7bit);
    if(ret < 0)
    {
        return ret;
    }

    for(i = 0; i < length; i++)
    {
        if(i == (u8)(length - 1))
        {
            I2C_AcknowledgeConfig(SHT31_I2C, DISABLE);
            I2C_GenerateSTOP(SHT31_I2C, ENABLE);
        }

        if(!I2C2_WaitFlagState(I2C_FLAG_RXNE, SET))
        {
            I2C_AcknowledgeConfig(SHT31_I2C, ENABLE);
            I2C_GenerateSTOP(SHT31_I2C, ENABLE);
            return -I2C_RX;
        }

        data[i] = I2C_ReceiveData(SHT31_I2C);
    }

    I2C_AcknowledgeConfig(SHT31_I2C, ENABLE);
    return I2C_OK;
}

static int SHT31_ReadMeasure(u8 addr_7bit, int16_t *temp_x100, u16 *humi_x100)
{
    u8 rx[6] = {0};
    u16 raw_temp;
    u16 raw_humi;
    u8 crc0;
    u8 crc1;
    int ret;

    ret = SHT31_WriteCommand(addr_7bit, SHT31_CMD_MEASURE_H, SHT31_CMD_MEASURE_L);
    if(ret < 0)
    {
        return ret;
    }

    Delay_Ms(20);

    ret = SHT31_ReadData(addr_7bit, rx, sizeof(rx));
    if(ret < 0)
    {
        return ret;
    }

    crc0 = SHT31_CRC8(&rx[0], 2);
    crc1 = SHT31_CRC8(&rx[3], 2);
    if((crc0 != rx[2]) || (crc1 != rx[5]))
    {
        printf("RAW[%02X]: %02X %02X %02X %02X %02X %02X  CRC=%02X/%02X RXCRC=%02X/%02X\r\n",
               addr_7bit,
               rx[0], rx[1], rx[2], rx[3], rx[4], rx[5],
               crc0, crc1, rx[2], rx[5]);
        return -I2C_CRC;
    }

    raw_temp = ((u16)rx[0] << 8) | rx[1];
    raw_humi = ((u16)rx[3] << 8) | rx[4];

    *temp_x100 = (int16_t)((17500L * raw_temp) / 65535L - 4500L);
    *humi_x100 = (u16)((10000UL * raw_humi) / 65535UL);

    return I2C_OK;
}

void Hardware(void)
{
    int ret44;
    int ret45;
    int ret;
    int16_t temp_x100 = 0;
    u16 humi_x100 = 0;
    u8 addr = SHT31_ADDR_44;

    IIC_Init(100000, 0x00);
    Delay_Ms(100);

    printf("SHT31 I2C2 demo start\r\n");
    printf("I2C2_SCL=PC0, I2C2_SDA=PC1\r\n");
    I2C2_ScanBus();

    ret44 = SHT31_Probe(SHT31_ADDR_44);
    ret45 = SHT31_Probe(SHT31_ADDR_45);

    printf("Probe 0x44 ret=%d\r\n", ret44);
    printf("Probe 0x45 ret=%d\r\n", ret45);

    while(1)
    {
        addr = SHT31_ADDR_44;
        ret = SHT31_ReadMeasure(addr, &temp_x100, &humi_x100);
        if(ret < 0)
        {
            addr = SHT31_ADDR_45;
            ret = SHT31_ReadMeasure(addr, &temp_x100, &humi_x100);
        }

        if(ret < 0)
        {
            printf("SHT31 FAIL ret=%d\r\n", ret);
        }
        else
        {
            printf("Addr=0x%02X Temp=%d.%02d C  Humi=%d.%02d %%RH\r\n",
                   addr,
                   temp_x100 / 100,
                   temp_x100 >= 0 ? (temp_x100 % 100) : -(temp_x100 % 100),
                   humi_x100 / 100,
                   humi_x100 % 100);
        }

        Delay_Ms(1000);
    }
}
