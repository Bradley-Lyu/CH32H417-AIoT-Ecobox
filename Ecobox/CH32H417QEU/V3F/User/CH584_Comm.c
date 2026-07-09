#include "CH584_Comm.h"

/*
 * Independent UART link for CH584.
 * Reserved wiring:
 * PD5 = USART2_TX -> CH584M RX
 * PD6 = USART2_RX <- CH584M TX
 *
 * main initializes this link at 115200 baud. PB10/PB11 remain the active
 * screen/status UART on USART3.
 */
#define CH584_COMM_USART               USART2
#define CH584_COMM_USART_CLK           RCC_HB1Periph_USART2
#define CH584_COMM_GPIO_PORT           GPIOD
#define CH584_COMM_GPIO_CLK            RCC_HB2Periph_GPIOD
#define CH584_COMM_TX_PIN              GPIO_Pin_5
#define CH584_COMM_RX_PIN              GPIO_Pin_6
#define CH584_COMM_TX_PIN_SOURCE       GPIO_PinSource5
#define CH584_COMM_RX_PIN_SOURCE       GPIO_PinSource6
#define CH584_COMM_GPIO_AF             GPIO_AF7
#define CH584_COMM_RX_BUFFER_SIZE      128
#define CH584_COMM_RX_BUFFER_MASK      (CH584_COMM_RX_BUFFER_SIZE - 1)

static volatile uint8_t s_rx_buffer[CH584_COMM_RX_BUFFER_SIZE];
static volatile uint8_t s_rx_head = 0;
static volatile uint8_t s_rx_tail = 0;

void USART2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static uint8_t CH584_Comm_IsGroupByte(uint8_t data)
{
    return (data == 0xA5) ||
           (data == 0xA4) ||
           (data == 0xA3);
}

static void CH584_Comm_PushRxByte(uint8_t data)
{
    uint8_t next_head = (uint8_t)((s_rx_head + 1U) & CH584_COMM_RX_BUFFER_MASK);

    if(next_head == s_rx_tail)
    {
        s_rx_tail = (uint8_t)((s_rx_tail + 1U) & CH584_COMM_RX_BUFFER_MASK);
    }

    s_rx_buffer[s_rx_head] = data;
    s_rx_head = next_head;
}

void CH584_Comm_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio_init = {0};
    USART_InitTypeDef usart_init = {0};

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | CH584_COMM_GPIO_CLK, ENABLE);
    RCC_HB1PeriphClockCmd(CH584_COMM_USART_CLK, ENABLE);

    GPIO_PinAFConfig(CH584_COMM_GPIO_PORT, CH584_COMM_TX_PIN_SOURCE, CH584_COMM_GPIO_AF);
    GPIO_PinAFConfig(CH584_COMM_GPIO_PORT, CH584_COMM_RX_PIN_SOURCE, CH584_COMM_GPIO_AF);

    gpio_init.GPIO_Pin = CH584_COMM_TX_PIN;
    gpio_init.GPIO_Speed = GPIO_Speed_Very_High;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(CH584_COMM_GPIO_PORT, &gpio_init);

    gpio_init.GPIO_Pin = CH584_COMM_RX_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(CH584_COMM_GPIO_PORT, &gpio_init);

    usart_init.USART_BaudRate = baudrate;
    usart_init.USART_WordLength = USART_WordLength_8b;
    usart_init.USART_StopBits = USART_StopBits_1;
    usart_init.USART_Parity = USART_Parity_No;
    usart_init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart_init.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

    s_rx_head = 0;
    s_rx_tail = 0;

    USART_Init(CH584_COMM_USART, &usart_init);
    USART_ITConfig(CH584_COMM_USART, USART_IT_RXNE, ENABLE);
    NVIC_EnableIRQ(USART2_IRQn);
    USART_Cmd(CH584_COMM_USART, ENABLE);
}

void CH584_Comm_FlushRx(void)
{
    s_rx_head = 0;
    s_rx_tail = 0;
}

uint8_t CH584_Comm_ReadByte(uint8_t *data)
{
    if(s_rx_head == s_rx_tail)
    {
        return 0;
    }

    if(data != 0)
    {
        *data = s_rx_buffer[s_rx_tail];
    }

    s_rx_tail = (uint8_t)((s_rx_tail + 1U) & CH584_COMM_RX_BUFFER_MASK);
    return 1;
}

uint8_t CH584_Comm_ReadPacket(CH584_CommPacket *packet)
{
    static uint8_t rx_state = 0;
    static uint8_t rx_group = 0;
    static uint8_t rx_cmd = 0;
    static uint8_t rx_value = 0;
    static uint8_t rx_has_value = 0;
    uint8_t rx_byte = 0;

    while(CH584_Comm_ReadByte(&rx_byte))
    {
        if(rx_state == 0)
        {
            if(CH584_Comm_IsGroupByte(rx_byte))
            {
                rx_group = rx_byte;
                rx_has_value = 0;
                rx_state = 1;
            }
        }
        else if(rx_state == 1)
        {
            if(CH584_Comm_IsGroupByte(rx_byte))
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
            if(CH584_Comm_IsGroupByte(rx_byte))
            {
                rx_group = rx_byte;
                rx_has_value = 0;
                rx_state = 1;
                continue;
            }

            if(rx_byte == 0x5A)
            {
                rx_state = 0;
                if(packet != 0)
                {
                    packet->group = rx_group;
                    packet->cmd = rx_cmd;
                    packet->value = 0;
                    packet->has_value = 0;
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
                if(packet != 0)
                {
                    packet->group = rx_group;
                    packet->cmd = rx_cmd;
                    packet->value = rx_value;
                    packet->has_value = rx_has_value;
                }
                return 1;
            }

            if(CH584_Comm_IsGroupByte(rx_byte))
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

void CH584_Comm_SendByte(uint8_t data)
{
    while(USART_GetFlagStatus(CH584_COMM_USART, USART_FLAG_TXE) == RESET)
        ;
    USART_SendData(CH584_COMM_USART, data);
}

void CH584_Comm_SendBuffer(const uint8_t *data, uint16_t length)
{
    uint16_t i = 0;

    if(data == 0)
    {
        return;
    }

    for(i = 0; i < length; i++)
    {
        CH584_Comm_SendByte(data[i]);
    }
}

void CH584_Comm_SendPacket(const CH584_CommPacket *packet)
{
    if(packet == 0)
    {
        return;
    }

    CH584_Comm_SendByte(packet->group);
    CH584_Comm_SendByte(packet->cmd);
    if(packet->has_value)
    {
        CH584_Comm_SendByte(packet->value);
    }
    CH584_Comm_SendByte(0x5A);
}

void USART2_IRQHandler(void)
{
    if(USART_GetITStatus(CH584_COMM_USART, USART_IT_RXNE) != RESET)
    {
        CH584_Comm_PushRxByte((uint8_t)USART_ReceiveData(CH584_COMM_USART));
    }

    if(USART_GetFlagStatus(CH584_COMM_USART, USART_FLAG_ORE) != RESET)
    {
        (void)USART_ReceiveData(CH584_COMM_USART);
    }
}
