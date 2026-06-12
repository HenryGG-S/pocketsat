#include "stm32f4xx_hal.h"

#include <string.h>
#include <stdio.h>

/* system state */

typedef enum
{
    MODE_BOOT = 0,
    MODE_SAFE,
    MODE_NOMINAL,
    MODE_PAYLOAD
} SystemMode;

typedef enum
{
    FAULT_NONE = 0,
    FAULT_SENSOR_MISSING,
    FAULT_OVERTEMP,
    FAULT_COMMAND_STORM
} SystemFault;

/* global variables */

static UART_HandleTypeDef huart2;

/* function declarations */

static void SystemClock_Config(void);
static void GPIO_Init(void);
static void USART2_UART_Init(void);
static void Error_Handler(void);

void SysTick_Handler(void);

static void telemetry_write(const char *text);
static void telemetry_write_line(const char *line);

static void command_process_byte(
    uint8_t byte,
    char *buffer,
    size_t *length,
    size_t capacity,
    SystemMode *current_mode,
    SystemFault *current_fault
);

static void command_handle_line(
    const char *line,
    SystemMode *current_mode,
    SystemFault *current_fault
);

static const char *mode_to_string(SystemMode mode);
static const char *fault_to_string(SystemFault fault);

static void request_mode_change(
    SystemMode *current_mode,
    SystemFault current_fault,
    SystemMode requested
);

static int mode_transition_allowed(SystemMode from, SystemMode to);

static void raise_fault(
    SystemFault *current_fault,
    SystemMode *current_mode,
    SystemFault fault
);

static void clear_faults(SystemFault *current_fault);

int main(void)
{
    uint32_t last_health_tick = 0;
    uint32_t health_counter = 0;

    char command_buffer[64] = {0};
    size_t command_len = 0;

    SystemMode current_mode = MODE_BOOT;
    SystemFault current_fault = FAULT_NONE;

    HAL_Init();
    SystemClock_Config();
    GPIO_Init();
    USART2_UART_Init();

    telemetry_write_line("EVENT,BOOT");
    telemetry_write_line("FW,NAME=pocketsat-mk1,VERSION=0.2.0");
    telemetry_write_line("BOARD,TYPE=NUCLEO-F401RE");
    telemetry_write_line("MCU,TYPE=STM32F401RE");
    telemetry_write_line("MODE,current=BOOT");

    current_mode = MODE_SAFE;
    telemetry_write_line("EVENT,MODE_CHANGE,from=BOOT,to=SAFE,reason=boot_complete");

    while (1)
    {
        uint32_t now = HAL_GetTick();

        if ((now - last_health_tick) >= 1000)
        {
            last_health_tick = now;
            health_counter++;

            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);

            char line[128];

            snprintf(
                line,
                sizeof(line),
                "TLM,HEALTH,tick=%lu,counter=%lu,mode=%s,fault=%s",
                (unsigned long)now,
                (unsigned long)health_counter,
                mode_to_string(current_mode),
                fault_to_string(current_fault)
            );

            telemetry_write_line(line);
        }

        uint8_t rx_byte = 0;

        if (HAL_UART_Receive(&huart2, &rx_byte, 1, 0) == HAL_OK)
        {
            command_process_byte(
                rx_byte,
                command_buffer,
                &command_len,
                sizeof(command_buffer),
                &current_mode,
                &current_fault
            );
        }
    }
}

/* setup */

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

/* telemetry */

static void telemetry_write(const char *text)
{
    HAL_UART_Transmit(
        &huart2,
        (uint8_t *)text,
        (uint16_t)strlen(text),
        HAL_MAX_DELAY
    );
}

static void telemetry_write_line(const char *line)
{
    telemetry_write(line);
    telemetry_write("\r\n");
}

/* command input */

