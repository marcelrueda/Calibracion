// calib.c
#include "calib.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "calib_store.h"
#include "console_in.h"
#include "oled_view.h"

static const char *TAG = "CALIB";

#define KEY_ICM       "icm"
#define ICM_VERSION   1

static calib_icm_t s_icm;
static esp_err_t s_icm_load = ESP_ERR_NOT_FOUND;   // resultado de cargar el ICM al arrancar

static void icm_identity(calib_icm_t *c)
{
    memset(c, 0, sizeof(*c));
    for (int i = 0; i < 3; i++)
    {
        c->acc_scale[i] = 1.0f;
    }
}

// ---------------------------------------------------------------- estadística

void stats3_init(stats3_t *s)
{
    memset(s, 0, sizeof(*s));
}

void stats3_add(stats3_t *s, const float v[3])
{
    s->n++;
    for (int i = 0; i < 3; i++)
    {
        const double d = (double)v[i] - s->mean[i];
        s->mean[i] += d / (double)s->n;
        s->m2[i] += d * ((double)v[i] - s->mean[i]);
    }
}

void stats3_result(const stats3_t *s, float mean[3], float std[3])
{
    for (int i = 0; i < 3; i++)
    {
        mean[i] = (float)s->mean[i];
        std[i] = (s->n > 1) ? (float)sqrt(s->m2[i] / (double)(s->n - 1)) : 0.0f;
    }
}

esp_err_t calib_gyro_evaluate(const stats3_t *s, float bias_out[3], float std_out[3])
{
    if (s->n < CALIB_GYRO_MIN_SAMPLES)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    float mean[3], std[3];
    stats3_result(s, mean, std);
    for (int i = 0; i < 3; i++)
    {
        std_out[i] = std[i];
        bias_out[i] = mean[i];
    }

    float worst = 0.0f;
    for (int i = 0; i < 3; i++)
    {
        if (std[i] > CALIB_GYRO_MAX_STD_DPS)
        {
            return CALIB_ERR_MOTION;
        }
        if (fabsf(mean[i]) > worst)
        {
            worst = fabsf(mean[i]);
        }
    }
    if (worst > CALIB_GYRO_HARD_MAX_DPS)
    {
        return CALIB_ERR_ABSURD;
    }
    if (worst > CALIB_GYRO_MAX_ABS_DPS)
    {
        return CALIB_ERR_RANGE;
    }
    return ESP_OK;
}

// ---------------------------------------------------------------- carga y aplicación

esp_err_t calib_init(void)
{
    icm_identity(&s_icm);
    esp_err_t ret = calib_store_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "NVS no disponible: %s. No se podra guardar ni cargar calibraciones", esp_err_to_name(ret));
        s_icm_load = ret;
        return ret;
    }

    calib_icm_t tmp;
    s_icm_load = calib_store_load(KEY_ICM, ICM_VERSION, &tmp, sizeof(tmp));
    if (s_icm_load == ESP_OK)
    {
        s_icm = tmp;
    }
    return ESP_OK;
}

void calib_print_status(void)
{
    if (s_icm_load == ESP_OK)
    {
        printf("CAL >> ICM: giroscopio %s", s_icm.gyro_valid ? "calibrado" : "sin calibrar");
        if (s_icm.gyro_valid)
        {
            printf(" (sesgo X=%+.3f Y=%+.3f Z=%+.3f dps)", s_icm.gyro_bias_dps[0], s_icm.gyro_bias_dps[1], s_icm.gyro_bias_dps[2]);
        }
        if (s_icm.gyro_valid && s_icm.gyro_out_of_spec)
        {
            printf(" [FUERA DE ESPECIFICACION: sesgo > %.0f dps]", CALIB_GYRO_SPEC_DPS);
        }
        printf(" | acelerometro %s\n", s_icm.acc_valid ? "calibrado" : "sin calibrar");
    }
    else if (s_icm_load == ESP_ERR_NOT_FOUND)
    {
        printf("CAL >> ICM: sin calibracion guardada (se muestra sin corregir)\n");
    }
    else
    {
        printf("CAL >> ICM: la calibracion guardada NO se pudo usar (%s); se muestra sin corregir\n", esp_err_to_name(s_icm_load));
    }
}

const calib_icm_t *calib_icm(void)
{
    return &s_icm;
}

void calib_icm_apply(icm_sample_t *s)
{
    for (int i = 0; i < 3; i++)
    {
        if (s_icm.acc_valid)
        {
            s->acc_g[i] = (s->acc_g[i] - s_icm.acc_offset_g[i]) / s_icm.acc_scale[i];
        }
        if (s_icm.gyro_valid)
        {
            s->gyro_dps[i] -= s_icm.gyro_bias_dps[i];
        }
    }
}

// ---------------------------------------------------------------- calibración del giroscopio

// Espera n_ms atendiendo la tecla q. Devuelve true si se pidió cancelar.
static bool wait_or_cancel(uint32_t n_ms)
{
    const int c = console_in_getc(n_ms);
    return (c == 'q' || c == 'Q');
}

