// imu_spi.h
// Bus SPI3 compartido por el ICM-42670-P y el SCL3400 (mismas líneas, distinto CS).
#ifndef IMU_SPI_H
#define IMU_SPI_H

#include "esp_err.h"
#include "driver/spi_master.h"

#define IMU_SPI_HOST   SPI3_HOST
#define IMU_SPI_SCLK   40
#define IMU_SPI_MOSI   41
#define IMU_SPI_MISO   42

// Crea el bus una sola vez (idempotente).
esp_err_t imu_spi_init(void);

// Agrega un dispositivo al bus (modo SPI 0) con su CS y su velocidad.
esp_err_t imu_spi_add_device(int cs_gpio, int clock_hz, spi_device_handle_t *out);

#endif // IMU_SPI_H
