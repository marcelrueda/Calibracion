// mag_display.c
#include "mag_display.h"

#include <math.h>
#include <stdio.h>
#include "esp_log.h"
#include "ssd1306.h"

static const char *TAG = "MAG_OLED";

static HelperI2CDevice s_oled;
static bool s_ready = false;

esp_err_t mag_display_init(HelperI2C *bus)
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

void mag_display_show(float gx, float gy, float gz, bool saturated)
{
    if (!s_ready)
    {
        return;
    }

    // Tamaño 1 = 6 px de ancho por carácter (21 por línea) y 7 px de alto.
    char line[24];
    const float mag = sqrtf(gx * gx + gy * gy + gz * gz);

    SSD1306_ClearDisplay();
    SSD1306_Drawtext(0, 0, "QMC5883L  campo [G]", 1);

    snprintf(line, sizeof(line), "X: %+.3f", gx);
    SSD1306_Drawtext(0, 14, line, 1);
    snprintf(line, sizeof(line), "Y: %+.3f", gy);
    SSD1306_Drawtext(0, 26, line, 1);
    snprintf(line, sizeof(line), "Z: %+.3f", gz);
    SSD1306_Drawtext(0, 38, line, 1);

    snprintf(line, sizeof(line), "|B|=%.3f G%s", mag, saturated ? " OVL" : "");
    SSD1306_Drawtext(0, 52, line, 1);

    SSD1306_Display(&s_oled);
}

void mag_display_message(const char *line1, const char *line2)
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