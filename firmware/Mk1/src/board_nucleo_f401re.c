#include "board_nucleo_f401re.h"
#include "app_config.h"

#include <string.h>

static UART_HandleTypeDef huart2;
static I2C_HandleTypeDef hi2c1;
static uint8_t uart_rx_it_byte = 0u;
static volatile uint8_t uart_rx_ring[UART_RX_RING_SIZE];
static volatile uint16_t uart_rx_head = 0u;
static volatile uint16_t uart_rx_tail = 0u;
static volatile uint32_t uart_rx_overflow_count = 0u;

static void SystemClock_Config(void);
static void GPIO_Init(void);
static void I2C1_GPIO_Init(void);
static void USART2_UART_Init(void);
static void I2C1_Init(void);
static void Error_Handler(void);
static void board_uart_rx_ring_push(uint8_t byte);

void board_init(void)
{
    SystemClock_Config();
    GPIO_Init();
    USART2_UART_Init();

    if (HAL_UART_Receive_IT(&huart2, &uart_rx_it_byte, 1u) != HAL_OK)
    {
        Error_Handler();
    }

    I2C1_Init();
}

void board_error_handler(void)
{
    Error_Handler();
}

uint32_t board_millis(void)
{
    return HAL_GetTick();
}

void board_delay_ms(uint32_t delay_ms)
{
    HAL_Delay(delay_ms);
}

void board_heartbeat_toggle(void)
{
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
}

void board_uart_write(const char *text)
{
    if (text == NULL)
    {
        return;
    }

    HAL_UART_Transmit(
        &huart2, (uint8_t *)text, (uint16_t)strlen(text), HAL_MAX_DELAY);
}

uint8_t board_uart_receive_byte(uint8_t *byte)
{
    uint8_t result = 0u;

    if (byte == NULL)
    {
        return 0u;
    }

    __disable_irq();

    if (uart_rx_tail != uart_rx_head)
    {
        *byte = uart_rx_ring[uart_rx_tail];
        uart_rx_tail = (uint16_t)((uart_rx_tail + 1u) % UART_RX_RING_SIZE);
        result = 1u;
    }

    __enable_irq();

    return result;
}

HAL_StatusTypeDef board_i2c_mem_read(uint16_t dev_addr,
                                     uint16_t mem_addr,
                                     uint16_t mem_addr_size,
                                     uint8_t *data,
                                     uint16_t size,
                                     uint32_t timeout_ms)
{
    return HAL_I2C_Mem_Read(
        &hi2c1, dev_addr, mem_addr, mem_addr_size, data, size, timeout_ms);
}

HAL_StatusTypeDef board_i2c_mem_write(uint16_t dev_addr,
                                      uint16_t mem_addr,
                                      uint16_t mem_addr_size,
                                      uint8_t *data,
                                      uint16_t size,
                                      uint32_t timeout_ms)
{
    return HAL_I2C_Mem_Write(
        &hi2c1, dev_addr, mem_addr, mem_addr_size, data, size, timeout_ms);
}

void board_i2c1_recover(void)
{
    HAL_I2C_DeInit(&hi2c1);

    __HAL_RCC_I2C1_FORCE_RESET();
    HAL_Delay(I2C_RECOVERY_DELAY_MS);
    __HAL_RCC_I2C1_RELEASE_RESET();

    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &gpio);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(I2C_RECOVERY_DELAY_MS);

    for (int i = 0; i < 9; i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);
        HAL_Delay(I2C_RECOVERY_DELAY_MS);

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
        HAL_Delay(I2C_RECOVERY_DELAY_MS);
    }

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_RESET);
    HAL_Delay(I2C_RECOVERY_DELAY_MS);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
    HAL_Delay(I2C_RECOVERY_DELAY_MS);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9, GPIO_PIN_SET);
    HAL_Delay(I2C_RECOVERY_DELAY_MS);

    I2C1_GPIO_Init();
    I2C1_Init();
}

uint32_t board_i2c1_error(void)
{
    return HAL_I2C_GetError(&hi2c1);
}

GPIO_PinState board_i2c1_scl_state(void)
{
    return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8);
}

GPIO_PinState board_i2c1_sda_state(void)
{
    return HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9);
}

static void SystemClock_Config(void)
{
    /* For first bring-up, leave the default clock configuration alone. */
}

static void GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    /* LD2: PA5 */
    gpio.Pin = GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOA, &gpio);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET);

    /* USART2: PA2 TX, PA3 RX */
    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;

    HAL_GPIO_Init(GPIOA, &gpio);

    /* I2C1: PB8 SCL, PB9 SDA */
    I2C1_GPIO_Init();
}

static void I2C1_GPIO_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    gpio.Mode = GPIO_MODE_AF_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF4_I2C1;

    HAL_GPIO_Init(GPIOB, &gpio);
}

static void USART2_UART_Init(void)
{
    __HAL_RCC_USART2_CLK_ENABLE();

    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;

    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_NVIC_SetPriority(USART2_IRQn, 5u, 0u);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static void I2C1_Init(void)
{
    __HAL_RCC_I2C1_CLK_ENABLE();

    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
    {
        Error_Handler();
    }
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

BoardResetCause board_reset_cause_get(void)
{
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != RESET)
    {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        return BOARD_RESET_POWER_ON;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET)
    {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        return BOARD_RESET_PIN;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET)
    {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        return BOARD_RESET_SOFTWARE;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
    {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        return BOARD_RESET_IWDG;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET)
    {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        return BOARD_RESET_WWDG;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != RESET)
    {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        return BOARD_RESET_BROWNOUT;
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET)
    {
        __HAL_RCC_CLEAR_RESET_FLAGS();
        return BOARD_RESET_LOW_POWER;
    }
    return BOARD_RESET_UNKNOWN;
}

const char *board_reset_cause_to_string(BoardResetCause cause)
{
    switch (cause)
    {
        case BOARD_RESET_UNKNOWN:
            return "UNKNOWN";
        case BOARD_RESET_POWER_ON:
            return "POWER_ON";
        case BOARD_RESET_PIN:
            return "PIN";
        case BOARD_RESET_SOFTWARE:
            return "SOFTWARE";
        case BOARD_RESET_IWDG:
            return "IWDG";
        case BOARD_RESET_WWDG:
            return "WWDG";
        case BOARD_RESET_BROWNOUT:
            return "BROWNOUT";
        case BOARD_RESET_LOW_POWER:
            return "LOW_POWER";
        default:
            return "INVALID";
    }
}

static void board_uart_rx_ring_push(uint8_t byte)
{
    uint16_t next_head = (uint16_t)((uart_rx_head + 1u) % UART_RX_RING_SIZE);

    if (next_head == uart_rx_tail)
    {
        uart_rx_overflow_count++;
        return;
    }

    uart_rx_ring[uart_rx_head] = byte;
    uart_rx_head = next_head;
}

void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart2);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2)
    {
        return;
    }

    board_uart_rx_ring_push(uart_rx_it_byte);

    if (HAL_UART_Receive_IT(&huart2, &uart_rx_it_byte, 1u) != HAL_OK)
    {
        uart_rx_overflow_count++;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART2)
    {
        return;
    }

    uart_rx_overflow_count++;

    (void)HAL_UART_Receive_IT(&huart2, &uart_rx_it_byte, 1u);
}