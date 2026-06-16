#include "stm32f4xx_hal.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* PocketSat Mk1 software requirements, informal ECSS-inspired trace.
 *
 * REQ-MODE-001:
 *   The system shall enter SAFE after BOOT completes.
 *
 * REQ-MODE-002:
 *   The system shall deny transitions out of SAFE when an active fault exists.
 *
 * REQ-CMD-001:
 *   The system shall reject commands longer than COMMAND_BUFFER_SIZE - 1.
 *
 * REQ-CMD-002:
 *   The system shall reject unknown commands.
 *
 * REQ-FDIR-001:
 *   The system shall raise SENSOR_MISSING if MPU6050 WHO_AM_I cannot be read
 *   or returns an unexpected device ID.
 *
 * REQ-FDIR-002:
 *   The system shall enter SAFE when a fault is raised outside SAFE.
 *
 * REQ-FDIR-003:
 *   The system shall raise COMMAND_STORM if more than COMMAND_STORM_LIMIT
 *   complete command lines are received within COMMAND_STORM_WINDOW_MS.
 *
 * REQ-TLM-001:
 *   Every emitted telemetry line shall contain a monotonic sequence number.
 *
 * REQ-TLM-002:
 *   Every emitted telemetry line shall contain the current HAL tick.
 *
 * REQ-TLM-003:
 *   Every event telemetry line shall contain a monotonic event identifier.
 *
 * REQ-SNS-001:
 *   CHECK_SENSOR shall actively read the MPU6050 WHO_AM_I register.
 *
 * REQ-SNS-002:
 *   INIT_SENSOR shall verify WHO_AM_I before waking/configuring the MPU6050.
 *
 * REQ-SNS-003:
 *   SAMPLE_IMU shall perform an immediate MPU6050 burst sample attempt.
 *
 * REQ-SNS-004:
 *   GET_IMU shall report the latest cached valid IMU sample.
 *
 * REQ-SNS-005:
 *   GET_IMU shall reject if no valid IMU sample has been acquired.
 */

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

typedef enum
{
    SENSOR_OK = 0,
    SENSOR_MISSING,
    SENSOR_BAD_ID
} SensorStatus;

typedef struct
{
    int16_t ax;
    int16_t ay;
    int16_t az;
    int16_t temp;
    int16_t gx;
    int16_t gy;
    int16_t gz;
} ImuSample;

typedef struct
{
    uint32_t telemetry_seq;
    uint32_t event_seq;
    uint32_t command_seq;
} TelemetryState;

typedef struct
{
    uint32_t window_start_tick;
    uint8_t count;
} CommandRateState;

typedef struct
{
    ImuSample latest_imu;
    SensorStatus latest_sensor_status;
    uint8_t latest_whoami;
    uint32_t latest_imu_tick;
    uint32_t latest_sensor_tick;
    uint8_t imu_valid;
    uint8_t sensor_status_valid;
} SensorRuntime;

typedef enum
{
    LCD_PAGE_STATUS = 0,
    LCD_PAGE_IMU
} LcdPage;

typedef struct
{
    SystemMode mode;
    SystemFault fault;

    TelemetryState telemetry;
    CommandRateState command_rate;
    SensorRuntime sensor;

    LcdPage lcd_page;
} AppState;

/* command model */

typedef enum
{
    CMD_INVALID = 0,
    CMD_PING,
    CMD_GET_STATUS,
    CMD_CHECK_SENSOR,
    CMD_DEBUG_I2C,
    CMD_INIT_SENSOR,
    CMD_SAMPLE_IMU,
    CMD_GET_IMU,
    CMD_GET_SENSOR_STATUS,
    CMD_SET_MODE,
    CMD_INJECT_FAULT,
    CMD_CLEAR_FAULTS
} CommandId;

typedef struct
{
    CommandId id;
    SystemMode requested_mode;
    SystemFault requested_fault;
} ParsedCommand;

/* MPU6050 on GY-521 */

#define MPU6050_I2C_ADDR_7BIT (0x68u)
#define MPU6050_I2C_ADDR_HAL (MPU6050_I2C_ADDR_7BIT << 1)

#define MPU6050_REG_GYRO_CONFIG (0x1Bu)
#define MPU6050_REG_ACCEL_CONFIG (0x1Cu)
#define MPU6050_REG_ACCEL_XOUT_H (0x3Bu)
#define MPU6050_REG_PWR_MGMT_1 (0x6Bu)
#define MPU6050_REG_WHO_AM_I (0x75u)

#define MPU6050_EXPECTED_WHO_AM_I (0x68u)

/* command and timing constants */

#define COMMAND_BUFFER_SIZE (64u)

#define HEALTH_PERIOD_MS (10000u)
#define SENSOR_CHECK_PERIOD_MS (3000u)
#define IMU_SAMPLE_PERIOD_MS (1000u)

#define I2C_TIMEOUT_MS (100u)
#define I2C_RECOVERY_DELAY_MS (1u)
#define SENSOR_RETRY_DELAY_MS (50u)
#define SENSOR_WAKE_DELAY_MS (100u)

#define COMMAND_STORM_WINDOW_MS (1000u)
#define COMMAND_STORM_LIMIT (8u)

