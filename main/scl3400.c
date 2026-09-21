// scl3400.c
#include "scl3400.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "imu_spi.h"

static const char *TAG = "SCL3400";

// Tramas de 32 bits (datasheet, tabla 14). Cada una lleva su CRC en el último byte.
#define CMD_RD_ACC_X    0x040000F7u
#define CMD_RD_ACC_Y    0x080000FDu
#define CMD_RD_TEMP     0x140000EFu   // el código viejo tenía 0x140000F9 (CRC incorrecto)
#define CMD_RD_STATUS   0x180000E5u
#define CMD_RD_ERR_FLAG1 0x1C0000E3u
#define CMD_RD_ERR_FLAG2 0x200000C1u
#define CMD_RD_WHOAMI   0x40000091u   // el código viejo tenía 0x40000000 (CRC incorrecto)
#define CMD_MODE_A      0xB400001Fu
#define CMD_SW_RESET    0xB4002098u

#define WHOAMI_EXPECTED 0xE0
#define RS_NORMAL       0x01          // '01' = operación normal, sin banderas

// Tiempo mínimo entre tramas con CSB en alto: 10 us (datasheet 5.1.2). Se usa margen.
#define FRAME_GAP_US    100

static spi_device_handle_t s_dev = NULL;
static bool s_ready = false;
static uint32_t s_last_frame = 0;

// --- CRC-8 (datasheet 5.2, figura 12): poly 0x1D, init 0xFF, salida invertida ---
static uint8_t crc8_step(uint8_t bit, uint8_t crc)
{
    uint8_t temp = (uint8_t)(crc & 0x80);
    if (bit)
    {
        temp ^= 0x80;
    }
    crc = (uint8_t)(crc << 1);
    if (temp)
    {
        crc ^= 0x1D;
    }
    return crc;
}

static uint8_t frame_crc(uint32_t frame)
{
    uint8_t crc = 0xFF;
    for (int i = 31; i > 7; i--)
    {
        crc = crc8_step((uint8_t)((frame >> i) & 0x01), crc);
    }
    return (uint8_t)~crc;
}

// Protocolo "off-frame": la respuesta que llega es la del comando ANTERIOR.
static esp_err_t xfer(uint32_t cmd, uint32_t *resp)
{
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 32;
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.tx_data[0] = (uint8_t)(cmd >> 24);
    t.tx_data[1] = (uint8_t)(cmd >> 16);
    t.tx_data[2] = (uint8_t)(cmd >> 8);
    t.tx_data[3] = (uint8_t)cmd;

    esp_err_t ret = spi_device_transmit(s_dev, &t);
    if (ret == ESP_OK && resp != NULL)
    {
        *resp = ((uint32_t)t.rx_data[0] << 24) | ((uint32_t)t.rx_data[1] << 16) |
                ((uint32_t)t.rx_data[2] << 8) | (uint32_t)t.rx_data[3];
        s_last_frame = *resp;
    }
    esp_rom_delay_us(FRAME_GAP_US);
    return ret;
}

uint32_t scl3400_last_frame(void)
{
    return s_last_frame;
}

// Valida el CRC de una respuesta y separa RS (bits 25:24) y datos (bits 23:8).
static esp_err_t parse(uint32_t r, uint8_t *rs, uint16_t *data)
{
    if (frame_crc(r) != (uint8_t)(r & 0xFF))
    {
        return ESP_ERR_INVALID_CRC;
    }
    *rs = (uint8_t)((r >> 24) & 0x03);
    *data = (uint16_t)((r >> 8) & 0xFFFF);
    return ESP_OK;
}

