#ifndef BOARD_NUCLEO_F401RE_H
#define BOARD_NUCLEO_F401RE_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

void board_init(void);
void board_error_handler(void);

uint32_t board_millis(void);
void board_delay_ms(uint32_t delay_ms);

void board_heartbeat_toggle(void);

void board_uart_write(const char *text);
uint8_t board_uart_receive_byte(uint8_t *byte);

HAL_StatusTypeDef board_i2c_mem_read(uint16_t dev_addr,
                                     uint16_t mem_addr,
                                     uint16_t mem_addr_size,
                                     uint8_t *data,
                                     uint16_t size,
                                     uint32_t timeout_ms);

HAL_StatusTypeDef board_i2c_mem_write(uint16_t dev_addr,
                                      uint16_t mem_addr,
                                      uint16_t mem_addr_size,
                                      uint8_t *data,
                                      uint16_t size,
                                      uint32_t timeout_ms);

void board_i2c1_recover(void);
uint32_t board_i2c1_error(void);
GPIO_PinState board_i2c1_scl_state(void);
GPIO_PinState board_i2c1_sda_state(void);

typedef enum
{
    BOARD_RESET_UNKNOWN = 0,
    BOARD_RESET_POWER_ON,
    BOARD_RESET_PIN,
    BOARD_RESET_SOFTWARE,
    BOARD_RESET_IWDG,
    BOARD_RESET_WWDG,
    BOARD_RESET_BROWNOUT,
    BOARD_RESET_LOW_POWER
} BoardResetCause;

BoardResetCause board_reset_cause_get(void);
const char *board_reset_cause_to_string(BoardResetCause cause);

void board_watchdog_init(void);
void board_watchdog_kick(void);

#endif