/* REQ-HW-LCD-001:
 * LCD1602 shall be connected in 4-bit mode using the Nucleo D2-D7 mapping.
 *
 * LCD1602 / HD44780-compatible display
 *
 * Chosen wiring:
 *   RS -> Nucleo D2 / PA10
 *   E  -> Nucleo D3 / PB3
 *   D4 -> Nucleo D4 / PB5
 *   D5 -> Nucleo D5 / PB4
 *   D6 -> Nucleo D6 / PB10
 *   D7 -> Nucleo D7 / PA8
 *
 * LCD RW must be tied to GND.
 * LCD D0-D3 are unused in 4-bit mode.
 */

#define LCD_RS_PORT GPIOA
#define LCD_RS_PIN GPIO_PIN_10 /* Nucleo D2 */

#define LCD_E_PORT GPIOB
#define LCD_E_PIN GPIO_PIN_3 /* Nucleo D3 */

#define LCD_D4_PORT GPIOB
#define LCD_D4_PIN GPIO_PIN_5 /* Nucleo D4 */

#define LCD_D5_PORT GPIOB
#define LCD_D5_PIN GPIO_PIN_4 /* Nucleo D5 */

#define LCD_D6_PORT GPIOB
#define LCD_D6_PIN GPIO_PIN_10 /* Nucleo D6 */

#define LCD_D7_PORT GPIOA
#define LCD_D7_PIN GPIO_PIN_8 /* Nucleo D7 */

#define LCD_WIDTH (16u)

/* global handles */

static UART_HandleTypeDef huart2;
static I2C_HandleTypeDef hi2c1;

/* setup */

static void SystemClock_Config(void);
static void GPIO_Init(void);
static void I2C1_GPIO_Init(void);
static void LCD_GPIO_Init(void);
static void USART2_UART_Init(void);
static void I2C1_Init(void);
static void I2C1_Bus_Recover(void);
static void Error_Handler(void);

void SysTick_Handler(void);

/* telemetry */

static void telemetry_write(const char *text);
static void telemetry_write_line(const char *line);

static void
telemetry_emit(AppState *app, const char *record_type, const char *fields);

static void
telemetry_tlm(AppState *app, const char *tlm_type, const char *fields);

static void telemetry_event(AppState *app, const char *event_fields);

static void
telemetry_ack(AppState *app, uint32_t cmd_seq, const char *cmd_name);

static void telemetry_ack_detail(AppState *app,
                                 uint32_t cmd_seq,
                                 const char *cmd_name,
                                 const char *details);

static void
telemetry_reject(AppState *app, uint32_t cmd_seq, const char *reason);

static void telemetry_reject_detail(AppState *app,
                                    uint32_t cmd_seq,
                                    const char *reason,
                                    const char *details);

static void telemetry_deny(AppState *app,
                           uint32_t cmd_seq,
                           const char *reason,
                           const char *details);

/* command input */

static void command_process_byte(
    uint8_t byte, char *buffer, size_t *length, size_t capacity, AppState *app);

static void command_handle_line(const char *line, AppState *app);
static ParsedCommand command_parse(const char *line);
static void
command_execute(const ParsedCommand *cmd, AppState *app, uint32_t cmd_seq);
static uint8_t command_rate_note(CommandRateState *rate);

/* modes and faults */

static const char *mode_to_string(SystemMode mode);
static const char *fault_to_string(SystemFault fault);
static const char *sensor_status_to_string(SensorStatus status);

static void
request_mode_change(AppState *app, SystemMode requested, uint32_t cmd_seq);

static uint8_t mode_transition_allowed(SystemMode from, SystemMode to);

static void raise_fault(AppState *app, SystemFault fault);
static void clear_faults(AppState *app, uint32_t cmd_seq);

/* sensor */

static SensorStatus mpu6050_read_whoami(uint8_t *whoami);
static SensorStatus mpu6050_init_sensor(void);
static SensorStatus mpu6050_read_sample(ImuSample *sample);
static HAL_StatusTypeDef mpu6050_write_register(uint8_t reg, uint8_t value);
static int16_t combine_i16(uint8_t high, uint8_t low);

static void sensor_check_and_report(AppState *app, uint32_t cmd_seq);
static void sensor_cached_report(AppState *app, uint32_t cmd_seq);

static void imu_sample_and_report(AppState *app, uint32_t cmd_seq);
static void imu_cached_report(AppState *app, uint32_t cmd_seq);

/* LCD */

static void lcd_init(void);
static void lcd_command(uint8_t command);
static void lcd_data(uint8_t data);
static void lcd_write_byte(uint8_t value, GPIO_PinState rs);
static void lcd_write4(uint8_t nibble);
static void lcd_set_data_nibble(uint8_t nibble);
static void lcd_pulse_enable(void);
static void lcd_clear(void);
static void lcd_set_cursor(uint8_t row, uint8_t col);
static void lcd_print_fixed(const char *text);
static void lcd_show_status(SystemMode mode, SystemFault fault);
static void lcd_show_imu(const ImuSample *sample);
static void lcd_show_status_page(AppState *app);
static void lcd_show_imu_page(AppState *app, const ImuSample *sample);
static void lcd_refresh(AppState *app);

