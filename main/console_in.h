// console_in.h
// Lectura de teclas desde el monitor serie (UART0, el mismo COM3 de la consola).
#ifndef CONSOLE_IN_H
#define CONSOLE_IN_H

#include <stdint.h>
#include "esp_err.h"

// Instala el driver de UART0 para poder leer teclas. La salida por printf sigue funcionando.
esp_err_t console_in_init(void);

// Espera hasta timeout_ms una tecla. Devuelve el carácter, o -1 si no llegó ninguna.
int console_in_getc(uint32_t timeout_ms);

// Descarta lo que haya llegado sin leer.
void console_in_flush(void);

#endif // CONSOLE_IN_H
