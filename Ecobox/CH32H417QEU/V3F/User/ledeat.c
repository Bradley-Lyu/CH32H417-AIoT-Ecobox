#include "ledeat.h"

#define LEDEAST_GPIO_PORT        GPIOA
#define LEDEAST_GPIO_CLK         RCC_HB2Periph_GPIOA
#define LEDEAST_GPIO_PIN         GPIO_Pin_8
#define LEDEAST_GPIO_PIN_SOURCE  GPIO_PinSource8
#define LEDEAST_GPIO_AF          GPIO_AF1

#define LEDEAST_TIM              TIM1
#define LEDEAST_TIM_CLK          RCC_HB2Periph_TIM1
#define LEDEAST_PWM_PERIOD       1000U

static uint8_t g_ledeast_brightness = 0;

void LedEast_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    TIM_OCInitTypeDef tim_oc_init = {0};
    TIM_TimeBaseInitTypeDef tim_base_init = {0};
    uint16_t prescaler = 0;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | LEDEAST_GPIO_CLK | LEDEAST_TIM_CLK, ENABLE);
    GPIO_PinAFConfig(LEDEAST_GPIO_PORT, LEDEAST_GPIO_PIN_SOURCE, LEDEAST_GPIO_AF);

    gpio_init.GPIO_Pin = LEDEAST_GPIO_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(LEDEAST_GPIO_PORT, &gpio_init);

    prescaler = (uint16_t)(SystemCoreClock / 1000000U) - 1U;

    tim_base_init.TIM_Period = LEDEAST_PWM_PERIOD - 1U;
    tim_base_init.TIM_Prescaler = prescaler;
    tim_base_init.TIM_ClockDivision = TIM_CKD_DIV1;
    tim_base_init.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(LEDEAST_TIM, &tim_base_init);

    tim_oc_init.TIM_OCMode = TIM_OCMode_PWM1;
    tim_oc_init.TIM_OutputState = TIM_OutputState_Enable;
    tim_oc_init.TIM_Pulse = 0;
    tim_oc_init.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(LEDEAST_TIM, &tim_oc_init);

    TIM_CtrlPWMOutputs(LEDEAST_TIM, ENABLE);
    TIM_OC1PreloadConfig(LEDEAST_TIM, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(LEDEAST_TIM, ENABLE);
    TIM_Cmd(LEDEAST_TIM, ENABLE);

    LedEast_SetBrightness(0);
}

void LedEast_SetBrightness(uint8_t percent)
{
    uint16_t compare = 0;

    if(percent > 100U)
    {
        percent = 100U;
    }

    g_ledeast_brightness = percent;
    compare = (uint16_t)(((uint32_t)percent * (LEDEAST_PWM_PERIOD - 1U)) / 100U);
    TIM_SetCompare1(LEDEAST_TIM, compare);
}

uint8_t LedEast_GetBrightness(void)
{
    return g_ledeast_brightness;
}
