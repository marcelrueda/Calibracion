// imu_spi.c
#include "imu_spi.h"

#include <stdbool.h>
#include "esp_log.h"

static const char *TAG = "IMU_SPI";
static bool s_bus_ready = false;

esp_err_t imu_spi_init(void)
{
    if (s_bus_ready)
    {
        return ESP_OK;
    }

    const spi_bus_config_t cfg = {
        .mosi_io_num = IMU_SPI_MOSI,
        .miso_io_num = IMU_SPI_MISO,
        .sclk_io_num = IMU_SPI_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 64,
    };
    esp_err_t ret = spi_bus_initialize(IMU_SPI_HOST, &cfg, SPI_DMA_CH_AUTO);
    if (ret == ESP_OK)
    {
        s_bus_ready = true;
        ESP_LOGI(TAG, "Bus SPI listo (SCLK=%d MOSI=%d MISO=%d)", IMU_SPI_SCLK, IMU_SPI_MOSI, IMU_SPI_MISO);
    }
    return ret;
}

esp_err_t imu_spi_add_device(int cs_gpio, int clock_hz, spi_device_handle_t *out)
{
    esp_err_t ret = imu_spi_init();
    if (ret != ESP_OK)
    {
        return ret;
    }

    const spi_device_interface_config_t dev = {
        .clock_speed_hz = clock_hz,
        .mode = 0,
        .spics_io_num = cs_gpio,
        .queue_size = 1,
    };
    ret = spi_bus_add_device(IMU_SPI_HOST, &dev, out);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Dispositivo agregado: CS=%d a %d Hz", cs_gpio, clock_hz);
    }
    return ret;
}
