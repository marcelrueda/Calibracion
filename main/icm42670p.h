// icm42670p.h
// Acelerómetro del ICM-42670-P por SPI (CS en GPIO37).
// Por ahora solo acelerómetro: el giroscopio queda apagado.
#ifndef ICM42670P_H
#define ICM42670P_H

#include <stdint.h>
#include "esp_err.h"

#define ICM_CS_GPIO          37
#define ICM_SPI_HZ           10000000   // el chip admite hasta 24 MHz
#define ICM_ACCEL_LSB_PER_G  16384.0f   // ±2 g (datasheet, tabla 2)

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} icm_raw3_t;

// Configura ±2 g, 100 Hz, filtro 16 Hz, y verifica la configuración leyéndola de vuelta.
esp_err_t icm42670p_init(void);

esp_err_t icm42670p_read_accel_raw(icm_raw3_t *out);

// Aceleración en g (X, Y, Z).
esp_err_t icm42670p_read_accel_g(float g[3]);

#endif // ICM42670P_H
