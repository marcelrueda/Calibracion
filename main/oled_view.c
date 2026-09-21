// oled_view.c
#include "oled_view.h"

#include <math.h>
#include <stdio.h>
#include "esp_log.h"
#include "ssd1306.h"

static const char *TAG = "OLED_VIEW";

static HelperI2CDevice s_oled;
static bool s_ready = false;

// Distribución: 8 filas de 8 px (título y valores de cada sensor, una fila cada uno).
// Con letra de tamaño 1 caben 21 caracteres por fila.
#define ROW(n)  ((n) * 8)

esp_err_t oled_view_init(HelperI2C *bus)
{
    if (s_ready)
    {
        return ESP_OK;
    }
    if (bus == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = i2c_master_probe(bus->bus_handle, SSD1306_I2C_ADDRESS, 100);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "OLED no responde en 0x%02X (%s)", SSD1306_I2C_ADDRESS, esp_err_to_name(ret));
        return ret;
    }

    ret = helper_i2c_add_device(bus, &s_oled, SSD1306_I2C_ADDRESS, I2C_MASTER_FREQ_HZ);
    if (ret != ESP_OK)
    {
        return ret;
    }

    ret = SSD1306_Begin(&s_oled, SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS);
    if (ret != ESP_OK)
    {
        return ret;
    }

    SSD1306_ClearDisplay();
    SSD1306_Display(&s_oled);
    s_ready = true;
    ESP_LOGI(TAG, "OLED inicializada (0x%02X)", SSD1306_I2C_ADDRESS);
    return ESP_OK;
}

// Giroscopio: 2 decimales, y 1 decimal si el valor es de 100 o más (para que quepan 3 valores).
static void fmt_gyro(char *dst, size_t n, float v)
{
    snprintf(dst, n, (fabsf(v) < 100.0f) ? "%+.2f" : "%+.1f", (double)v);
}

static void draw_block(int row, const char *title, bool ok, const char *values)
{
    char line[24];
    snprintf(line, sizeof(line), "%s", title);
    SSD1306_Drawtext(0, (uint8_t)ROW(row), line, 1);
    snprintf(line, sizeof(line), "%s", ok ? values : "sin datos");
    SSD1306_Drawtext(0, (uint8_t)ROW(row + 1), line, 1);
}

void oled_view_show_all(const oled_data_t *d)
{
    if (!s_ready || d == NULL)
    {
        return;
    }

    char v[32];
    char gx[10], gy[10], gz[10];

    SSD1306_ClearDisplay();

    snprintf(v, sizeof(v), "%+.3f %+.3f %+.3f", (double)d->acc_g[0], (double)d->acc_g[1], (double)d->acc_g[2]);
    draw_block(0, "ACEL g", d->acc_ok, v);

    fmt_gyro(gx, sizeof(gx), d->gyr_dps[0]);
    fmt_gyro(gy, sizeof(gy), d->gyr_dps[1]);
    fmt_gyro(gz, sizeof(gz), d->gyr_dps[2]);
    snprintf(v, sizeof(v), "%s %s %s", gx, gy, gz);
    draw_block(2, "GIRO dps", d->gyr_ok, v);

    snprintf(v, sizeof(v), "%+.3f %+.3f", (double)d->inc_deg[0], (double)d->inc_deg[1]);
    draw_block(4, d->inc_warn ? "INCL grad !" : "INCL grad", d->inc_ok, v);

    snprintf(v, sizeof(v), "%+.3f %+.3f %+.3f", (double)d->mag_g[0], (double)d->mag_g[1], (double)d->mag_g[2]);
    draw_block(6, "MAG gauss", d->mag_ok, v);

    SSD1306_Display(&s_oled);
}

void oled_view_message(const char *line1, const char *line2)
{
    if (!s_ready)
    {
        return;
    }

    char buf[22];
    SSD1306_ClearDisplay();
    if (line1 != NULL)
    {
        snprintf(buf, sizeof(buf), "%s", line1);
        SSD1306_Drawtext(0, 8, buf, 1);
    }
    if (line2 != NULL)
    {
        snprintf(buf, sizeof(buf), "%s", line2);
        SSD1306_Drawtext(0, 24, buf, 1);
    }
    SSD1306_Display(&s_oled);
}
