// icm42670p.c
#include "icm42670p.h"

#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "imu_spi.h"

static const char *TAG = "ICM42670P";

// Registros (datasheet DS-000451, banco 0)
#define REG_WHO_AM_I        0x75
#define REG_PWR_MGMT0       0x1F
#define REG_GYRO_CONFIG0    0x20
#define REG_ACCEL_CONFIG0   0x21
#define REG_GYRO_CONFIG1    0x23
#define REG_ACCEL_CONFIG1   0x24
#define REG_ACCEL_DATA_X1   0x0B   // ACCEL X1 X0 Y1 Y0 Z1 Z0, luego GYRO X1 X0 Y1 Y0 Z1 Z0 (0x11..0x16)

#define WHOAMI_EXPECTED     0x67

// PWR_MGMT0: GYRO_MODE[3:2]=11 y ACCEL_MODE[1:0]=11 (ambos en Low Noise)
#define PWR_ACCEL_GYRO_LN   0x0F
// ACCEL_CONFIG0: ACCEL_UI_FS_SEL[6:5]=11 (±2 g) | ACCEL_ODR[3:0]=1001 (100 Hz)
#define ACCEL_CFG0_2G_100HZ 0x69
// GYRO_CONFIG0: GYRO_UI_FS_SEL[6:5]=11 (±250 °/s) | GYRO_ODR[3:0]=1001 (100 Hz)
#define GYRO_CFG0_250_100HZ 0x69
// ACCEL_CONFIG1 (reset 0x41) y GYRO_CONFIG1 (reset 0x31) con UI_FILT_BW[2:0]=111 (16 Hz)
#define ACCEL_CFG1_BW16HZ   0x47
#define GYRO_CFG1_BW16HZ    0x37

static spi_device_handle_t s_dev = NULL;
static bool s_ready = false;

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 16;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = (uint8_t)(reg & 0x7F); // bit7=0 -> escritura
    t.tx_data[1] = val;
    return spi_device_transmit(s_dev, &t);
}

static esp_err_t read_regs(uint8_t reg, uint8_t *dst, size_t n)
{
    if (n == 0 || n > 15)
    {
        return ESP_ERR_INVALID_ARG;
    }
    uint8_t tx[16] = {0};
    uint8_t rx[16] = {0};
    tx[0] = (uint8_t)(reg | 0x80); // bit7=1 -> lectura

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = (n + 1) * 8;
    t.tx_buffer = tx;
    t.rx_buffer = rx;
    esp_err_t ret = spi_device_transmit(s_dev, &t);
    if (ret == ESP_OK)
    {
        memcpy(dst, &rx[1], n);
    }
    return ret;
}