int main(void)
{
    uint32_t last_health_tick = 0;
    uint32_t health_counter = 0;

    uint32_t last_sensor_check_tick = 0;
    uint32_t last_imu_sample_tick = 0;

    char command_buffer[COMMAND_BUFFER_SIZE] = {0};
    size_t command_len = 0;

    AppState app = {0};

    app.mode = MODE_BOOT;
    app.fault = FAULT_NONE;
    app.lcd_page = LCD_PAGE_STATUS;
    app.command_rate.window_start_tick = 0;
    app.command_rate.count = 0u;

    HAL_Init();
    SystemClock_Config();

    GPIO_Init();
    USART2_UART_Init();
    I2C1_Init();
    lcd_init();

    lcd_show_status_page(&app);

    telemetry_event(&app, "type=BOOT");
    telemetry_tlm(&app, "FW", "name=pocketsat-mk1,version=0.5.0");
    telemetry_tlm(&app, "BOARD", "type=NUCLEO-F401RE");
    telemetry_tlm(&app, "MCU", "type=STM32F401RE");
    telemetry_tlm(&app, "MODE", "current=BOOT");

    app.mode = MODE_SAFE;
    telemetry_event(&app,
                    "type=MODE_CHANGE,from=BOOT,to=SAFE,reason=boot_complete");
    lcd_show_status_page(&app);

    while (1)
    {
        uint32_t now = HAL_GetTick();

        if ((now - last_health_tick) >= HEALTH_PERIOD_MS)
        {
            char fields[128];

            last_health_tick = now;
            health_counter++;

            HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);

            snprintf(fields,
                     sizeof(fields),
                     "counter=%lu,mode=%s,fault=%s",
                     (unsigned long)health_counter,
                     mode_to_string(app.mode),
                     fault_to_string(app.fault));

            telemetry_tlm(&app, "HEALTH", fields);
            lcd_refresh(&app);
        }

        if ((now - last_sensor_check_tick) >= SENSOR_CHECK_PERIOD_MS)
        {
            last_sensor_check_tick = now;
            sensor_check_and_report(&app, 0u);
        }

        if (app.fault == FAULT_NONE &&
            (app.mode == MODE_NOMINAL || app.mode == MODE_PAYLOAD) &&
            (now - last_imu_sample_tick) >= IMU_SAMPLE_PERIOD_MS)
        {
            last_imu_sample_tick = now;
            imu_sample_and_report(&app, 0u);
        }

        uint8_t rx_byte = 0;

        if (HAL_UART_Receive(&huart2, &rx_byte, 1, 0) == HAL_OK)
        {
            command_process_byte(rx_byte,
                                 command_buffer,
                                 &command_len,
                                 sizeof(command_buffer),
                                 &app);
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

    /* LCD1602 control/data pins */
    LCD_GPIO_Init();
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

static void LCD_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    gpio.Pin = LCD_RS_PIN | LCD_D7_PIN;
    HAL_GPIO_Init(GPIOA, &gpio);

    gpio.Pin = LCD_E_PIN | LCD_D4_PIN | LCD_D5_PIN | LCD_D6_PIN;
    HAL_GPIO_Init(GPIOB, &gpio);

    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D4_PORT, LCD_D4_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D5_PORT, LCD_D5_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D6_PORT, LCD_D6_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_D7_PORT, LCD_D7_PIN, GPIO_PIN_RESET);
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

static void I2C1_Bus_Recover(void)
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
        &huart2, (uint8_t *)text, (uint16_t)strlen(text), HAL_MAX_DELAY);
}

static void telemetry_write_line(const char *line)
{
    telemetry_write(line);
    telemetry_write("\r\n");
}

static void
telemetry_emit(AppState *app, const char *record_type, const char *fields)
{
    char line[224];

    app->telemetry.telemetry_seq++;

    if (fields != NULL && fields[0] != '\0')
    {
        snprintf(line,
                 sizeof(line),
                 "%s,seq=%lu,tick=%lu,%s",
                 record_type,
                 (unsigned long)app->telemetry.telemetry_seq,
                 (unsigned long)HAL_GetTick(),
                 fields);
    }
    else
    {
        snprintf(line,
                 sizeof(line),
                 "%s,seq=%lu,tick=%lu",
                 record_type,
                 (unsigned long)app->telemetry.telemetry_seq,
                 (unsigned long)HAL_GetTick());
    }

    telemetry_write_line(line);
}

static void
telemetry_tlm(AppState *app, const char *tlm_type, const char *fields)
{
    char payload[176];

    if (fields != NULL && fields[0] != '\0')
    {
        snprintf(payload, sizeof(payload), "type=%s,%s", tlm_type, fields);
    }
    else
    {
        snprintf(payload, sizeof(payload), "type=%s", tlm_type);
    }

    telemetry_emit(app, "TLM", payload);
}

static void telemetry_event(AppState *app, const char *event_fields)
{
    char payload[176];

    app->telemetry.event_seq++;

    if (event_fields != NULL && event_fields[0] != '\0')
    {
        snprintf(payload,
                 sizeof(payload),
                 "event_id=%lu,%s",
                 (unsigned long)app->telemetry.event_seq,
                 event_fields);
    }
    else
    {
        snprintf(payload,
                 sizeof(payload),
                 "event_id=%lu",
                 (unsigned long)app->telemetry.event_seq);
    }

    telemetry_emit(app, "EVENT", payload);
}

static void telemetry_ack(AppState *app, uint32_t cmd_seq, const char *cmd_name)
{
    char fields[96];

    snprintf(fields,
             sizeof(fields),
             "cmd_seq=%lu,cmd=%s",
             (unsigned long)cmd_seq,
             cmd_name);

    telemetry_emit(app, "ACK", fields);
}

