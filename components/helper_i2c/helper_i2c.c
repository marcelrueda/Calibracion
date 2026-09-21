// File: components/helper_i2c/helper_i2c.c
// Helper functions for I2C communication
// This file provides functions to initialize the I2C bus, add devices, and perform read and write operations.

// Version: 0.1.0
// IDF version: 5.4
// Description: This file contains helper functions for I2C communication using the ESP-IDF framework.
// Ing: Carlos A. Vargas

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_log.h"
#include "helper_i2c.h"

static const char *TAG = "HelperI2C";

esp_err_t helper_i2c_init(HelperI2C *i2c, i2c_port_t port, int scl_pin, int sda_pin, uint32_t freq_hz)
{
    if (i2c == NULL)
    {
        ESP_LOGE(TAG, "Invalid I2C helper pointer");
        return ESP_ERR_INVALID_ARG;
    }

    // Verificar si ya estaba inicializado
    if (i2c->bus_handle != NULL && i2c->mutex != NULL)
    {
        ESP_LOGW(TAG, "I2C bus on port %d already initialized, skipping re-init", i2c->port);
        return ESP_OK; // o ESP_ERR_INVALID_STATE si quieres marcar error
    }

    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = port,
        .scl_io_num = scl_pin,
        .sda_io_num = sda_pin,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&i2c_mst_config, &i2c->bus_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize I2C master bus (%d): %s", port, esp_err_to_name(ret));
        i2c->bus_handle = NULL;
        return ret;
    }

    i2c->mutex = xSemaphoreCreateMutex();
    if (i2c->mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create I2C mutex");
        i2c_del_master_bus(i2c->bus_handle); // liberar bus si falla
        i2c->bus_handle = NULL;
        return ESP_FAIL;
    }

    i2c->port = port;

    ESP_LOGI(TAG, "I2C master bus initialized successfully on port %d", port);
    return ESP_OK;
}

esp_err_t helper_i2c_add_device(HelperI2C *i2c, HelperI2CDevice *device, uint8_t slave_addr, uint32_t freq_hz)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = slave_addr,
        .scl_speed_hz = freq_hz};

    esp_err_t ret = i2c_master_bus_add_device(i2c->bus_handle, &dev_cfg, &device->slave_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add slave device at address 0x%02X: %s", slave_addr, esp_err_to_name(ret));
        return ret;
    }

    device->slave_address = slave_addr;
    device->parent = i2c; // <-- Asocia el bus al dispositivo

    ESP_LOGI(TAG, "Slave device at address 0x%02X added successfully", slave_addr);
    return ESP_OK;
}

esp_err_t helper_i2c_write(HelperI2CDevice *device, uint8_t *data, size_t data_len)
{
    if (device == NULL || device->slave_handle == NULL)
        return ESP_ERR_INVALID_ARG;

    HelperI2C *i2c = (HelperI2C *)device->parent;

    if (xSemaphoreTake(i2c->mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Timeout al tomar mutex I2C para escritura");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = i2c_master_transmit(device->slave_handle, data, data_len, -1);

    xSemaphoreGive(i2c->mutex);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to write to slave at address 0x%02X: %s", device->slave_address, esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t helper_i2c_read(HelperI2CDevice *device, uint8_t *data, size_t data_len)
{
    if (device == NULL || device->slave_handle == NULL)
        return ESP_ERR_INVALID_ARG;

    HelperI2C *i2c = (HelperI2C *)device->parent;

    if (xSemaphoreTake(i2c->mutex, pdMS_TO_TICKS(100)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Timeout al tomar mutex I2C para lectura");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = i2c_master_receive(device->slave_handle, data, data_len, -1);

    xSemaphoreGive(i2c->mutex);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read from slave at address 0x%02X: %s", device->slave_address, esp_err_to_name(ret));
    }
    return ret;
}

// Function to scan the I2C bus for devices
void helper_i2c_scan(HelperI2C *i2c)
{
    if (i2c == NULL || i2c->bus_handle == NULL)
    {
        ESP_LOGE(TAG, "Bus I2C no inicializado, no se puede escanear");
        return;
    }

    ESP_LOGI(TAG, "🔍 Iniciando escaneo del bus I2C...");

    int found = 0;

    for (uint8_t addr = 0x03; addr <= 0x77; addr++)
    {
        esp_err_t ret = i2c_master_probe(i2c->bus_handle, addr, pdMS_TO_TICKS(50));

        if (ret == ESP_OK)
        {
            ESP_LOGW(TAG, "Dispositivo detectado en 0x%02X", addr);
            found++;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (found == 0)
        ESP_LOGW(TAG, "No se detectó ningún dispositivo I2C");
    else
        ESP_LOGI(TAG, "Escaneo finalizado, %d dispositivo(s) detectado(s)", found);
}