#include "stm32f4xx_hal.h"

#include <string.h>
#include <stdio.h>

/* global variables */

static UART_HandleTypeDef huart2;

/* function declarations */

static void SystemClock_Config(void);
static void GPIO_Init(void);
static void Error_Handler(void);
void SysTick_Handler(void);

static void USART2_UART_Init(void);
static void telemetry_write(const char *text);
static void telemetry_write_line(const char *line);

static void command_process_byte(
    uint8_t byte,
    char *buffer,
    size_t *length,
    size_t capacity
);

static void command_handle_line(const char *line);

int main(void)
{
    uint32_t last_health_tick = 0;
    uint32_t health_counter = 0;
    char command_buffer[64] = {0};
    size_t command_len = 0;

    /* startup sequence */
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    USART2_UART_Init();  

    telemetry_write_line("EVENT,BOOT");
    telemetry_write_line("FW,NAME=pocketsat-mk1,VERSION=0.1.0");
    telemetry_write_line("BOARD,TYPE=NUCLEO-F401RE");
    telemetry_write_line("MCU,TYPE=STM32F401RE");
    telemetry_write_line("MODE,current=BOOT");

    while (1)
    {
        // health telemetry
        uint32_t now = HAL_GetTick();
        
        if ((now - last_health_tick) >= 10000)
        {
            last_health_tick = now;
            health_counter++;
            // blink LD2
            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);

            char line[80];

            snprintf(
                line,
                sizeof(line),
                "TLM,HEALTH,tick=%lu,counter=%lu",
                (unsigned long)now,
                (unsigned long)health_counter
            );

            telemetry_write_line(line);
        }

        // command
        uint8_t rx_byte = 0;

        if (HAL_UART_Receive(&huart2, &rx_byte, 1, 0) == HAL_OK)
        {
            command_process_byte(
                rx_byte,
                command_buffer,
                &command_len,
                sizeof(command_buffer)
            );
        }
    }
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

    gpio.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = GPIO_AF7_USART2;

    HAL_GPIO_Init(GPIOA, &gpio);
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
}

static void telemetry_write(const char *text)
{
    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)text,
        strlen(text),
        HAL_MAX_DELAY
    );
}

static void telemetry_write_line(const char *line)
{
    telemetry_write(line);
    telemetry_write("\r\n");
}

static void command_process_byte(
    uint8_t byte,
    char *buffer,
    size_t *length,
    size_t capacity
)
{
    if (byte == '\r')
    {
        return;
    }

    if (byte == '\n')
    {
        buffer[*length] = '\0';
        command_handle_line(buffer);
        *length = 0;
        buffer[0] = '\0';
        return;
    }

    if (*length >= capacity - 1)
    {
        telemetry_write_line("REJECT,COMMAND_BUFFER_OVERFLOW");
        *length = 0;
        buffer[0] = '\0';
        return;
    }

    buffer[*length] = byte;
    *length = *length + 1;
    return;
}

static void command_handle_line(const char *line)
{
    if (strcmp(line, "") == 0 )
    {
        telemetry_write_line("REJECT,EMPTY_COMMAND");
        return;
    }
    if (strcmp(line, "PING") == 0)
    {
        telemetry_write_line("ACK,PING");
        return;
    }

    if (strcmp(line, "GET_STATUS") == 0)
    {
        telemetry_write_line("TLM,STATUS,mode=BOOT");
        return;
    }

    telemetry_write_line("REJECT,UNKNOWN_COMMAND");
}
