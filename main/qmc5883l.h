// qmc5883l.h
// Driver del magnetómetro QMC5883L (I2C, dirección fija 0x0D)
#ifndef QMC5883L_H
#define QMC5883L_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define QMC5883L_I2C_ADDR       0x0D
#define QMC5883L_I2C_FREQ_HZ    400000
#define QMC5883L_LSB_PER_GAUSS  12000.0f   // Sensibilidad en rango ±2 G (datasheet, tabla 2)

// Bits del registro de estado 0x06
#define QMC5883L_STATUS_DRDY    0x01       // Dato nuevo listo
#define QMC5883L_STATUS_OVL     0x02       // Saturación en algún eje
#define QMC5883L_STATUS_DOR     0x04       // Se saltó una muestra sin leer

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t z;
} qmc5883l_raw_t;

// Agrega el sensor al bus indicado, lo resetea y lo deja en modo continuo
// (OSR=512, ±2 G, 100 Hz). Es idempotente. Devuelve ESP_OK solo si el sensor
// responde y los registros de configuración se leen de vuelta correctamente.
esp_err_t qmc5883l_init(i2c_master_bus_handle_t bus);

// Lee una muestra cruda.
//   ESP_OK               -> dato nuevo en *out
//   ESP_ERR_NOT_FINISHED -> aún no hay dato nuevo (DRDY=0), no es un error
//   otro                 -> error de bus
// *status (opcional) recibe el registro 0x06 (DRDY/OVL/DOR).
esp_err_t qmc5883l_read_raw(qmc5883l_raw_t *out, uint8_t *status);

// ETAPA 1: imprime lecturas por consola durante duration_ms.
void qmc5883l_debug_print(uint32_t duration_ms);

#endif // QMC5883L_H