static void telemetry_ack_detail(AppState *app,
                                 uint32_t cmd_seq,
                                 const char *cmd_name,
                                 const char *details)
{
    char fields[144];

    if (details != NULL && details[0] != '\0')
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,cmd=%s,%s",
                 (unsigned long)cmd_seq,
                 cmd_name,
                 details);
    }
    else
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,cmd=%s",
                 (unsigned long)cmd_seq,
                 cmd_name);
    }

    telemetry_emit(app, "ACK", fields);
}

static void
telemetry_reject(AppState *app, uint32_t cmd_seq, const char *reason)
{
    telemetry_reject_detail(app, cmd_seq, reason, "");
}

static void telemetry_reject_detail(AppState *app,
                                    uint32_t cmd_seq,
                                    const char *reason,
                                    const char *details)
{
    char fields[160];

    if (details != NULL && details[0] != '\0')
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,reason=%s,%s",
                 (unsigned long)cmd_seq,
                 reason,
                 details);
    }
    else
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,reason=%s",
                 (unsigned long)cmd_seq,
                 reason);
    }

    telemetry_emit(app, "REJECT", fields);
}

static void telemetry_deny(AppState *app,
                           uint32_t cmd_seq,
                           const char *reason,
                           const char *details)
{
    char fields[160];

    if (details != NULL && details[0] != '\0')
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,reason=%s,%s",
                 (unsigned long)cmd_seq,
                 reason,
                 details);
    }
    else
    {
        snprintf(fields,
                 sizeof(fields),
                 "cmd_seq=%lu,reason=%s",
                 (unsigned long)cmd_seq,
                 reason);
    }

    telemetry_emit(app, "DENY", fields);
}

/* command input */

static void command_process_byte(
    uint8_t byte, char *buffer, size_t *length, size_t capacity, AppState *app)
{
    if (byte == '\r')
    {
        return;
    }

    if (byte == '\n')
    {
        buffer[*length] = '\0';

        command_handle_line(buffer, app);

        *length = 0;
        buffer[0] = '\0';

        return;
    }

    if (*length >= capacity - 1u)
    {
        telemetry_reject(app, 0u, "COMMAND_BUFFER_OVERFLOW");

        *length = 0;
        buffer[0] = '\0';

        return;
    }

    buffer[*length] = (char)byte;
    *length = *length + 1u;
    buffer[*length] = '\0';
}

static void command_handle_line(const char *line, AppState *app)
{
    ParsedCommand cmd = {0};
    uint32_t cmd_seq = 0;

    app->telemetry.command_seq++;
    cmd_seq = app->telemetry.command_seq;

    if (command_rate_note(&app->command_rate) != 0u)
    {
        telemetry_reject(app, cmd_seq, "COMMAND_STORM");
        raise_fault(app, FAULT_COMMAND_STORM);
        lcd_show_status_page(app);
        return;
    }

    if (strcmp(line, "") == 0)
    {
        telemetry_reject(app, cmd_seq, "EMPTY_COMMAND");
        return;
    }

    cmd = command_parse(line);

    command_execute(&cmd, app, cmd_seq);
}

static ParsedCommand command_parse(const char *line)
{
    ParsedCommand cmd = {0};

    cmd.id = CMD_INVALID;
    cmd.requested_mode = MODE_SAFE;
    cmd.requested_fault = FAULT_NONE;

    if (strcmp(line, "PING") == 0)
    {
        cmd.id = CMD_PING;
    }
    else if (strcmp(line, "GET_STATUS") == 0)
    {
        cmd.id = CMD_GET_STATUS;
    }
    else if (strcmp(line, "CHECK_SENSOR") == 0)
    {
        cmd.id = CMD_CHECK_SENSOR;
    }
    else if (strcmp(line, "DEBUG_I2C") == 0)
    {
        cmd.id = CMD_DEBUG_I2C;
    }
    else if (strcmp(line, "INIT_SENSOR") == 0)
    {
        cmd.id = CMD_INIT_SENSOR;
    }
    else if (strcmp(line, "SAMPLE_IMU") == 0)
    {
        cmd.id = CMD_SAMPLE_IMU;
    }
    else if (strcmp(line, "GET_IMU") == 0)
    {
        cmd.id = CMD_GET_IMU;
    }
    else if (strcmp(line, "GET_SENSOR_STATUS") == 0)
    {
        cmd.id = CMD_GET_SENSOR_STATUS;
    }
    else if (strcmp(line, "SET_MODE SAFE") == 0)
    {
        cmd.id = CMD_SET_MODE;
        cmd.requested_mode = MODE_SAFE;
    }
    else if (strcmp(line, "SET_MODE NOMINAL") == 0)
    {
        cmd.id = CMD_SET_MODE;
        cmd.requested_mode = MODE_NOMINAL;
    }
    else if (strcmp(line, "SET_MODE PAYLOAD") == 0)
    {
        cmd.id = CMD_SET_MODE;
        cmd.requested_mode = MODE_PAYLOAD;
    }
    else if (strcmp(line, "INJECT_FAULT SENSOR_MISSING") == 0)
    {
        cmd.id = CMD_INJECT_FAULT;
        cmd.requested_fault = FAULT_SENSOR_MISSING;
    }
    else if (strcmp(line, "INJECT_FAULT OVERTEMP") == 0)
    {
        cmd.id = CMD_INJECT_FAULT;
        cmd.requested_fault = FAULT_OVERTEMP;
    }
    else if (strcmp(line, "INJECT_FAULT COMMAND_STORM") == 0)
    {
        cmd.id = CMD_INJECT_FAULT;
        cmd.requested_fault = FAULT_COMMAND_STORM;
    }
    else if (strcmp(line, "CLEAR_FAULTS") == 0)
    {
        cmd.id = CMD_CLEAR_FAULTS;
    }

    return cmd;
}