esp_err_t icm42670p_init(void)
{
    if (s_ready)
    {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    if (s_dev == NULL)
    {
        ret = imu_spi_add_device(ICM_CS_GPIO, ICM_SPI_HZ, &s_dev);
        if (ret != ESP_OK)
        {
            s_dev = NULL;
            return ret;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t who = 0;
    ret = read_regs(REG_WHO_AM_I, &who, 1);
    if (ret != ESP_OK)
    {
        return ret;
    }
    if (who != WHOAMI_EXPECTED)
    {
        ESP_LOGE(TAG, "WHO_AM_I=0x%02X (esperado 0x%02X). Revisa CS=GPIO%d y el cableado SPI", who, WHOAMI_EXPECTED, ICM_CS_GPIO);
        return ESP_ERR_NOT_FOUND;
    }

    // Encender acelerómetro y giroscopio y esperar antes de escribir más registros
    // (datasheet: sin escrituras durante 200 us tras encender un sensor).
    if ((ret = write_reg(REG_PWR_MGMT0, PWR_ACCEL_GYRO_LN)) != ESP_OK)
    {
        return ret;
    }
    vTaskDelay(pdMS_TO_TICKS(2));

    if ((ret = write_reg(REG_ACCEL_CONFIG0, ACCEL_CFG0_2G_100HZ)) != ESP_OK ||
        (ret = write_reg(REG_ACCEL_CONFIG1, ACCEL_CFG1_BW16HZ)) != ESP_OK ||
        (ret = write_reg(REG_GYRO_CONFIG0, GYRO_CFG0_250_100HZ)) != ESP_OK ||
        (ret = write_reg(REG_GYRO_CONFIG1, GYRO_CFG1_BW16HZ)) != ESP_OK)
    {
        return ret;
    }
    // Arranque del giroscopio: 30 ms típico y debe permanecer encendido al menos 45 ms.
    vTaskDelay(pdMS_TO_TICKS(60));

    // Verificación por relectura (ignora los bits reservados)
    uint8_t pwr = 0, ac0 = 0, ac1 = 0, gy0 = 0, gy1 = 0;
    if ((ret = read_regs(REG_PWR_MGMT0, &pwr, 1)) != ESP_OK ||
        (ret = read_regs(REG_ACCEL_CONFIG0, &ac0, 1)) != ESP_OK ||
        (ret = read_regs(REG_ACCEL_CONFIG1, &ac1, 1)) != ESP_OK ||
        (ret = read_regs(REG_GYRO_CONFIG0, &gy0, 1)) != ESP_OK ||
        (ret = read_regs(REG_GYRO_CONFIG1, &gy1, 1)) != ESP_OK)
    {
        return ret;
    }
    if ((pwr & 0x0F) != PWR_ACCEL_GYRO_LN ||
        (ac0 & 0x6F) != ACCEL_CFG0_2G_100HZ ||
        (ac1 & 0x77) != ACCEL_CFG1_BW16HZ ||
        (gy0 & 0x6F) != GYRO_CFG0_250_100HZ ||
        (gy1 & 0x07) != (GYRO_CFG1_BW16HZ & 0x07))
    {
        ESP_LOGE(TAG, "Configuracion no aplicada: PWR=0x%02X ACC0=0x%02X ACC1=0x%02X GYR0=0x%02X GYR1=0x%02X",
                 pwr, ac0, ac1, gy0, gy1);
        return ESP_ERR_INVALID_RESPONSE;
    }

    s_ready = true;
    ESP_LOGI(TAG, "Listo (WHO_AM_I=0x%02X): acelerometro +-2 g y giroscopio +-250 dps, 100 Hz, filtro 16 Hz", who);
    return ESP_OK;
}

esp_err_t icm42670p_read_sample(icm_sample_t *out)
{
    if (!s_ready || out == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t b[12];
    esp_err_t ret = read_regs(REG_ACCEL_DATA_X1, b, sizeof(b));
    if (ret != ESP_OK)
    {
        return ret;
    }
    for (int i = 0; i < 3; i++)
    {
        const int16_t acc = (int16_t)((b[2 * i] << 8) | b[2 * i + 1]);
        const int16_t gyr = (int16_t)((b[6 + 2 * i] << 8) | b[6 + 2 * i + 1]);
        out->acc_g[i] = acc / ICM_ACCEL_LSB_PER_G;
        out->gyro_dps[i] = gyr / ICM_GYRO_LSB_PER_DPS;
    }
    return ESP_OK;
}

esp_err_t icm42670p_read_accel_raw(icm_raw3_t *out)
{
    if (!s_ready || out == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t b[6];
    esp_err_t ret = read_regs(REG_ACCEL_DATA_X1, b, sizeof(b));
    if (ret != ESP_OK)
    {
        return ret;
    }
    out->x = (int16_t)((b[0] << 8) | b[1]);
    out->y = (int16_t)((b[2] << 8) | b[3]);
    out->z = (int16_t)((b[4] << 8) | b[5]);
    return ESP_OK;
}

esp_err_t icm42670p_read_accel_g(float g[3])
{
    icm_raw3_t r;
    esp_err_t ret = icm42670p_read_accel_raw(&r);
    if (ret != ESP_OK)
    {
        return ret;
    }
    g[0] = r.x / ICM_ACCEL_LSB_PER_G;
    g[1] = r.y / ICM_ACCEL_LSB_PER_G;
    g[2] = r.z / ICM_ACCEL_LSB_PER_G;
    return ESP_OK;
}
