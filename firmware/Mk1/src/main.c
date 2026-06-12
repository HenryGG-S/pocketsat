#include "stm32f4xx_hal.h"

/* function declarations */


static void SystemClock_Config(void);
static void GPIO_Init(void);
static void Error_Handler(void);
void SysTick_Handler(void);

int main(void)
{
    /* startup sequence */
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();

    while (1)
    {
        HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        HAL_Delay(1000);
    }

    return 0;
}

/* helper functions */

static void SystemClock_Config(void)
{
    /* For first bring-up, leave the default clock configuration alone. */
}

static void GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOA, &gpio);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);
}

static void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}
