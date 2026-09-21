// icm42670p.h
// Acelerómetro y giroscopio del ICM-42670-P por SPI (CS en GPIO37).
#ifndef ICM42670P_H
#define ICM42670P_H

#include <stdint.h>
#include "esp_err.h"

#define ICM_CS_GPIO           37
#define ICM_SPI_HZ            10000000   // el chip admite hasta 24 MHz
#define ICM_ACCEL_LSB_PER_G   16384.0f   // ±2 g      (datasheet, tabla 2)
#define ICM_GYRO_LSB_PER_DPS  131.0f     // ±250 °/s   (datasheet, tabla 1)

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} icm_raw3_t;

typedef struct
{
    float acc_g[3];      // aceleración X, Y, Z en g
    float gyro_dps[3];   // velocidad angular X, Y, Z en grados por segundo
} icm_sample_t;

// Configura acelerómetro (±2 g, 100 Hz) y giroscopio (±250 °/s, 100 Hz), ambos con filtro
// de 16 Hz, y verifica la configuración leyéndola de vuelta.
esp_err_t icm42670p_init(void);

// Lee aceleración y giroscopio en una sola ráfaga SPI (las dos medidas son de la misma muestra).
esp_err_t icm42670p_read_sample(icm_sample_t *out);

esp_err_t icm42670p_read_accel_raw(icm_raw3_t *out);
esp_err_t icm42670p_read_accel_g(float g[3]);

#endif // ICM42670P_H
