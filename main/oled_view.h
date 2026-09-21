// oled_view.h
// Pantalla OLED SSD1306 (128x64, I2C 0x3C): acelerómetro del ICM e inclinómetro SCL3400.
#ifndef OLED_VIEW_H
#define OLED_VIEW_H

#include <stdbool.h>
#include "esp_err.h"
#include "helper_i2c.h"

// Agrega la OLED al bus indicado y la inicializa. Si no responde devuelve error
// y el programa puede seguir usando solo la consola.
esp_err_t oled_view_init(HelperI2C *bus);

// Dibuja la aceleración X, Y, Z (g) y la inclinación X, Y (grados).
// Si un sensor no tiene dato válido, su bloque muestra el texto acc_msg / inc_msg
// (por ejemplo un código de error), o "sin datos" si es NULL.
// inc_warn=true agrega un "!" al título del inclinómetro (el sensor reporta banderas).
void oled_view_show_imu(bool acc_ok, float ax, float ay, float az, const char *acc_msg,
                        bool inc_ok, float inc_x_deg, float inc_y_deg, const char *inc_msg, bool inc_warn);

// Dos líneas de texto para avisos y errores. Ignora NULL.
void oled_view_message(const char *line1, const char *line2);

#endif // OLED_VIEW_H