static void
command_execute(const ParsedCommand *cmd, AppState *app, uint32_t cmd_seq)
{
    switch (cmd->id)
    {
        case CMD_PING:
            telemetry_ack(app, cmd_seq, "PING");
            break;

        case CMD_GET_STATUS:
        {
            char fields[96];

            snprintf(fields,
                     sizeof(fields),
                     "cmd_seq=%lu,mode=%s,fault=%s",
                     (unsigned long)cmd_seq,
                     mode_to_string(app->mode),
                     fault_to_string(app->fault));

            telemetry_tlm(app, "STATUS", fields);
            lcd_show_status_page(app);
            break;
        }

        case CMD_CHECK_SENSOR:
            sensor_check_and_report(app, cmd_seq);
            break;

        case CMD_DEBUG_I2C:
        {
            uint32_t error = HAL_I2C_GetError(&hi2c1);
            GPIO_PinState scl = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8);
            GPIO_PinState sda = HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_9);
            char fields[128];

            snprintf(fields,
                     sizeof(fields),
                     "cmd_seq=%lu,error=0x%08lX,scl=%lu,sda=%lu",
                     (unsigned long)cmd_seq,
                     (unsigned long)error,
                     (unsigned long)scl,
                     (unsigned long)sda);

            telemetry_tlm(app, "I2C", fields);
            break;
        }

        case CMD_INIT_SENSOR:
        {
            SensorStatus status = mpu6050_init_sensor();

            if (status == SENSOR_OK)
            {
                telemetry_ack(app, cmd_seq, "INIT_SENSOR");
                sensor_check_and_report(app, cmd_seq);
            }
            else
            {
                char details[64];

                snprintf(details,
                         sizeof(details),
                         "status=%s",
                         sensor_status_to_string(status));

                telemetry_reject_detail(
                    app, cmd_seq, "INIT_SENSOR_FAILED", details);

                if (app->fault == FAULT_NONE)
                {
                    raise_fault(app, FAULT_SENSOR_MISSING);
                }
            }

            lcd_show_status_page(app);
            break;
        }

        case CMD_SAMPLE_IMU:
            imu_sample_and_report(app, cmd_seq);
            break;

        case CMD_GET_IMU:
            imu_cached_report(app, cmd_seq);
            break;

        case CMD_GET_SENSOR_STATUS:
            sensor_cached_report(app, cmd_seq);
            break;

        case CMD_SET_MODE:
            request_mode_change(app, cmd->requested_mode, cmd_seq);
            lcd_show_status_page(app);
            break;

        case CMD_INJECT_FAULT:
        {
            char details[64];

            snprintf(details,
                     sizeof(details),
                     "fault=%s",
                     fault_to_string(cmd->requested_fault));

            telemetry_ack_detail(app, cmd_seq, "INJECT_FAULT", details);
            raise_fault(app, cmd->requested_fault);
            lcd_show_status_page(app);
            break;
        }

        case CMD_CLEAR_FAULTS:
            clear_faults(app, cmd_seq);
            lcd_show_status_page(app);
            break;

        case CMD_INVALID:
        default:
            telemetry_reject(app, cmd_seq, "UNKNOWN_COMMAND");
            break;
    }
}

