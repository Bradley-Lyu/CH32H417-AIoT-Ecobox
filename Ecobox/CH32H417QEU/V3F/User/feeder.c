#include "feeder.h"

#define FEEDER_GPIO_PORT          GPIOA
#define FEEDER_GPIO_CLK           RCC_HB2Periph_GPIOA
#define FEEDER_GPIO_PIN           GPIO_Pin_2
#define FEEDER_GPIO_PIN_SOURCE    GPIO_PinSource2
#define FEEDER_GPIO_AF            GPIO_AF1

#define FEEDER_TIM                TIM2
#define FEEDER_TIM_CLK            RCC_HB1Periph_TIM2
#define FEEDER_TIM_CHANNEL_INIT   TIM_OC3Init
#define FEEDER_TIM_SET_COMPARE    TIM_SetCompare3

#define FEEDER_MIN_PULSE_US        500U
#define FEEDER_MAX_PULSE_US       2500U
#define FEEDER_RETURN_DELAY_MS     700U

static uint16_t g_feeder_pulse_us = FEEDER_PULSE_90_DEG_US;
static uint8_t g_feeder_busy = 0;
static uint32_t g_feeder_elapsed_ms = 0;

void Feeder_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    TIM_OCInitTypeDef tim_oc_init = {0};
    TIM_TimeBaseInitTypeDef tim_base_init = {0};
    uint16_t prescaler = 0;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | FEEDER_GPIO_CLK, ENABLE);
    RCC_HB1PeriphClockCmd(FEEDER_TIM_CLK, ENABLE);

    GPIO_PinAFConfig(FEEDER_GPIO_PORT, FEEDER_GPIO_PIN_SOURCE, FEEDER_GPIO_AF);

    gpio_init.GPIO_Pin = FEEDER_GPIO_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(FEEDER_GPIO_PORT, &gpio_init);

    prescaler = (uint16_t)(SystemCoreClock / 1000000U) - 1U;

    tim_base_init.TIM_Period = FEEDER_PWM_PERIOD_US - 1U;
    tim_base_init.TIM_Prescaler = prescaler;
    tim_base_init.TIM_ClockDivision = TIM_CKD_DIV1;
    tim_base_init.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(FEEDER_TIM, &tim_base_init);

    tim_oc_init.TIM_OCMode = TIM_OCMode_PWM1;
    tim_oc_init.TIM_OutputState = TIM_OutputState_Enable;
    tim_oc_init.TIM_Pulse = FEEDER_PULSE_90_DEG_US;
    tim_oc_init.TIM_OCPolarity = TIM_OCPolarity_High;
    FEEDER_TIM_CHANNEL_INIT(FEEDER_TIM, &tim_oc_init);

    TIM_OC3PreloadConfig(FEEDER_TIM, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(FEEDER_TIM, ENABLE);
    TIM_Cmd(FEEDER_TIM, ENABLE);

    Feeder_Set90Deg();
}

void Feeder_SetPulseUs(uint16_t pulse_us)
{
    if(pulse_us < FEEDER_MIN_PULSE_US)
    {
        pulse_us = FEEDER_MIN_PULSE_US;
    }
    else if(pulse_us > FEEDER_MAX_PULSE_US)
    {
        pulse_us = FEEDER_MAX_PULSE_US;
    }

    g_feeder_pulse_us = pulse_us;
    FEEDER_TIM_SET_COMPARE(FEEDER_TIM, pulse_us);
}

void Feeder_Set90Deg(void)
{
    Feeder_SetPulseUs(FEEDER_PULSE_90_DEG_US);
}

void Feeder_Set180Deg(void)
{
    Feeder_SetPulseUs(FEEDER_PULSE_180_DEG_US);
}

void Feeder_Reset(void)
{
    Feeder_Set90Deg();
    g_feeder_busy = 0;
    g_feeder_elapsed_ms = 0;
}

void Feeder_FeedOnce(void)
{
    Feeder_Set180Deg();
    g_feeder_busy = 1;
    g_feeder_elapsed_ms = 0;
}

void Feeder_Service(uint32_t elapsed_ms)
{
    if(!g_feeder_busy)
    {
        return;
    }

    g_feeder_elapsed_ms += elapsed_ms;
    if(g_feeder_elapsed_ms >= FEEDER_RETURN_DELAY_MS)
    {
        Feeder_Reset();
    }
}
