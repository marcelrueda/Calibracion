// main.c - Proyecto de calibración de sensores
// ETAPA 2: leer el acelerómetro (ICM-42670-P) y el inclinómetro (SCL3400)
// y mostrarlos por la consola serie y por la OLED.
#include <math.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "helper_i2c.h"
#include "icm42670p.h"
#include "oled_view.h"
#include "scl3400.h"

// Bus I2C0 de la OLED (el mismo del firmware principal)
#define I2C_SCL_GPIO   0
#define I2C_SDA_GPIO   1
#define I2C_FREQ_HZ    400000

#define SAMPLE_PERIOD_MS  10     // se leen los sensores cada 10 ms
#define PRINT_PERIOD_MS   100    // consola: 10 veces por segundo
#define OLED_PERIOD_MS    200    // OLED: 5 veces por segundo
#define RETRY_PERIOD_MS   2000   // reintento de arranque de un sensor que falló
#define WARN_PERIOD_MS    1000   // aviso repetido mientras un sensor no está disponible

static const char *TAG = "CALIB";

static HelperI2C s_bus; // bus I2C0 (solo la OLED)

// Escaneo 0x03..0x77: muestra qué dispositivos hay en el bus I2C
static void i2c_scan(i2c_master_bus_handle_t bus)
{
    printf("I2C >> Escaneando bus (SCL=GPIO%d, SDA=GPIO%d)...\n", I2C_SCL_GPIO, I2C_SDA_GPIO);
    int found = 0;
    for (uint8_t addr = 0x03; addr <= 0x77; addr++)
    {
        if (i2c_master_probe(bus, addr, 50) == ESP_OK)
        {
            printf("I2C >> Dispositivo detectado en 0x%02X\n", addr);
            found++;
        }
    }
    printf("I2C >> Escaneo terminado: %d dispositivo(s)\n", found);
}

