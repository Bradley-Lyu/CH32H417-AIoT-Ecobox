#include "pump.h"

#define PUMP_GPIO_PORT      GPIOA
#define PUMP_GPIO_CLK       RCC_HB2Periph_GPIOA
#define PUMP_GPIO_PIN       GPIO_Pin_0

static uint8_t g_pump_state = 0;

void Pump_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    RCC_HB2PeriphClockCmd(PUMP_GPIO_CLK, ENABLE);

    gpio_init.GPIO_Pin = PUMP_GPIO_PIN;
    gpio_init.GPIO_Speed = GPIO_Speed_Very_High;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(PUMP_GPIO_PORT, &gpio_init);

    Pump_Off();
}

void Pump_On(void)
{
    GPIO_SetBits(PUMP_GPIO_PORT, PUMP_GPIO_PIN);
    g_pump_state = 1;
}

void Pump_Off(void)
{
    GPIO_ResetBits(PUMP_GPIO_PORT, PUMP_GPIO_PIN);
    g_pump_state = 0;
}

void Pump_Set(uint8_t on)
{
    if(on)
    {
        Pump_On();
    }
    else
    {
        Pump_Off();
    }
}

uint8_t Pump_GetState(void)
{
    return g_pump_state;
}
