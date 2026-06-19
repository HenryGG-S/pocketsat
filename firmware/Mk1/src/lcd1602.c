#include "lcd1602.h"
#include "board_nucleo_f401re.h"
#include "fdir.h"
#include "mode.h"

#include <stdio.h>

/* REQ-HW-LCD-001:
 * LCD1602 shall be connected in 4-bit mode using the Nucleo D2-D7 mapping.
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
#define LCD_RS_PIN GPIO_PIN_10

#define LCD_E_PORT GPIOB
#define LCD_E_PIN GPIO_PIN_3

#define LCD_D4_PORT GPIOB
#define LCD_D4_PIN GPIO_PIN_5

#define LCD_D5_PORT GPIOB
#define LCD_D5_PIN GPIO_PIN_4

#define LCD_D6_PORT GPIOB
#define LCD_D6_PIN GPIO_PIN_10

#define LCD_D7_PORT GPIOA
#define LCD_D7_PIN GPIO_PIN_8

#define LCD_WIDTH (16u)

static void LCD_GPIO_Init(void);
static void lcd_command(uint8_t command);
static void lcd_data(uint8_t data);
static void lcd_write_byte(uint8_t value, GPIO_PinState rs);
static void lcd_write4(uint8_t nibble);
static void lcd_set_data_nibble(uint8_t nibble);
static void lcd_pulse_enable(void);
static void lcd_set_cursor(uint8_t row, uint8_t col);
static void lcd_print_fixed(const char *text);

void lcd_init(void)
{
    LCD_GPIO_Init();

    board_delay_ms(50u);

    HAL_GPIO_WritePin(LCD_RS_PORT, LCD_RS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_RESET);

    lcd_write4(0x03u);
    board_delay_ms(5u);

    lcd_write4(0x03u);
    board_delay_ms(5u);

    lcd_write4(0x03u);
    board_delay_ms(1u);

    lcd_write4(0x02u);
    board_delay_ms(1u);

    lcd_command(0x28u); /* 4-bit, 2-line, 5x8 font */
    lcd_command(0x0Cu); /* display on, cursor off, blink off */
    lcd_command(0x06u); /* entry mode: increment, no shift */
    lcd_clear();
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

static void lcd_command(uint8_t command)
{
    lcd_write_byte(command, GPIO_PIN_RESET);

    if (command == 0x01u || command == 0x02u)
    {
        board_delay_ms(2u);
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
    board_delay_ms(1u);

    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_SET);
    board_delay_ms(1u);

    HAL_GPIO_WritePin(LCD_E_PORT, LCD_E_PIN, GPIO_PIN_RESET);
    board_delay_ms(1u);
}

void lcd_clear(void)
{
    lcd_command(0x01u);
    board_delay_ms(2u);
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

void lcd_show_status(SystemMode mode, SystemFault fault)
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

void lcd_show_imu(const ImuSample *sample)
{
    char row0[17];
    char row1[17];

    snprintf(row0, sizeof(row0), "AX:%d AY:%d", (int)sample->ax, (int)sample->ay);
    snprintf(row1, sizeof(row1), "AZ:%d GZ:%d", (int)sample->az, (int)sample->gz);

    lcd_set_cursor(0u, 0u);
    lcd_print_fixed(row0);

    lcd_set_cursor(1u, 0u);
    lcd_print_fixed(row1);
}

void lcd_show_status_page(AppState *app)
{
    app->lcd_page = LCD_PAGE_STATUS;
    lcd_show_status(app->mode, app->fault);
}

void lcd_show_imu_page(AppState *app, const ImuSample *sample)
{
    app->lcd_page = LCD_PAGE_IMU;
    lcd_show_imu(sample);
}

void lcd_refresh(AppState *app)
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