// Lee y muestra el estado interno del SCL3400 (STATUS, ERR_FLAG1, ERR_FLAG2)
static void print_scl_diag(void)
{
    uint16_t st = 0, e1 = 0, e2 = 0;
    char txt[128];
    if (scl3400_read_diag(&st, &e1, &e2) == ESP_OK)
    {
        scl3400_describe_flags(st, e1, e2, txt, sizeof(txt));
        printf("INC >> DIAG: STATUS=0x%04X ERR_FLAG1=0x%04X ERR_FLAG2=0x%04X -> %s\n", st, e1, e2, txt);
    }
    else
    {
        printf("INC >> DIAG: no se pudo leer el estado\n");
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(helper_i2c_init(&s_bus, I2C_NUM_0, I2C_SCL_GPIO, I2C_SDA_GPIO, I2C_FREQ_HZ));
    i2c_scan(s_bus.bus_handle);

    // La OLED es opcional: si no responde, se sigue solo con la consola
    if (oled_view_init(&s_bus) != ESP_OK)
    {
        printf("OLED >> no disponible, se usa solo la consola\n");
    }

    // Cada sensor es independiente: si uno falla, el otro sigue funcionando
    esp_err_t e_icm_init = icm42670p_init();
    esp_err_t e_scl_init = scl3400_init();
    bool icm_ready = (e_icm_init == ESP_OK);
    bool scl_ready = (e_scl_init == ESP_OK);
    if (!icm_ready)
    {
        ESP_LOGE(TAG, "ICM-42670-P no disponible: %s", esp_err_to_name(e_icm_init));
    }
    if (!scl_ready)
    {
        ESP_LOGE(TAG, "SCL3400 no disponible: %s (se reintenta cada %d s)", esp_err_to_name(e_scl_init), RETRY_PERIOD_MS / 1000);
    }

    printf("SENS >> Leyendo. Deja la unidad quieta y luego inclinala despacio.\n");
    if (scl_ready)
    {
        print_scl_diag();
    }

    float a[3] = {0};
    scl3400_data_t s = {0};
    bool acc_ok = false;
    bool inc_ok = false;
    esp_err_t e_icm = ESP_OK;
    esp_err_t e_scl = ESP_OK;
    int64_t last_print_ms = 0;
    int64_t last_oled_ms = 0;
    int64_t last_retry_ms = 0;
    int64_t last_warn_ms = 0;

    while (true)
    {
        const int64_t now_ms = esp_timer_get_time() / 1000;

        // Reintentar el arranque de los sensores que fallaron
        if ((!icm_ready || !scl_ready) && now_ms - last_retry_ms >= RETRY_PERIOD_MS)
        {
            last_retry_ms = now_ms;
            if (!icm_ready)
            {
                e_icm_init = icm42670p_init();
                icm_ready = (e_icm_init == ESP_OK);
            }
            if (!scl_ready)
            {
                e_scl_init = scl3400_init();
                scl_ready = (e_scl_init == ESP_OK);
            }
        }

        if (icm_ready)
        {
            e_icm = icm42670p_read_accel_g(a);
            acc_ok = (e_icm == ESP_OK);
        }
        if (scl_ready)
        {
            e_scl = scl3400_read(&s);
            inc_ok = (e_scl == ESP_OK);
        }

        // Texto de error para la OLED cuando un bloque no tiene datos
        char acc_msg[24];
        char inc_msg[24];
        snprintf(acc_msg, sizeof(acc_msg), "E%03X %s", (unsigned)((icm_ready ? e_icm : e_icm_init) & 0xFFF),
                 icm_ready ? "lectura" : "arranque");
        snprintf(inc_msg, sizeof(inc_msg), "E%03X R=%08lX", (unsigned)((scl_ready ? e_scl : e_scl_init) & 0xFFF),
                 (unsigned long)scl3400_last_frame());

        if (now_ms - last_print_ms >= PRINT_PERIOD_MS)
        {
            last_print_ms = now_ms;
            if (icm_ready && acc_ok)
            {
                printf("ACC >> X=%+.4f Y=%+.4f Z=%+.4f g | |a|=%.4f g\n",
                       a[0], a[1], a[2], sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]));
            }
            if (scl_ready && inc_ok)
            {
                printf("INC >> X=%+.3f Y=%+.3f grados | ax=%+.5f ay=%+.5f g | T=%.1f C%s\n",
                       s.ang_x_deg, s.ang_y_deg, s.acc_x_g, s.acc_y_g, s.temp_c,
                       (s.status != 0) ? " [CON BANDERAS]" : "");
            }
        }

        // Avisos repetidos (cada 1 s) para poder verlos aunque el monitor se abra tarde
        if (now_ms - last_warn_ms >= WARN_PERIOD_MS)
        {
            last_warn_ms = now_ms;
            if (!icm_ready)
            {
                printf("ACC >> NO DISPONIBLE (arranque: %s)\n", esp_err_to_name(e_icm_init));
            }
            else if (!acc_ok)
            {
                printf("ACC >> error de lectura: %s\n", esp_err_to_name(e_icm));
            }
            if (scl_ready && inc_ok && s.status != 0)
            {
                printf("INC >> ATENCION: el sensor reporta banderas (STATUS=0x%04X). Los datos se muestran, pero NO son confiables para calibrar\n", s.status);
                print_scl_diag();
            }
            if (!scl_ready)
            {
                printf("INC >> NO DISPONIBLE (arranque: %s, ultima trama 0x%08lX)\n",
                       esp_err_to_name(e_scl_init), (unsigned long)scl3400_last_frame());
            }
            else if (!inc_ok)
            {
                printf("INC >> error de lectura: %s (ultima trama 0x%08lX)\n",
                       esp_err_to_name(e_scl), (unsigned long)scl3400_last_frame());
            }
        }

        if (now_ms - last_oled_ms >= OLED_PERIOD_MS)
        {
            last_oled_ms = now_ms;
            oled_view_show_imu(acc_ok, a[0], a[1], a[2], acc_msg, inc_ok, s.ang_x_deg, s.ang_y_deg, inc_msg, inc_ok && s.status != 0);
        }

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}