static uint8_t command_rate_note(CommandRateState *rate)
{
    uint32_t now = HAL_GetTick();

    if ((now - rate->window_start_tick) >= COMMAND_STORM_WINDOW_MS)
    {
        rate->window_start_tick = now;
        rate->count = 0u;
    }

    if (rate->count < UINT8_MAX)
    {
        rate->count++;
    }

    return (rate->count > COMMAND_STORM_LIMIT) ? 1u : 0u;
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

static void
request_mode_change(AppState *app, SystemMode requested, uint32_t cmd_seq)
{
    SystemMode previous = app->mode;

    if (app->fault != FAULT_NONE && requested != MODE_SAFE)
    {
        char details[96];

        snprintf(details,
                 sizeof(details),
                 "fault=%s,requested=%s",
                 fault_to_string(app->fault),
                 mode_to_string(requested));

        telemetry_deny(app, cmd_seq, "ACTIVE_FAULT", details);
        return;
    }

    if (mode_transition_allowed(previous, requested) == 0u)
    {
        char details[96];

        snprintf(details,
                 sizeof(details),
                 "from=%s,to=%s",
                 mode_to_string(previous),
                 mode_to_string(requested));

        telemetry_deny(app, cmd_seq, "MODE_NOT_ALLOWED", details);
        return;
    }

    if (previous == requested)
    {
        char details[64];

        snprintf(details,
                 sizeof(details),
                 "mode=%s,result=NO_CHANGE",
                 mode_to_string(requested));

        telemetry_ack_detail(app, cmd_seq, "SET_MODE", details);
        return;
    }

    app->mode = requested;

    {
        char details[64];

        snprintf(
            details, sizeof(details), "mode=%s", mode_to_string(requested));

        telemetry_ack_detail(app, cmd_seq, "SET_MODE", details);
    }

    {
        char event[96];

        snprintf(event,
                 sizeof(event),
                 "type=MODE_CHANGE,from=%s,to=%s,reason=command",
                 mode_to_string(previous),
                 mode_to_string(requested));

        telemetry_event(app, event);
    }
}

static uint8_t mode_transition_allowed(SystemMode from, SystemMode to)
{
    if (from == to)
    {
        return 1u;
    }

    if (to == MODE_SAFE)
    {
        return 1u;
    }

    if (from == MODE_SAFE && to == MODE_NOMINAL)
    {
        return 1u;
    }

    if (from == MODE_NOMINAL && to == MODE_PAYLOAD)
    {
        return 1u;
    }

    if (from == MODE_PAYLOAD && to == MODE_NOMINAL)
    {
        return 1u;
    }

    return 0u;
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

static void raise_fault(AppState *app, SystemFault fault)
{
    char event[128];

    if (fault == FAULT_NONE)
    {
        return;
    }

    if (app->fault == fault)
    {
        return;
    }

    app->fault = fault;

    snprintf(event,
             sizeof(event),
             "type=FAULT_RAISED,fault=%s",
             fault_to_string(fault));

    telemetry_event(app, event);

    if (app->mode != MODE_SAFE)
    {
        SystemMode previous = app->mode;
        app->mode = MODE_SAFE;

        snprintf(event,
                 sizeof(event),
                 "type=MODE_CHANGE,from=%s,to=SAFE,reason=fault",
                 mode_to_string(previous));

        telemetry_event(app, event);
    }
}

static void clear_faults(AppState *app, uint32_t cmd_seq)
{
    SystemFault previous = app->fault;

    app->fault = FAULT_NONE;

    telemetry_ack(app, cmd_seq, "CLEAR_FAULTS");

    if (previous != FAULT_NONE)
    {
        char event[96];

        snprintf(event,
                 sizeof(event),
                 "type=FAULT_CLEARED,fault=%s,reason=operator_command",
                 fault_to_string(previous));

        telemetry_event(app, event);
    }
    else
    {
        telemetry_event(app, "type=FAULT_CLEAR_REQUEST,result=NO_ACTIVE_FAULT");
    }
}

/* sensor */

static const char *sensor_status_to_string(SensorStatus status)
{
    switch (status)
    {
        case SENSOR_OK:
            return "OK";

        case SENSOR_MISSING:
            return "MISSING";

        case SENSOR_BAD_ID:
            return "BAD_ID";

        default:
            return "UNKNOWN";
    }
}

static HAL_StatusTypeDef mpu6050_write_register(uint8_t reg, uint8_t value)
{
    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&hi2c1,
                                                 MPU6050_I2C_ADDR_HAL,
                                                 reg,
                                                 I2C_MEMADD_SIZE_8BIT,
                                                 &value,
                                                 1,
                                                 I2C_TIMEOUT_MS);

    if (status != HAL_OK)
    {
        I2C1_Bus_Recover();

        status = HAL_I2C_Mem_Write(&hi2c1,
                                   MPU6050_I2C_ADDR_HAL,
                                   reg,
                                   I2C_MEMADD_SIZE_8BIT,
                                   &value,
                                   1,
                                   I2C_TIMEOUT_MS);
    }

    return status;
}

static SensorStatus mpu6050_read_whoami(uint8_t *whoami)
{
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1,
                                                MPU6050_I2C_ADDR_HAL,
                                                MPU6050_REG_WHO_AM_I,
                                                I2C_MEMADD_SIZE_8BIT,
                                                whoami,
                                                1,
                                                I2C_TIMEOUT_MS);

    if (status != HAL_OK)
    {
        I2C1_Bus_Recover();

        HAL_Delay(SENSOR_RETRY_DELAY_MS);

        status = HAL_I2C_Mem_Read(&hi2c1,
                                  MPU6050_I2C_ADDR_HAL,
                                  MPU6050_REG_WHO_AM_I,
                                  I2C_MEMADD_SIZE_8BIT,
                                  whoami,
                                  1,
                                  I2C_TIMEOUT_MS);
    }

    if (status != HAL_OK)
    {
        *whoami = 0u;
        return SENSOR_MISSING;
    }

    if (*whoami != MPU6050_EXPECTED_WHO_AM_I)
    {
        return SENSOR_BAD_ID;
    }

    return SENSOR_OK;
}

static SensorStatus mpu6050_init_sensor(void)
{
    uint8_t whoami = 0u;
    SensorStatus status = mpu6050_read_whoami(&whoami);

    if (status != SENSOR_OK)
    {
        return status;
    }

    /* Wake sensor: clear SLEEP bit in PWR_MGMT_1. */
    if (mpu6050_write_register(MPU6050_REG_PWR_MGMT_1, 0x00u) != HAL_OK)
    {
        return SENSOR_MISSING;
    }

    HAL_Delay(SENSOR_WAKE_DELAY_MS);

    /* +/-250 deg/s gyro, +/-2g accel. Keep first telemetry raw. */
    if (mpu6050_write_register(MPU6050_REG_GYRO_CONFIG, 0x00u) != HAL_OK)
    {
        return SENSOR_MISSING;
    }

    if (mpu6050_write_register(MPU6050_REG_ACCEL_CONFIG, 0x00u) != HAL_OK)
    {
        return SENSOR_MISSING;
    }

    return SENSOR_OK;
}

static int16_t combine_i16(uint8_t high, uint8_t low)
{
    return (int16_t)(((uint16_t)high << 8) | (uint16_t)low);
}

