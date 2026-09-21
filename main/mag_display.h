// mag_display.h
// Muestra en la OLED SSD1306 (128x64, I2C 0x3C) los valores del magnetómetro.
#ifndef MAG_DISPLAY_H
#define MAG_DISPLAY_H

#include <stdbool.h>
#include "esp_err.h"
#include "helper_i2c.h"

// Agrega la OLED al bus indicado y la inicializa. Si la pantalla no responde
// devuelve error y el resto del programa puede seguir usando solo la consola.
esp_err_t mag_display_init(HelperI2C *bus);

// Dibuja X, Y, Z (en gauss) y la magnitud |B|. saturated=true agrega "OVL".
void mag_display_show(float gx, float gy, float gz, bool saturated);

// Muestra dos líneas de texto (para avisos y errores). Ignora NULL.
void mag_display_message(const char *line1, const char *line2);

#endif // MAG_DISPLAY_H