esp_err_t scl3400_init(void)
{
    if (s_ready)
    {
        return ESP_OK;
    }

    esp_err_t ret = ESP_OK;
    if (s_dev == NULL)
    {
        ret = imu_spi_add_device(SCL_CS_GPIO, SCL_SPI_HZ, &s_dev);
        if (ret != ESP_OK)
        {
            s_dev = NULL;
            return ret;
        }
    }

    uint32_t r = 0;
    uint8_t rs = 0;
    uint16_t data = 0;
    vTaskDelay(pdMS_TO_TICKS(10));

    xfer(CMD_SW_RESET, &r);            // paso 2: reset por software
    vTaskDelay(pdMS_TO_TICKS(10));     // paso 3: esperar >= 3 ms
    xfer(CMD_MODE_A, &r);              // paso 4: modo A
    vTaskDelay(pdMS_TO_TICKS(150));    // paso 5: esperar >= 100 ms

    // Pasos 6 a 8: leer STATUS para limpiar el resumen de estado. El datasheet usa
    // 3 lecturas; aquí se permiten hasta 8 antes de rendirse.
    bool rs_ok = false;
    for (int i = 0; i < 8; i++)
    {
        xfer(CMD_RD_STATUS, &r);
        if (i >= 2 && parse(r, &rs, &data) == ESP_OK && rs == RS_NORMAL)
        {
            rs_ok = true;
            break;
        }
    }

    // WHOAMI: la primera lectura devuelve la respuesta anterior, la segunda trae el dato.
    // Es la prueba decisiva de que el sensor responde (CRC valido y valor 0xE0).
    xfer(CMD_RD_WHOAMI, &r);
    xfer(CMD_RD_WHOAMI, &r);
    ret = parse(r, &rs, &data);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Sin respuesta valida (CRC). Ultima trama 0x%08lX. Revisa CS=GPIO%d, MISO y la alimentacion",
                 (unsigned long)r, SCL_CS_GPIO);
        return ret;
    }
    if ((data & 0xFF) != WHOAMI_EXPECTED)
    {
        ESP_LOGE(TAG, "WHOAMI=0x%02X (esperado 0x%02X)", data & 0xFF, WHOAMI_EXPECTED);
        return ESP_ERR_NOT_FOUND;
    }
    if (!rs_ok)
    {
        ESP_LOGW(TAG, "El estado no quedo en RS=1 tras el arranque (RS=%u); se continua porque WHOAMI es correcto", rs);
    }

    s_ready = true;
    ESP_LOGI(TAG, "Listo (WHOAMI=0x%02X): modo A, +-30 grados, %.0f LSB/g, filtro 10 Hz", data & 0xFF, SCL_LSB_PER_G);
    return ESP_OK;
}