esp_err_t calib_run_gyro(void)
{
    char l1[24], l2[24];

    printf("CAL >> GIROSCOPIO: apoya la unidad y NO la toques ni la muevas. Cancelar: tecla q\n");
    console_in_flush();

    for (int s = CALIB_GYRO_SETTLE_S; s > 0; s--)
    {
        snprintf(l2, sizeof(l2), "inicia en %d s", s);
        oled_view_message("GIRO: no mover", l2);
        printf("CAL >> empieza en %d s...\n", s);
        if (wait_or_cancel(1000))
        {
            printf("CAL >> cancelado\n");
            return ESP_ERR_INVALID_STATE;
        }
    }

    stats3_t st;
    stats3_init(&st);
    int errors = 0;
    const int64_t t_end = esp_timer_get_time() + (int64_t)CALIB_GYRO_CAPTURE_S * 1000000;
    int64_t last_msg = 0;

    while (esp_timer_get_time() < t_end)
    {
        icm_sample_t smp;
        if (icm42670p_read_sample(&smp) == ESP_OK)
        {
            stats3_add(&st, smp.gyro_dps);
        }
        else if (++errors > 50)
        {
            printf("CAL >> demasiados errores de lectura del ICM: se cancela\n");
            oled_view_message("GIRO: error", "de lectura");
            return ESP_FAIL;
        }

        const int64_t now = esp_timer_get_time();
        if (now - last_msg >= 1000000)
        {
            last_msg = now;
            snprintf(l2, sizeof(l2), "quedan %d s", (int)((t_end - now) / 1000000) + 1);
            oled_view_message("GIRO: no mover", l2);
        }
        if (wait_or_cancel(10))
        {
            printf("CAL >> cancelado\n");
            return ESP_ERR_INVALID_STATE;
        }
    }

    float bias[3] = {0}, std[3] = {0};
    esp_err_t ev = calib_gyro_evaluate(&st, bias, std);
    printf("CAL >> %u muestras. Dispersion (desv. tipica): X=%.3f Y=%.3f Z=%.3f dps\n",
           (unsigned)st.n, std[0], std[1], std[2]);

    bool out_of_spec = false;
    if (ev == CALIB_ERR_RANGE)
    {
        // La unidad estaba quieta (la dispersión pasó el criterio), pero el cero es grande.
        printf("CAL >> La unidad estaba QUIETA, pero el sesgo es grande: X=%+.3f Y=%+.3f Z=%+.3f dps\n", bias[0], bias[1], bias[2]);
        printf("CAL >> El datasheet del ICM-42670-P da +-%.0f dps a 25 C: este sensor esta FUERA DE ESPECIFICACION\n", CALIB_GYRO_SPEC_DPS);
        printf("CAL >> Presiona S para guardarlo de todos modos (queda marcado) o cualquier otra tecla para descartar (%d s)\n", CALIB_CONFIRM_S);
        oled_view_message("Sesgo fuera de esp.", "S=guardar otra=no");
        const int c = console_in_getc((uint32_t)CALIB_CONFIRM_S * 1000);
        if (c == 's' || c == 'S')
        {
            out_of_spec = true;
            ev = ESP_OK;
        }
        else
        {
            printf("CAL >> descartado: NO se guardo\n");
            oled_view_message("GIRO: descartado", "sesgo grande");
            return CALIB_ERR_RANGE;
        }
    }

    if (ev != ESP_OK)
    {
        if (ev == CALIB_ERR_MOTION)
        {
            printf("CAL >> NO SE GUARDO: la unidad se movio (dispersion mayor que %.2f dps). Repite con la unidad quieta\n", CALIB_GYRO_MAX_STD_DPS);
            snprintf(l1, sizeof(l1), "se movio");
        }
        else if (ev == CALIB_ERR_ABSURD)
        {
            printf("CAL >> NO SE GUARDO: sesgo de %.1f dps o mas, imposible para un cero de giroscopio. Revisa la configuracion o el sensor\n", CALIB_GYRO_HARD_MAX_DPS);
            snprintf(l1, sizeof(l1), "sesgo imposible");
        }
        else
        {
            printf("CAL >> NO SE GUARDO: muy pocas muestras (%s)\n", esp_err_to_name(ev));
            snprintf(l1, sizeof(l1), "pocas muestras");
        }
        oled_view_message("GIRO: NO GUARDADO", l1);
        return ev;
    }

    calib_icm_t next = s_icm;               // conserva lo que ya hubiera del acelerómetro
    next.gyro_valid = 1;
    next.gyro_out_of_spec = out_of_spec ? 1 : 0;
    for (int i = 0; i < 3; i++)
    {
        next.gyro_bias_dps[i] = bias[i];
    }

    const esp_err_t sv = calib_store_save(KEY_ICM, ICM_VERSION, &next, sizeof(next));
    s_icm = next;                            // se aplica en esta sesión aunque falle el guardado
    printf("CAL >> sesgo del giroscopio: X=%+.3f Y=%+.3f Z=%+.3f dps\n", bias[0], bias[1], bias[2]);
    if (sv == ESP_OK)
    {
        s_icm_load = ESP_OK;
        printf("CAL >> GUARDADO en NVS%s. Se cargara solo en cada arranque\n", out_of_spec ? " (marcado como fuera de especificacion)" : "");
        oled_view_message("GIRO calibrado", "guardado en NVS");
    }
    else
    {
        printf("CAL >> ATENCION: se aplica ahora, pero NO se pudo guardar (%s)\n", esp_err_to_name(sv));
        oled_view_message("GIRO: aplicado", "NO se guardo");
    }
    return sv;
}

