#include "stm32f4xx_hal.h"

#include "app.h"
#include "app_config.h"
#include "app_types.h"
#include "board_nucleo_f401re.h"
#include "command.h"
#include "health_manager.h"

#include <stddef.h>
#include <stdint.h>

int main(void)
{
    AppState app = {0};
    char command_buffer[COMMAND_BUFFER_SIZE] = {0};
    size_t command_len = 0u;

    HAL_Init();

    board_init();
    app_init(&app);

    while (1)
    {
        uint8_t rx_byte = 0u;

        app_tick(&app);

        // TODO: consider asynchronously processing commands in a separate task
        // or interrupt handler
        if (board_uart_receive_byte(&rx_byte) != 0u)
        {
            command_process_byte(rx_byte,
                                 command_buffer,
                                 &command_len,
                                 sizeof(command_buffer),
                                 &app);
        }

        if (health_manager_system_alive(&app) != 0u)
        {
            board_watchdog_kick();
        }
    }
}