static SensorStatus mpu6050_read_sample(ImuSample *sample)
{
    uint8_t data[14] = {0};

    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1,
                                                MPU6050_I2C_ADDR_HAL,
                                                MPU6050_REG_ACCEL_XOUT_H,
                                                I2C_MEMADD_SIZE_8BIT,
                                                data,
                                                sizeof(data),
                                                I2C_TIMEOUT_MS);

    if (status != HAL_OK)
    {
        I2C1_Bus_Recover();

        status = HAL_I2C_Mem_Read(&hi2c1,
                                  MPU6050_I2C_ADDR_HAL,
                                  MPU6050_REG_ACCEL_XOUT_H,
                                  I2C_MEMADD_SIZE_8BIT,
                                  data,
                                  sizeof(data),
                                  I2C_TIMEOUT_MS);
    }

    if (status != HAL_OK)
    {
        return SENSOR_MISSING;
    }

    sample->ax = combine_i16(data[0], data[1]);
    sample->ay = combine_i16(data[2], data[3]);
    sample->az = combine_i16(data[4], data[5]);
    sample->temp = combine_i16(data[6], data[7]);
    sample->gx = combine_i16(data[8], data[9]);
    sample->gy = combine_i16(data[10], data[11]);
    sample->gz = combine_i16(data[12], data[13]);

    return SENSOR_OK;
}

static void sensor_check_and_report(AppState *app, uint32_t cmd_seq)
{
    uint8_t whoami = 0u;
    SensorStatus status = mpu6050_read_whoami(&whoami);
    char fields[144];

    app->sensor.latest_sensor_status = status;
    app->sensor.latest_whoami = whoami;
    app->sensor.latest_sensor_tick = HAL_GetTick();
    app->sensor.sensor_status_valid = 1u;

    if (cmd_seq != 0u)
    {
        snprintf(
            fields,
            sizeof(fields),
            "cmd_seq=%lu,module=GY521,chip=MPU6050,status=%s,whoami=0x%02X",
            (unsigned long)cmd_seq,
            sensor_status_to_string(status),
            whoami);
    }
    else
    {
        snprintf(fields,
                 sizeof(fields),
                 "module=GY521,chip=MPU6050,status=%s,whoami=0x%02X",
                 sensor_status_to_string(status),
                 whoami);
    }

    telemetry_tlm(app, "SENSOR", fields);

    if (status == SENSOR_OK)
    {
        if (app->fault == FAULT_SENSOR_MISSING)
        {
            app->fault = FAULT_NONE;

            telemetry_event(app,
                            "type=FAULT_CLEARED,fault=SENSOR_MISSING,reason="
                            "sensor_recovered");

            telemetry_event(app,
                            "type=RECOVERY_ACTION,action=remain_in_safe,reason="
                            "operator_required");

            lcd_show_status_page(app);
        }

        return;
    }

    if (app->fault == FAULT_NONE)
    {
        raise_fault(app, FAULT_SENSOR_MISSING);
        lcd_show_status_page(app);
    }
}

static void sensor_cached_report(AppState *app, uint32_t cmd_seq)
{
    char fields[128];

    if (app->sensor.sensor_status_valid == 0u)
    {
        telemetry_reject(app, cmd_seq, "NO_VALID_SENSOR_STATUS");
        return;
    }

    snprintf(fields,
             sizeof(fields),
             "cmd_seq=%lu,age_ms=%lu,status=%s,whoami=0x%02X",
             (unsigned long)cmd_seq,
             (unsigned long)(HAL_GetTick() - app->sensor.latest_sensor_tick),
             sensor_status_to_string(app->sensor.latest_sensor_status),
             app->sensor.latest_whoami);

    telemetry_tlm(app, "SENSOR_STATUS_CACHED", fields);
}

static void imu_sample_and_report(AppState *app, uint32_t cmd_seq)
{
    ImuSample sample = {0};
    SensorStatus status = mpu6050_read_sample(&sample);

    if (status != SENSOR_OK)
    {
        char fields[96];

        if (cmd_seq != 0u)
        {
            char details[64];

            snprintf(details,
                     sizeof(details),
                     "status=%s",
                     sensor_status_to_string(status));

            telemetry_reject_detail(app, cmd_seq, "SAMPLE_IMU_FAILED", details);
        }

        snprintf(fields,
                 sizeof(fields),
                 "status=%s",
                 sensor_status_to_string(status));

        telemetry_tlm(app, "IMU", fields);

        if (app->fault == FAULT_NONE)
        {
            raise_fault(app, FAULT_SENSOR_MISSING);
            lcd_show_status_page(app);
        }

        return;
    }

    app->sensor.latest_imu = sample;
    app->sensor.latest_imu_tick = HAL_GetTick();
    app->sensor.imu_valid = 1u;

    {
        char fields[192];

        if (cmd_seq != 0u)
        {
            snprintf(fields,
                     sizeof(fields),
                     "cmd_seq=%lu,source=LIVE,ax=%d,ay=%d,az=%d,temp=%d,gx=%d,"
                     "gy=%d,gz=%d",
                     (unsigned long)cmd_seq,
                     (int)sample.ax,
                     (int)sample.ay,
                     (int)sample.az,
                     (int)sample.temp,
                     (int)sample.gx,
                     (int)sample.gy,
                     (int)sample.gz);
        }
        else
        {
            snprintf(fields,
                     sizeof(fields),
                     "source=LIVE,ax=%d,ay=%d,az=%d,temp=%d,gx=%d,gy=%d,gz=%d",
                     (int)sample.ax,
                     (int)sample.ay,
                     (int)sample.az,
                     (int)sample.temp,
                     (int)sample.gx,
                     (int)sample.gy,
                     (int)sample.gz);
        }

        telemetry_tlm(app, "IMU", fields);
    }

    lcd_show_imu_page(app, &sample);
}