// ---------------------------------------------------------------- menú de arranque

static void boot_menu_run(bool icm_ready)
{
    char l2[24];
    console_in_flush();
    printf("CAL >> Presiona C en los proximos %d s para calibrar (si no, se continua)...\n", CALIB_BOOT_WINDOW_S);

    bool go = false;
    for (int s = CALIB_BOOT_WINDOW_S; s > 0 && !go; s--)
    {
        snprintf(l2, sizeof(l2), "%d s", s);
        oled_view_message("Calibrar? tecla C", l2);
        const int64_t t_end = esp_timer_get_time() + 1000000;
        while (esp_timer_get_time() < t_end)
        {
            const int c = console_in_getc(50);
            if (c == 'c' || c == 'C')
            {
                go = true;
                break;
            }
            if (c >= 0 && c != '\r' && c != '\n')
            {
                printf("CAL >> tecla 0x%02X ignorada (usa C)\n", (unsigned)c);
            }
        }
    }
    if (!go)
    {
        printf("CAL >> continuando sin calibrar\n");
        return;
    }

    while (true)
    {
        printf("CAL >> MENU (elige con una tecla): 1 = giroscopio (unidad quieta, ~%d s) | e = borrar calibracion del ICM | q = salir\n",
               CALIB_GYRO_SETTLE_S + CALIB_GYRO_CAPTURE_S);
        oled_view_message("1=giro e=borrar", "q=salir");

        int c = -1;
        for (int t = 0; t < CALIB_MENU_TIMEOUT_S * 10 && c < 0; t++)
        {
            c = console_in_getc(100);
            if (c == '\r' || c == '\n')
            {
                c = -1;
            }
        }
        if (c < 0)
        {
            printf("CAL >> sin respuesta en %d s: se continua\n", CALIB_MENU_TIMEOUT_S);
            return;
        }
        if (c == 'q' || c == 'Q')
        {
            printf("CAL >> saliendo del menu\n");
            return;
        }
        if (c == '1')
        {
            if (icm_ready)
            {
                calib_run_gyro();
            }
            else
            {
                printf("CAL >> el ICM no esta disponible\n");
            }
        }
        else if (c == 'e' || c == 'E')
        {
            const esp_err_t er = calib_store_erase(KEY_ICM);
            icm_identity(&s_icm);
            s_icm_load = ESP_ERR_NOT_FOUND;
            printf("CAL >> calibracion del ICM borrada (%s)\n", esp_err_to_name(er));
        }
        else if (c == 'c' || c == 'C')
        {
            printf("CAL >> ya estas en el menu: elige 1, e o q\n");
        }
        else
        {
            printf("CAL >> opcion 0x%02X no valida\n", (unsigned)c);
        }
    }
}

// ---------------------------------------------------------------- tarea del menú

typedef struct
{
    bool icm_ready;
    SemaphoreHandle_t done;
} menu_ctx_t;

static void menu_task(void *arg)
{
    menu_ctx_t *ctx = (menu_ctx_t *)arg;
    boot_menu_run(ctx->icm_ready);
    printf("CAL >> pila libre minima de la tarea de calibracion: %u bytes\n", (unsigned)uxTaskGetStackHighWaterMark(NULL));
    xSemaphoreGive(ctx->done);
    vTaskDelete(NULL);
}

// El menú y las calibraciones corren en una tarea con pila propia (guardar en NVS y usar printf con
// decimales necesitan más pila de la que tiene el hilo principal por defecto). El hilo principal espera.
void calib_boot_menu(bool icm_ready)
{
    menu_ctx_t ctx = {.icm_ready = icm_ready, .done = xSemaphoreCreateBinary()};
    if (ctx.done == NULL || xTaskCreate(menu_task, "calib", CALIB_TASK_STACK, &ctx, 1, NULL) != pdPASS)
    {
        printf("CAL >> no se pudo crear la tarea de calibracion (memoria): se omite\n");
        if (ctx.done != NULL)
        {
            vSemaphoreDelete(ctx.done);
        }
        return;
    }
    xSemaphoreTake(ctx.done, portMAX_DELAY);
    vSemaphoreDelete(ctx.done);
}
