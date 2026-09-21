// scl3400.h
// Inclinómetro Murata SCL3400-D01 por SPI (CS en GPIO38), modo A (±30°, 32768 LSB/g, filtro 10 Hz).
#ifndef SCL3400_H
#define SCL3400_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define SCL_CS_GPIO      38
#define SCL_SPI_HZ       1000000    // el firmware viejo usaba 1 MHz; el datasheet admite 0.1 a 8 MHz (2 a 4 MHz para el menor ruido)
#define SCL_LSB_PER_G    32768.0f   // modo A

// Bit 0 del resumen de estado: error de conexión interna (datasheet, tabla 23)
#define SCL_STATUS_PIN_CONTINUITY  0x0001

typedef struct
{
    int16_t raw_x;
    int16_t raw_y;
    float acc_x_g;      // aceleración en g
    float acc_y_g;
    float ang_x_deg;    // inclinación = asin(g), en grados
    float ang_y_deg;
    float temp_c;
    uint16_t status;    // resumen de estado si el sensor reportó banderas (0 = sin banderas)
} scl3400_data_t;

// Arranque según la tabla 10 del datasheet y verificación de WHOAMI (0xE0).
esp_err_t scl3400_init(void);

// Última trama de 32 bits recibida del sensor (para diagnóstico).
// 0xFFFFFFFF o 0x00000000 indican que no llega nada por MISO (cableado, CS o alimentación).
uint32_t scl3400_last_frame(void);

// Lee X, Y y temperatura con verificación de CRC y de los bits RS de cada respuesta.
// Si el sensor reporta SOLO la bandera PIN_CONTINUITY, devuelve ESP_OK con los datos y
// out->status distinto de 0: los datos se muestran, pero no son confiables para calibrar.
// Con cualquier otra bandera devuelve ESP_ERR_INVALID_RESPONSE.
esp_err_t scl3400_read(scl3400_data_t *out);

// Lee STATUS, ERR_FLAG1 y ERR_FLAG2 (con CRC). Leer STATUS limpia sus banderas; ERR_FLAG no.
esp_err_t scl3400_read_diag(uint16_t *status, uint16_t *err1, uint16_t *err2);

// Escribe en buf los nombres de las banderas activas (tablas 23, 28 y 30 del datasheet).
void scl3400_describe_flags(uint16_t status, uint16_t err1, uint16_t err2, char *buf, size_t n);

#endif // SCL3400_H
