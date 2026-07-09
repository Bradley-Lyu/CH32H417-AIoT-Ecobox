#include "ledwest.h"

#define LEDWEST_GPIO_PORT        GPIOA
#define LEDWEST_GPIO_CLK         RCC_HB2Periph_GPIOA
#define LEDWEST_GPIO_PIN         GPIO_Pin_5
#define LEDWEST_GPIO_PIN_SOURCE  GPIO_PinSource5
#define LEDWEST_GPIO_AF          GPIO_AF3

#define LEDWEST_TIM              TIM8
#define LEDWEST_TIM_CLK          RCC_HB2Periph_TIM8
#define LEDWEST_PWM_PERIOD       1000U

static uint8_t g_ledwest_brightness = 0;

void LedWest_Init(void)
{
    GPIO_InitTypeDef gpio_init = {0};
    TIM_OCInitTypeDef tim_oc_init = {0};
    TIM_TimeBaseInitTypeDef tim_base_init = {0};
    uint16_t prescaler = 0;

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO | LEDWEST_GPIO_CLK | LEDWEST_TIM_CLK, ENABLE);
    GPIO_PinAFConfig(LEDWEST_GPIO_PORT, LEDWEST_GPIO_PIN_SOURCE, LEDWEST_GPIO_AF);

    gpio_init.GPIO_Pin = LEDWEST_GPIO_PIN;
    gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_Very_High;
    GPIO_Init(LEDWEST_GPIO_PORT, &gpio_init);

    prescaler = (uint16_t)(SystemCoreClock / 1000000U) - 1U;

    tim_base_init.TIM_Period = LEDWEST_PWM_PERIOD - 1U;
    tim_base_init.TIM_Prescaler = prescaler;
    tim_base_init.TIM_ClockDivision = TIM_CKD_DIV1;
    tim_base_init.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(LEDWEST_TIM, &tim_base_init);

    tim_oc_init.TIM_OCMode = TIM_OCMode_PWM1;
    tim_oc_init.TIM_OutputState = TIM_OutputState_Disable;
    tim_oc_init.TIM_OutputNState = TIM_OutputNState_Enable;
    tim_oc_init.TIM_Pulse = 0;
    tim_oc_init.TIM_OCPolarity = TIM_OCPolarity_High;
    tim_oc_init.TIM_OCNPolarity = TIM_OCNPolarity_High;
    tim_oc_init.TIM_OCIdleState = TIM_OCIdleState_Reset;
    tim_oc_init.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
    TIM_OC1Init(LEDWEST_TIM, &tim_oc_init);

    TIM_CtrlPWMOutputs(LEDWEST_TIM, ENABLE);
    TIM_OC1PreloadConfig(LEDWEST_TIM, TIM_OCPreload_Disable);
    TIM_ARRPreloadConfig(LEDWEST_TIM, ENABLE);
    TIM_Cmd(LEDWEST_TIM, ENABLE);

    LedWest_SetBrightness(0);
}

void LedWest_SetBrightness(uint8_t percent)
{
    uint16_t compare = 0;

    if(percent > 100U)
    {
        percent = 100U;
    }

    g_ledwest_brightness = percent;
    compare = (uint16_t)(((uint32_t)percent * (LEDWEST_PWM_PERIOD - 1U)) / 100U);
    TIM_SetCompare1(LEDWEST_TIM, compare);
}

uint8_t LedWest_GetBrightness(void)
{
    return g_ledwest_brightness;
}
