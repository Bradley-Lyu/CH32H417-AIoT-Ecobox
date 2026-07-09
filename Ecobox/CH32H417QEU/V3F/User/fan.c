#include "fan.h"

#define FAN_GPIO_PORT       GPIOA
#define FAN_GPIO_CLK        RCC_HB2Periph_GPIOA
#define FAN_GPIO_PIN        GPIO_Pin_1

static uint8_t g_fan_state = 0;

void Fan_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    RCC_HB2PeriphClockCmd(FAN_GPIO_CLK, ENABLE);

    gpio_init.GPIO_Pin = FAN_GPIO_PIN;
    gpio_init.GPIO_Speed = GPIO_Speed_Very_High;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(FAN_GPIO_PORT, &gpio_init);

    Fan_Off();
}

void Fan_On(void)
{
    GPIO_SetBits(FAN_GPIO_PORT, FAN_GPIO_PIN);
    g_fan_state = 1;
}

void Fan_Off(void)
{
    GPIO_ResetBits(FAN_GPIO_PORT, FAN_GPIO_PIN);
    g_fan_state = 0;
}

void Fan_Set(uint8_t on)
{
    if(on)
    {
        Fan_On();
    }
    else
    {
        Fan_Off();
    }
}

uint8_t Fan_GetState(void)
{
    return g_fan_state;
}
