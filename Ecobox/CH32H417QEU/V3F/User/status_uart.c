#include "status_uart.h"
#include "usart.h"
#include <stdarg.h>
#include <stdio.h>

#define STATUS_USART               USART3
#define STATUS_USART_CLK           RCC_HB1Periph_USART3
#define STATUS_GPIO_PORT           GPIOB
#define STATUS_GPIO_CLK            RCC_HB2Periph_GPIOB
#define STATUS_TX_PIN              GPIO_Pin_10
#define STATUS_RX_PIN              GPIO_Pin_11
#define STATUS_TX_PIN_SOURCE       GPIO_PinSource10
#define STATUS_RX_PIN_SOURCE       GPIO_PinSource11
#define STATUS_GPIO_AF             GPIO_AF7
#define STATUS_RX_BUFFER_SIZE      128
#define STATUS_RX_BUFFER_MASK      (STATUS_RX_BUFFER_SIZE - 1)

static volatile uint8_t s_rx_buffer[STATUS_RX_BUFFER_SIZE];
static volatile uint8_t s_rx_head = 0;
static volatile uint8_t s_rx_tail = 0;

void USART3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static uint8_t StatusUart_IsGroupByte(uint8_t data)
{
    return (data == SCREEN_GROUP_ACTUATOR) ||
           (data == SCREEN_GROUP_FEEDER) ||
           (data == SCREEN_GROUP_LIGHT);
}

static void StatusUart_PushRxByte(uint8_t data)
{
    uint8_t next_head = (uint8_t)((s_rx_head + 1) & STATUS_RX_BUFFER_MASK);

    if(next_head == s_rx_tail)
    {
        s_rx_tail = (uint8_t)((s_rx_tail + 1) & STATUS_RX_BUFFER_MASK);
    }

    s_rx_buffer[s_rx_head] = data;
    s_rx_head = next_head;
}

void StatusUart_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio_init = {0};
    USART_InitTypeDef usart_init = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | STATUS_GPIO_CLK, ENABLE);
    RCC_HB1PeriphClockCmd(STATUS_USART_CLK, ENABLE);

    GPIO_PinAFConfig(STATUS_GPIO_PORT, STATUS_TX_PIN_SOURCE, STATUS_GPIO_AF);
    GPIO_PinAFConfig(STATUS_GPIO_PORT, STATUS_RX_PIN_SOURCE, STATUS_GPIO_AF);

    gpio_init.GPIO_Pin = STATUS_TX_PIN;
    gpio_init.GPIO_Speed = GPIO_Speed_Very_High;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(STATUS_GPIO_PORT, &gpio_init);

    gpio_init.GPIO_Pin = STATUS_RX_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(STATUS_GPIO_PORT, &gpio_init);

    usart_init.USART_BaudRate = baudrate;
    usart_init.USART_WordLength = USART_WordLength_8b;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    s_rx_head = 0;
    s_rx_tail = 0;

    USART_Init(STATUS_USART, &usart_init);
    USART_ITConfig(STATUS_USART, USART_IT_RXNE, ENABLE);
    NVIC_EnableIRQ(USART3_IRQn);
    USART_Cmd(STATUS_USART, ENABLE);
}

uint8_t StatusUart_ReadByte(uint8_t *data)
{
    if(s_rx_head == s_rx_tail)
    {
        return 0;
    }

    if(data != 0)
    {
        *data = s_rx_buffer[s_rx_tail];
    }

    s_rx_tail = (uint8_t)((s_rx_tail + 1) & STATUS_RX_BUFFER_MASK);

    return 1;
}

uint8_t StatusUart_ReadPacket(uint8_t *group, uint8_t *cmd, uint8_t *value, uint8_t *has_value)
{
    static uint8_t rx_state = 0;
    static uint8_t rx_group = 0;
    static uint8_t rx_cmd = 0;
    static uint8_t rx_value = 0;
    static uint8_t rx_has_value = 0;
    uint8_t rx_byte = 0;

    while(StatusUart_ReadByte(&rx_byte))
    {
        if(rx_state == 0)
        {
            if(StatusUart_IsGroupByte(rx_byte))
            {
                rx_group = rx_byte;
                rx_state = 1;
            }
            else
            {
                rx_state = 0;
            }
            rx_has_value = 0;
        }
        else if(rx_state == 1)
        {
            if(StatusUart_IsGroupByte(rx_byte))
            {
                rx_group = rx_byte;
                rx_has_value = 0;
                rx_state = 1;
                continue;
            }

            rx_cmd = rx_byte;
            rx_state = 2;
        }
        else if(rx_state == 2)
        {
            if(StatusUart_IsGroupByte(rx_byte))
            {
                rx_group = rx_byte;
                rx_has_value = 0;
                rx_state = 1;
                continue;
            }

            if(rx_byte == 0x5A)
            {
                rx_state = 0;
                if(group != 0)
                {
                    *group = rx_group;
                }
                if(cmd != 0)
                {
                    *cmd = rx_cmd;
                }
                if(value != 0)
                {
                    *value = 0;
                }
                if(has_value != 0)
                {
                    *has_value = 0;
                }
                return 1;
            }

            rx_value = rx_byte;
            rx_has_value = 1;
            rx_state = 3;
        }
        else
        {
            if(rx_byte == 0x5A)
            {
                rx_state = 0;
                if(group != 0)
                {
                    *group = rx_group;
                }
                if(cmd != 0)
                {
                    *cmd = rx_cmd;
                }
                if(value != 0)
                {
                    *value = rx_value;
                }
                if(has_value != 0)
                {
                    *has_value = rx_has_value;
                }
                return 1;
            }

            if(StatusUart_IsGroupByte(rx_byte))
            {
                rx_group = rx_byte;
                rx_has_value = 0;
                rx_state = 1;
                continue;
            }

            rx_has_value = 0;
            rx_state = 0;
        }
    }

    return 0;
}

void StatusUart_SendByte(uint8_t data)
{
    while(USART_GetFlagStatus(STATUS_USART, USART_FLAG_TXE) == RESET)
        ;
    USART_SendData(STATUS_USART, data);
}

void StatusUart_SendString(const char *text)
{
    while(*text != '\0')
    {
        StatusUart_SendByte((uint8_t)*text++);
    }
}

void StatusUart_Printf(const char *fmt, ...)
{
    char buffer[128];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    StatusUart_SendString(buffer);
}

void StatusUart_LogScreenPacket(uint8_t group, uint8_t cmd, uint8_t value, uint8_t has_value)
{
    if(has_value)
    {
        StatusUart_Printf("bridge rx: %02X %02X %02X 5A\r\n",
                          (unsigned int)group,
                          (unsigned int)cmd,
                          (unsigned int)value);
    }
    else
    {
        StatusUart_Printf("bridge rx: %02X %02X 5A\r\n",
                          (unsigned int)group,
                          (unsigned int)cmd);
    }
}

void USART3_IRQHandler(void)
{
    if(USART_GetITStatus(STATUS_USART, USART_IT_RXNE) != RESET)
    {
        StatusUart_PushRxByte((uint8_t)USART_ReceiveData(STATUS_USART));
    }

    if(USART_GetFlagStatus(STATUS_USART, USART_FLAG_ORE) != RESET)
    {
        (void)USART_ReceiveData(STATUS_USART);
    }
}
