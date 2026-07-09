#include "sht30_iic.h"
#include <string.h>

#define SHT30_I2C                I2C2
#define SHT30_I2C_CLK            RCC_HB1Periph_I2C2
#define SHT30_GPIO_PORT          GPIOC
#define SHT30_GPIO_CLK           RCC_HB2Periph_GPIOC
#define SHT30_SCL_PIN            GPIO_Pin_0
#define SHT30_SDA_PIN            GPIO_Pin_1
#define SHT30_SCL_SOURCE         GPIO_PinSource0
#define SHT30_SDA_SOURCE         GPIO_PinSource1
#define SHT30_GPIO_AF            GPIO_AF9

#define I2C_BUFFER_LENGTH        8
#define SHT30_ADDR_44            0x44
#define SHT30_ADDR_45            0x45
#define SHT30_TIMEOUT_MS         200

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
} SHT30_I2C_Error;

static uint8_t g_sht30_last_addr = SHT30_ADDR_44;
static int g_sht30_last_ret = 0;

static void SHT30_IIC_ResetBus(void)
{
    I2C_GenerateSTOP(SHT30_I2C, ENABLE);
    I2C_SoftwareResetCmd(SHT30_I2C, ENABLE);
    Delay_Us(10);
    I2C_SoftwareResetCmd(SHT30_I2C, DISABLE);
    I2C_Cmd(SHT30_I2C, ENABLE);
    I2C_AcknowledgeConfig(SHT30_I2C, ENABLE);
}

static uint8_t SHT30_WaitFlagState(uint32_t flag, FlagStatus state, uint32_t timeout_ms)
{
    uint32_t waited = 0;

    while(I2C_GetFlagStatus(SHT30_I2C, flag) != state)
    {
        Delay_Ms(1);
        waited++;
        if(waited >= timeout_ms)
        {
            return 0;
        }
    }

    return 1;
}

static uint8_t SHT30_WaitEvent(uint32_t event, uint32_t timeout_ms)
{
    uint32_t waited = 0;

    while(I2C_CheckEvent(SHT30_I2C, event) != READY)
    {
        if((I2C_GetFlagStatus(SHT30_I2C, I2C_FLAG_AF) != RESET) ||
           (I2C_GetFlagStatus(SHT30_I2C, I2C_FLAG_BERR) != RESET) ||
           (I2C_GetFlagStatus(SHT30_I2C, I2C_FLAG_ARLO) != RESET))
        {
            return 0;
        }

        Delay_Ms(1);
        waited++;
        if(waited >= timeout_ms)
        {
            return 0;
        }
    }

    return 1;
}

static uint8_t SHT30_CRC8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFF;
    uint8_t i = 0;
    uint8_t bit = 0;

    for(i = 0; i < len; i++)
    {
        crc ^= data[i];
        for(bit = 0; bit < 8; bit++)
        {
            if((crc & 0x80) != 0)
            {
                crc = (uint8_t)((crc << 1) ^ 0x31);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static int I2C_WriteTo(uint8_t addr_7bit, const uint8_t *data, uint8_t length, uint32_t timeout_ms)
{
    uint8_t i = 0;

    if(length > I2C_BUFFER_LENGTH)
    {
        return -I2C_NO_MEM;
    }

    if(SHT30_WaitFlagState(I2C_FLAG_BUSY, RESET, timeout_ms) == 0)
    {
        return -I2C_BUSY;
    }

    I2C_GenerateSTART(SHT30_I2C, ENABLE);
    if(SHT30_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT, timeout_ms) == 0)
    {
        SHT30_IIC_ResetBus();
        return -I2C_START;
    }

    I2C_Send7bitAddress(SHT30_I2C, (uint8_t)(addr_7bit << 1), I2C_Direction_Transmitter);
    if(SHT30_WaitEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED, timeout_ms) == 0)
    {
        SHT30_IIC_ResetBus();
        return -I2C_ADDR;
    }

    for(i = 0; i < length; i++)
    {
        I2C_SendData(SHT30_I2C, data[i]);
        if(SHT30_WaitEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED, timeout_ms) == 0)
        {
            SHT30_IIC_ResetBus();
            return -I2C_TX;
        }
    }

    I2C_GenerateSTOP(SHT30_I2C, ENABLE);
    return I2C_OK;
}

