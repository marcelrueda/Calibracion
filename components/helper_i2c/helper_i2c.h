// File: components/helper_i2c/helper_i2c.h
// Helper functions for I2C communication
// This file provides functions to initialize the I2C bus, add devices, and perform read and write operations.

// Version: 0.1.0
// IDF version: 5.4
// Description: This file contains helper functions for I2C communication using the ESP-IDF framework.
// Ing: Carlos A. Vargas

#ifndef HELPER_I2C_H
#define HELPER_I2C_H

#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // Adelantar definición para usar punteros
    typedef struct HelperI2C HelperI2C;

    // Esta estructura representa un dispositivo esclavo en el bus I2C.
    typedef struct
    {
        i2c_master_dev_handle_t slave_handle; // Handle del dispositivo esclavo en el bus.
        uint8_t slave_address;                // Dirección del esclavo.
        HelperI2C *parent;                    // Referencia al bus al que pertenece.
    } HelperI2CDevice;

    // Esta estructura representa el bus I2C maestro en sí mismo.
    struct HelperI2C
    {
        i2c_master_bus_handle_t bus_handle; // Handle del bus I2C maestro.
        SemaphoreHandle_t mutex;            // Mutex para proteger el acceso al bus I2C.
        i2c_port_t port;                    // <--- NUEVO
    };

    esp_err_t helper_i2c_init(HelperI2C *i2c, i2c_port_t port, int scl_pin, int sda_pin, uint32_t freq_hz);
    esp_err_t helper_i2c_add_device(HelperI2C *i2c, HelperI2CDevice *device, uint8_t slave_addr, uint32_t freq_hz);
    esp_err_t helper_i2c_write(HelperI2CDevice *device, uint8_t *data, size_t data_len);
    esp_err_t helper_i2c_read(HelperI2CDevice *device, uint8_t *data, size_t data_len);
    void helper_i2c_scan(HelperI2C *i2c);

#ifdef __cplusplus
}
#endif

#endif // HELPER_I2C_H
