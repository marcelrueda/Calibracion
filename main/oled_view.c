// oled_view.c
#include "oled_view.h"

#include <stdio.h>
#include "esp_log.h"
#include "ssd1306.h"

static const char *TAG = "OLED_VIEW";

static HelperI2CDevice s_oled;
static bool s_ready = false;

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

void oled_view_show_imu(bool acc_ok, float ax, float ay, float az, const char *acc_msg,
                        bool inc_ok, float inc_x_deg, float inc_y_deg, const char *inc_msg, bool inc_warn)
{
    if (!s_ready)
    {
        return;
    }

    // Tamaño 1 = 6 px de ancho por carácter (21 por línea) y 7 px de alto.
    char line[24];

    SSD1306_ClearDisplay();

    SSD1306_Drawtext(0, 0, "ACEL ICM-42670 [g]", 1);
    if (acc_ok)
    {
        snprintf(line, sizeof(line), "X:%+.3f Y:%+.3f", ax, ay);
        SSD1306_Drawtext(0, 11, line, 1);
        snprintf(line, sizeof(line), "Z:%+.3f", az);
        SSD1306_Drawtext(0, 22, line, 1);
    }
    else
    {
        snprintf(line, sizeof(line), "%s", (acc_msg != NULL) ? acc_msg : "sin datos");
        SSD1306_Drawtext(0, 11, line, 1);
    }

    SSD1306_Drawtext(0, 36, inc_warn ? "INCL SCL3400 [grad]!" : "INCL SCL3400 [grad]", 1);
    if (inc_ok)
    {
        snprintf(line, sizeof(line), "X:%+.3f Y:%+.3f", inc_x_deg, inc_y_deg);
        SSD1306_Drawtext(0, 47, line, 1);
    }
    else
    {
        snprintf(line, sizeof(line), "%s", (inc_msg != NULL) ? inc_msg : "sin datos");
        SSD1306_Drawtext(0, 47, line, 1);
    }

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