esp_err_t scl3400_read_diag(uint16_t *status, uint16_t *err1, uint16_t *err2)
{
    if (!s_ready || status == NULL || err1 == NULL || err2 == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Cada respuesta es la del comando anterior: r[1]=STATUS, r[2]=ERR_FLAG1, r[3]=ERR_FLAG2.
    static const uint32_t cmds[4] = {CMD_RD_STATUS, CMD_RD_ERR_FLAG1, CMD_RD_ERR_FLAG2, CMD_RD_STATUS};
    uint32_t r[4] = {0};
    for (int i = 0; i < 4; i++)
    {
        esp_err_t ret = xfer(cmds[i], &r[i]);
        if (ret != ESP_OK)
        {
            return ret;
        }
    }

    uint8_t rs = 0;
    uint16_t d[3] = {0};
    for (int i = 0; i < 3; i++)
    {
        esp_err_t ret = parse(r[i + 1], &rs, &d[i]);
        if (ret != ESP_OK)
        {
            return ret;
        }
    }
    *status = d[0];
    *err1 = d[1];
    *err2 = d[2];
    return ESP_OK;
}

static void add_name(char *buf, size_t n, size_t *len, const char *prefix, const char *name)
{
    if (*len + 1 >= n)
    {
        return;
    }
    const int w = snprintf(buf + *len, n - *len, "%s%s%s", (*len > 0) ? " " : "", prefix, name);
    if (w > 0)
    {
        *len += (size_t)w;
        if (*len >= n)
        {
            *len = n - 1;
        }
    }
}

void scl3400_describe_flags(uint16_t status, uint16_t err1, uint16_t err2, char *buf, size_t n)
{
    if (buf == NULL || n == 0)
    {
        return;
    }
    buf[0] = '\0';
    size_t len = 0;

    // STATUS (tabla 23), bit 0 a 9
    static const char *const st_names[10] = {"PIN_CONTINUITY", "MODE_CHANGE", "PD", "MEM", "PWR",
                                             "TEMP", "SAT", "CLK", "DIGI2", "DIGI1"};
    for (int i = 0; i < 10; i++)
    {
        if (status & (1u << i))
        {
            add_name(buf, n, &len, "EST:", st_names[i]);
        }
    }

    // ERR_FLAG1 (tabla 28): bit 0 MEM, bits 10:1 AFE_SAT, bit 11 ADC_SAT
    if (err1 & 0x0001) add_name(buf, n, &len, "E1:", "MEM");
    if (err1 & 0x07FE) add_name(buf, n, &len, "E1:", "AFE_SAT");
    if (err1 & 0x0800) add_name(buf, n, &len, "E1:", "ADC_SAT");

    // ERR_FLAG2 (tabla 30)
    static const struct { uint8_t bit; const char *name; } e2_names[] = {
        {0, "CLK"}, {1, "TEMP"}, {2, "APWR_2"}, {3, "VREF"}, {4, "DPWR"}, {5, "APWR"},
        {7, "MEMORY_CRC"}, {8, "PD"}, {9, "MODE_CHANGE"}, {11, "VDD"}, {12, "AGND"},
        {13, "A_EXT_C"}, {14, "D_EXT_C"},
    };
    for (size_t i = 0; i < sizeof(e2_names) / sizeof(e2_names[0]); i++)
    {
        if (err2 & (1u << e2_names[i].bit))
        {
            add_name(buf, n, &len, "E2:", e2_names[i].name);
        }
    }

    if (len == 0)
    {
        snprintf(buf, n, "sin banderas");
    }
}

esp_err_t scl3400_read(scl3400_data_t *out)
{
    if (!s_ready || out == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // Cada respuesta es la del comando anterior: r[1]=ACC_X, r[2]=ACC_Y, r[3]=TEMP.
    static const uint32_t cmds[4] = {CMD_RD_ACC_X, CMD_RD_ACC_Y, CMD_RD_TEMP, CMD_RD_ACC_X};
    uint32_t r[4] = {0};
    for (int i = 0; i < 4; i++)
    {
        esp_err_t ret = xfer(cmds[i], &r[i]);
        if (ret != ESP_OK)
        {
            return ret;
        }
    }

    uint8_t rs[3];
    uint16_t d[3];
    for (int i = 0; i < 3; i++)
    {
        esp_err_t ret = parse(r[i + 1], &rs[i], &d[i]);
        if (ret != ESP_OK)
        {
            return ret;
        }
    }

    out->status = 0;
    if (rs[0] != RS_NORMAL || rs[1] != RS_NORMAL || rs[2] != RS_NORMAL)
    {
        // El sensor reporta banderas: se averigua cuáles leyendo STATUS.
        uint16_t st = 0, e1 = 0, e2 = 0;
        esp_err_t ret = scl3400_read_diag(&st, &e1, &e2);
        if (ret != ESP_OK)
        {
            return ret;
        }
        // Sin banderas (RS pasajero) o con cualquier bandera distinta de PIN_CONTINUITY: se descarta.
        if (st == 0 || (st & (uint16_t)~SCL_STATUS_PIN_CONTINUITY) != 0)
        {
            return ESP_ERR_INVALID_RESPONSE;
        }
        out->status = st;
    }

    out->raw_x = (int16_t)d[0];
    out->raw_y = (int16_t)d[1];
    out->acc_x_g = out->raw_x / SCL_LSB_PER_G;
    out->acc_y_g = out->raw_y / SCL_LSB_PER_G;

    // Inclinación = asin(g), válida mientras |g| <= 1 (datasheet, sección 4.3)
    const float gx = fmaxf(fminf(out->acc_x_g, 1.0f), -1.0f);
    const float gy = fmaxf(fminf(out->acc_y_g, 1.0f), -1.0f);
    out->ang_x_deg = asinf(gx) * (180.0f / (float)M_PI);
    out->ang_y_deg = asinf(gy) * (180.0f / (float)M_PI);

    out->temp_c = -273.0f + ((int16_t)d[2] / 18.9f);   // datasheet 2.4
    return ESP_OK;
}