static void command_process_byte(
    uint8_t byte,
    char *buffer,
    size_t *length,
    size_t capacity,
    SystemMode *current_mode,
    SystemFault *current_fault
)
{
    if (byte == '\r')
    {
        return;
    }

    if (byte == '\n')
    {
        buffer[*length] = '\0';

        command_handle_line(
            buffer,
            current_mode,
            current_fault
        );

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

    buffer[*length] = (char)byte;
    *length = *length + 1;
    buffer[*length] = '\0';
}

static void command_handle_line(
    const char *line,
    SystemMode *current_mode,
    SystemFault *current_fault
)
{
    if (strcmp(line, "") == 0)
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
        char response[96];

        snprintf(
            response,
            sizeof(response),
            "TLM,STATUS,mode=%s,fault=%s",
            mode_to_string(*current_mode),
            fault_to_string(*current_fault)
        );

        telemetry_write_line(response);
        return;
    }

    if (strcmp(line, "SET_MODE SAFE") == 0)
    {
        request_mode_change(current_mode, *current_fault, MODE_SAFE);
        return;
    }

    if (strcmp(line, "SET_MODE NOMINAL") == 0)
    {
        request_mode_change(current_mode, *current_fault, MODE_NOMINAL);
        return;
    }

    if (strcmp(line, "SET_MODE PAYLOAD") == 0)
    {
        request_mode_change(current_mode, *current_fault, MODE_PAYLOAD);
        return;
    }

    if (strcmp(line, "INJECT_FAULT SENSOR_MISSING") == 0)
    {
        raise_fault(current_fault, current_mode, FAULT_SENSOR_MISSING);
        return;
    }

    if (strcmp(line, "INJECT_FAULT OVERTEMP") == 0)
    {
        raise_fault(current_fault, current_mode, FAULT_OVERTEMP);
        return;
    }

    if (strcmp(line, "INJECT_FAULT COMMAND_STORM") == 0)
    {
        raise_fault(current_fault, current_mode, FAULT_COMMAND_STORM);
        return;
    }

    if (strcmp(line, "CLEAR_FAULTS") == 0)
    {
        clear_faults(current_fault);
        return;
    }

    telemetry_write_line("REJECT,UNKNOWN_COMMAND");
}

/* modes */

static const char *mode_to_string(SystemMode mode)
{
    switch (mode)
    {
        case MODE_BOOT:
            return "BOOT";

        case MODE_SAFE:
            return "SAFE";

        case MODE_NOMINAL:
            return "NOMINAL";

        case MODE_PAYLOAD:
            return "PAYLOAD";

        default:
            return "UNKNOWN";
    }
}

static void request_mode_change(
    SystemMode *current_mode,
    SystemFault current_fault,
    SystemMode requested
)
{
    SystemMode previous = *current_mode;

    if (current_fault != FAULT_NONE && requested != MODE_SAFE)
    {
        char response[96];

        snprintf(
            response,
            sizeof(response),
            "DENY,ACTIVE_FAULT,fault=%s",
            fault_to_string(current_fault)
        );

        telemetry_write_line(response);
        return;
    }

    if (!mode_transition_allowed(previous, requested))
    {
        char response[96];

        snprintf(
            response,
            sizeof(response),
            "DENY,MODE_NOT_ALLOWED,from=%s,to=%s",
            mode_to_string(previous),
            mode_to_string(requested)
        );

        telemetry_write_line(response);
        return;
    }

    *current_mode = requested;

    char ack[64];

    snprintf(
        ack,
        sizeof(ack),
        "ACK,SET_MODE,%s",
        mode_to_string(requested)
    );

    telemetry_write_line(ack);

    char event[96];

    snprintf(
        event,
        sizeof(event),
        "EVENT,MODE_CHANGE,from=%s,to=%s,reason=command",
        mode_to_string(previous),
        mode_to_string(requested)
    );

    telemetry_write_line(event);
}

static int mode_transition_allowed(SystemMode from, SystemMode to)
{
    if (from == to)
    {
        return 1;
    }

    if (to == MODE_SAFE)
    {
        return 1;
    }

    if (from == MODE_SAFE && to == MODE_NOMINAL)
    {
        return 1;
    }

    if (from == MODE_NOMINAL && to == MODE_PAYLOAD)
    {
        return 1;
    }

    if (from == MODE_PAYLOAD && to == MODE_NOMINAL)
    {
        return 1;
    }

    return 0;
}

/* faults */

static const char *fault_to_string(SystemFault fault)
{
    switch (fault)
    {
        case FAULT_NONE:
            return "NONE";

        case FAULT_SENSOR_MISSING:
            return "SENSOR_MISSING";

        case FAULT_OVERTEMP:
            return "OVERTEMP";

        case FAULT_COMMAND_STORM:
            return "COMMAND_STORM";

        default:
            return "UNKNOWN";
    }
}

static void raise_fault(
    SystemFault *current_fault,
    SystemMode *current_mode,
    SystemFault fault
)
{
    *current_fault = fault;

    char fault_event[96];

    snprintf(
        fault_event,
        sizeof(fault_event),
        "EVENT,FAULT_RAISED,fault=%s",
        fault_to_string(fault)
    );

    telemetry_write_line(fault_event);

    if (*current_mode != MODE_SAFE)
    {
        SystemMode previous = *current_mode;
        *current_mode = MODE_SAFE;

        char mode_event[96];

        snprintf(
            mode_event,
            sizeof(mode_event),
            "EVENT,MODE_CHANGE,from=%s,to=SAFE,reason=fault",
            mode_to_string(previous)
        );

        telemetry_write_line(mode_event);
    }
}

static void clear_faults(SystemFault *current_fault)
{
    *current_fault = FAULT_NONE;

    telemetry_write_line("ACK,CLEAR_FAULTS");
    telemetry_write_line("EVENT,FAULT_CLEARED");
}