static void imu_cached_report(AppState *app, uint32_t cmd_seq)
{
    char fields[192];
    const ImuSample *sample = NULL;

    if (app->sensor.imu_valid == 0u)
    {
        telemetry_reject(app, cmd_seq, "NO_VALID_IMU_SAMPLE");
        return;
    }

    sample = &app->sensor.latest_imu;

    snprintf(
        fields,
        sizeof(fields),
        "cmd_seq=%lu,age_ms=%lu,ax=%d,ay=%d,az=%d,temp=%d,gx=%d,gy=%d,gz=%d",
        (unsigned long)cmd_seq,
        (unsigned long)(HAL_GetTick() - app->sensor.latest_imu_tick),
        (int)sample->ax,
        (int)sample->ay,
        (int)sample->az,
        (int)sample->temp,
        (int)sample->gx,
        (int)sample->gy,
        (int)sample->gz);

    telemetry_tlm(app, "IMU_CACHED", fields);
    lcd_show_imu_page(app, sample);
}

/* LCD */

static void lcd_init(void)
{
    HAL_Delay(50);

    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_RESET);

    lcd_write4(0x03u);
    HAL_Delay(5);

    lcd_write4(0x03u);
    HAL_Delay(5);

    lcd_write4(0x03u);
    HAL_Delay(1);

    lcd_write4(0x02u);
    HAL_Delay(1);

    lcd_command(0x28u); /* 4-bit, 2-line, 5x8 font */
    lcd_command(0x0Cu); /* display on, cursor off, blink off */
    lcd_command(0x06u); /* entry mode: increment, no shift */
    lcd_clear();
}

static void lcd_command(uint8_t command)
{
    lcd_write_byte(command, GPIO_PIN_RESET);

    if (command == 0x01u || command == 0x02u)
    {
        HAL_Delay(2);
    }
}

static void lcd_data(uint8_t data)
{
    lcd_write_byte(data, GPIO_PIN_SET);
}

static void lcd_write_byte(uint8_t value, GPIO_PinState rs)
{
    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, rs);

    lcd_write4((uint8_t)(value >> 4));
    lcd_write4((uint8_t)(value & 0x0Fu));
}

static void lcd_write4(uint8_t nibble)
{
    lcd_set_data_nibble(nibble);
    lcd_pulse_enable();
}

static void lcd_set_data_nibble(uint8_t nibble)
{
    HAL_GPIO_WritePin(LCD_D4_PORT,
                      LCD_D4_PIN,
                      (nibble & 0x01u) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_D5_PORT,
                      LCD_D5_PIN,
                      (nibble & 0x02u) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_D6_PORT,
                      LCD_D6_PIN,
                      (nibble & 0x04u) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_D7_PORT,
                      LCD_D7_PIN,
                      (nibble & 0x08u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void lcd_pulse_enable(void)
{
    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);

    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_SET);
    HAL_Delay(1);

    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_RESET);
    HAL_Delay(1);
}

static void lcd_clear(void)
{
    lcd_command(0x01u);
    HAL_Delay(2);
}

static void lcd_set_cursor(uint8_t row, uint8_t col)
{
    uint8_t address = 0x00u;

    if (row != 0u)
    {
        address = 0x40u;
    }

    address = (uint8_t)(address + col);

    lcd_command((uint8_t)(0x80u | address));
}

static void lcd_print_fixed(const char *text)
{
    uint8_t i = 0u;

    for (i = 0u; i < LCD_WIDTH; i++)
    {
        if (text[i] == '\0')
        {
            break;
        }

        lcd_data((uint8_t)text[i]);
    }

    for (; i < LCD_WIDTH; i++)
    {
        lcd_data((uint8_t)' ');
    }
}

static void lcd_show_status(SystemMode mode, SystemFault fault)
{
    char row0[17];
    char row1[17];

    snprintf(row0, sizeof(row0), "M:%s", mode_to_string(mode));
    snprintf(row1, sizeof(row1), "F:%s", fault_to_string(fault));

    lcd_set_cursor(0u, 0u);
    lcd_print_fixed(row0);

    lcd_set_cursor(1u, 0u);
    lcd_print_fixed(row1);
}

static void lcd_show_imu(const ImuSample *sample)
{
    char row0[17];
    char row1[17];

    snprintf(
        row0, sizeof(row0), "AX:%d AY:%d", (int)sample->ax, (int)sample->ay);

    snprintf(
        row1, sizeof(row1), "AZ:%d GZ:%d", (int)sample->az, (int)sample->gz);

    lcd_set_cursor(0u, 0u);
    lcd_print_fixed(row0);

    lcd_set_cursor(1u, 0u);
    lcd_print_fixed(row1);
}

static void lcd_show_status_page(AppState *app)
{
    app->lcd_page = LCD_PAGE_STATUS;
    lcd_show_status(app->mode, app->fault);
}

static void lcd_show_imu_page(AppState *app, const ImuSample *sample)
{
    app->lcd_page = LCD_PAGE_IMU;
    lcd_show_imu(sample);
}

static void lcd_refresh(AppState *app)
{
    if (app->lcd_page == LCD_PAGE_IMU && app->sensor.imu_valid != 0u)
    {
        lcd_show_imu(&app->sensor.latest_imu);
    }
    else
    {
        lcd_show_status(app->mode, app->fault);
    }
}