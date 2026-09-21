// qmc5883l.c
#include "qmc5883l.h"

#include <math.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "QMC5883L";

// Registros (datasheet, tabla 13)
#define REG_DATA_X_LSB   0x00   // 0x00..0x05: X,Y,Z (LSB primero, 16 bits, complemento a 2)
#define REG_STATUS       0x06
#define REG_CTRL1        0x09   // OSR[7:6] RNG[5:4] ODR[3:2] MODE[1:0]
#define REG_CTRL2        0x0A   // bit7 = soft reset
#define REG_SETRESET     0x0B   // periodo SET/RESET, recomendado 0x01

// Config: OSR=512 (00) | RNG=±2G (00) | ODR=100Hz (10) | MODE=continuo (01)
#define CTRL1_CONFIG     0x09
#define SETRESET_VALUE   0x01

#define I2C_TIMEOUT_MS   100

static i2c_master_dev_handle_t s_dev = NULL;
static bool s_ready = false;

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    const uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), I2C_TIMEOUT_MS);
}

// Lectura con START repetido (datasheet, sec. 8.2.4)
static esp_err_t reg_read(uint8_t reg, uint8_t *dst, size_t n)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, dst, n, I2C_TIMEOUT_MS);
}

esp_err_t qmc5883l_init(i2c_master_bus_handle_t bus)
{
    if (s_ready)
    {
        return ESP_OK;
    }
    if (bus == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_probe(bus, QMC5883L_I2C_ADDR, I2C_TIMEOUT_MS);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "No responde en 0x%02X (%s). Revisa conexión y dirección", QMC5883L_I2C_ADDR, esp_err_to_name(ret));
        return ret;
    }

    if (s_dev == NULL)
    {
        const i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = QMC5883L_I2C_ADDR,
            .scl_speed_hz = QMC5883L_I2C_FREQ_HZ,
        };
        ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
        if (ret != ESP_OK)
        {
            s_dev = NULL;
            return ret;
        }
    }

    // Soft reset -> standby con registros por defecto
    ret = reg_write(REG_CTRL2, 0x80);
    if (ret != ESP_OK)
    {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    // Configuración recomendada por el datasheet (sec. 7.1)
    if ((ret = reg_write(REG_SETRESET, SETRESET_VALUE)) != ESP_OK ||
        (ret = reg_write(REG_CTRL1, CTRL1_CONFIG)) != ESP_OK)
    {
        return ret;
    }

    // Verificación por relectura: confirma que es un QMC5883L y no otro chip
    uint8_t c1 = 0, sr = 0;
    if ((ret = reg_read(REG_CTRL1, &c1, 1)) != ESP_OK ||
        (ret = reg_read(REG_SETRESET, &sr, 1)) != ESP_OK)
    {
        return ret;
    }
    if (c1 != CTRL1_CONFIG || sr != SETRESET_VALUE)
    {
        ESP_LOGE(TAG, "Relectura inesperada: CTRL1=0x%02X (esperado 0x%02X), SET/RESET=0x%02X (esperado 0x%02X)",
                 c1, CTRL1_CONFIG, sr, SETRESET_VALUE);
        return ESP_ERR_INVALID_RESPONSE;
    }

    s_ready = true;
    ESP_LOGI(TAG, "Inicializado (0x%02X): continuo, +-2 G, 100 Hz, OSR 512", QMC5883L_I2C_ADDR);
    return ESP_OK;
}

esp_err_t qmc5883l_read_raw(qmc5883l_raw_t *out, uint8_t *status)
{
    if (!s_ready || out == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t st = 0;
    esp_err_t ret = reg_read(REG_STATUS, &st, 1);
    if (ret != ESP_OK)
    {
        return ret;
    }
    if (status != NULL)
    {
        *status = st;
    }
    if (!(st & QMC5883L_STATUS_DRDY))
    {
        return ESP_ERR_NOT_FINISHED;
    }

    // Los 6 bytes de una vez: el chip bloquea los datos hasta leer 0x05
    uint8_t b[6];
    ret = reg_read(REG_DATA_X_LSB, b, sizeof(b));
    if (ret != ESP_OK)
    {
        return ret;
    }

    out->x = (int16_t)(((uint16_t)b[1] << 8) | b[0]);
    out->y = (int16_t)(((uint16_t)b[3] << 8) | b[2]);
    out->z = (int16_t)(((uint16_t)b[5] << 8) | b[4]);
    return ESP_OK;
}

void qmc5883l_debug_print(uint32_t duration_ms)
{
    printf("MAG >> Lectura de prueba %lu ms. Gira la unidad y mira como cambian los ejes.\n",
           (unsigned long)duration_ms);

    const int64_t t_end = esp_timer_get_time() + (int64_t)duration_ms * 1000;
    while (esp_timer_get_time() < t_end)
    {
        qmc5883l_raw_t m;
        uint8_t st = 0;
        esp_err_t ret = qmc5883l_read_raw(&m, &st);
        if (ret == ESP_OK)
        {
            const float gx = m.x / QMC5883L_LSB_PER_GAUSS;
            const float gy = m.y / QMC5883L_LSB_PER_GAUSS;
            const float gz = m.z / QMC5883L_LSB_PER_GAUSS;
            const float mag = sqrtf(gx * gx + gy * gy + gz * gz);
            printf("MAG >> raw X=%6d Y=%6d Z=%6d | %+.3f %+.3f %+.3f G | |B|=%.3f G%s\n",
                   m.x, m.y, m.z, gx, gy, gz, mag,
                   (st & QMC5883L_STATUS_OVL) ? "  [OVL: saturado]" : "");
        }
        else if (ret != ESP_ERR_NOT_FINISHED)
        {
            printf("MAG >> Error de lectura: %s\n", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // ~10 impresiones/s
    }
    printf("MAG >> Fin de la lectura de prueba\n");
}