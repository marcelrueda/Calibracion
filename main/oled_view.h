// oled_view.h
// Pantalla OLED SSD1306 (128x64, I2C 0x3C): acelerómetro, giroscopio, inclinómetro y magnetómetro.
#ifndef OLED_VIEW_H
#define OLED_VIEW_H

#include <stdbool.h>
#include "esp_err.h"
#include "helper_i2c.h"

typedef struct
{
    bool acc_ok;   float acc_g[3];     // aceleración X, Y, Z (g)
    bool gyr_ok;   float gyr_dps[3];   // giroscopio X, Y, Z (grados por segundo)
    bool inc_ok;   float inc_deg[2];   // inclinación X, Y (grados)
    bool inc_warn;                     // el inclinómetro reporta banderas: agrega "!" al título
    bool mag_ok;   float mag_g[3];     // campo magnético X, Y, Z (gauss)
} oled_data_t;

// Agrega la OLED al bus indicado y la inicializa. Si no responde devuelve error
// y el programa puede seguir usando solo la consola.
esp_err_t oled_view_init(HelperI2C *bus);

// Dibuja los cuatro sensores, cada uno con un título corto y sus valores.
// Un sensor sin dato válido muestra "sin datos".
void oled_view_show_all(const oled_data_t *d);

// Dos líneas de texto para avisos y errores. Ignora NULL.
void oled_view_message(const char *line1, const char *line2);

#endif // OLED_VIEW_H