static int I2C_ReadFrom(uint8_t addr_7bit, uint8_t *data, uint8_t length, uint32_t timeout_ms)
{
    uint8_t i = 0;

    if(length > I2C_BUFFER_LENGTH)
    {
        return -I2C_NO_MEM;
    }

    if(SHT30_WaitFlagState(I2C_FLAG_BUSY, RESET, timeout_ms) == 0)
    {
        return -I2C_BUSY;
    }

    I2C_AcknowledgeConfig(SHT30_I2C, ENABLE);
    I2C_GenerateSTART(SHT30_I2C, ENABLE);
    if(SHT30_WaitEvent(I2C_EVENT_MASTER_MODE_SELECT, timeout_ms) == 0)
    {
        SHT30_IIC_ResetBus();
        return -I2C_START;
    }

    I2C_Send7bitAddress(SHT30_I2C, (uint8_t)(addr_7bit << 1), I2C_Direction_Receiver);
    if(SHT30_WaitEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED, timeout_ms) == 0)
    {
        SHT30_IIC_ResetBus();
        return -I2C_ADDR;
    }

    for(i = 0; i < length; i++)
    {
        if(i == (uint8_t)(length - 1))
        {
            I2C_AcknowledgeConfig(SHT30_I2C, DISABLE);
            I2C_GenerateSTOP(SHT30_I2C, ENABLE);
        }

        if(SHT30_WaitFlagState(I2C_FLAG_RXNE, SET, timeout_ms) == 0)
        {
            I2C_AcknowledgeConfig(SHT30_I2C, ENABLE);
            SHT30_IIC_ResetBus();
            return -I2C_RX;
        }

        data[i] = I2C_ReceiveData(SHT30_I2C);
    }

    I2C_AcknowledgeConfig(SHT30_I2C, ENABLE);
    return (int)length;
}

static int SHT30_ReadBytes(uint8_t addr_7bit, uint8_t *rx, uint8_t rx_len)
{
    uint8_t cmd[2] = {0x24, 0x00};
    int ret;

    ret = I2C_WriteTo(addr_7bit, cmd, sizeof(cmd), SHT30_TIMEOUT_MS);
    if(ret < 0)
    {
        return ret;
    }

    Delay_Ms(20);

    ret = I2C_ReadFrom(addr_7bit, rx, rx_len, SHT30_TIMEOUT_MS);
    if(ret < 0)
    {
        return ret;
    }

    if((SHT30_CRC8(&rx[0], 2) != rx[2]) || (SHT30_CRC8(&rx[3], 2) != rx[5]))
    {
        return -I2C_CRC;
    }

    return ret;
}

void SHT30_IIC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2C_InitTypeDef I2C_InitStructure = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | SHT30_GPIO_CLK, ENABLE);
    RCC_HB1PeriphClockCmd(SHT30_I2C_CLK, ENABLE);

    GPIO_PinAFConfig(SHT30_GPIO_PORT, SHT30_SCL_SOURCE, SHT30_GPIO_AF);
    GPIO_PinAFConfig(SHT30_GPIO_PORT, SHT30_SDA_SOURCE, SHT30_GPIO_AF);

    GPIO_InitStructure.GPIO_Pin = SHT30_SCL_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_Init(SHT30_GPIO_PORT, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = SHT30_SDA_PIN;
    GPIO_Init(SHT30_GPIO_PORT, &GPIO_InitStructure);

    I2C_StructInit(&I2C_InitStructure);
    I2C_InitStructure.I2C_ClockSpeed = 100000;
    I2C_InitStructure.I2C_Mode = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle = I2C_DutyCycle_16_9;
    I2C_InitStructure.I2C_OwnAddress1 = 0x42;
    I2C_InitStructure.I2C_Ack = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(SHT30_I2C, &I2C_InitStructure);
    I2C_Cmd(SHT30_I2C, ENABLE);
    I2C_AcknowledgeConfig(SHT30_I2C, ENABLE);

    g_sht30_last_addr = SHT30_ADDR_44;
    g_sht30_last_ret = 0;
}

uint8_t SHT30_ReadTemperatureX10(int32_t *temp_x10)
{
    uint8_t rx[6] = {0};
    uint8_t addr = SHT30_ADDR_44;
    uint16_t raw_temp = 0;
    int ret;

    memset(rx, 0, sizeof(rx));
    ret = SHT30_ReadBytes(addr, rx, sizeof(rx));
    if(ret < 0)
    {
        addr = SHT30_ADDR_45;
        memset(rx, 0, sizeof(rx));
        ret = SHT30_ReadBytes(addr, rx, sizeof(rx));
    }

    g_sht30_last_addr = addr;
    g_sht30_last_ret = ret;

    if(ret < 0)
    {
        return 0;
    }

    raw_temp = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);
    *temp_x10 = -450 + ((1750 * (int32_t)raw_temp + 32767) / 65535);
    return 1;
}

uint8_t SHT30_GetLastAddr(void)
{
    return g_sht30_last_addr;
}

void SHT30_GetLastStatusText(char *text, uint32_t text_size)
{
    const char *code = "OK";

    switch(g_sht30_last_ret)
    {
        case -I2C_BUSY:
            code = "BUS";
            break;
        case -I2C_START:
            code = "STA";
            break;
        case -I2C_ADDR:
            code = "ADR";
            break;
        case -I2C_TX:
            code = "TX";
            break;
        case -I2C_RX:
            code = "RX";
            break;
        case -I2C_CRC:
            code = "CRC";
            break;
        default:
            code = (g_sht30_last_ret < 0) ? "ERR" : "OK";
            break;
    }

    snprintf(text, text_size, "%02X %s", g_sht30_last_addr, code);
}
