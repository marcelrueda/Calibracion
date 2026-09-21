// console_in.c
#include "console_in.h"

#include <stddef.h>
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#define CONSOLE_UART   UART_NUM_0
#define RX_BUFFER_SIZE 256          // debe ser mayor que el FIFO de hardware (128)

esp_err_t console_in_init(void)
{
    if (uart_is_driver_installed(CONSOLE_UART))
    {
        return ESP_OK;
    }
    return uart_driver_install(CONSOLE_UART, RX_BUFFER_SIZE, 0, 0, NULL, 0);
}

int console_in_getc(uint32_t timeout_ms)
{
    uint8_t c = 0;
    const int n = uart_read_bytes(CONSOLE_UART, &c, 1, pdMS_TO_TICKS(timeout_ms));
    return (n == 1) ? (int)c : -1;
}

void console_in_flush(void)
{
    uart_flush_input(CONSOLE_UART);
}
