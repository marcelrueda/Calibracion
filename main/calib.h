// calib.h
// Calibración estática del ICM-42670-P (giroscopio ahora; acelerómetro en la etapa siguiente),
// con guardado en NVS y menú de arranque.
#ifndef CALIB_H
#define CALIB_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "icm42670p.h"

// ---- Criterios de calibración del giroscopio (unidad quieta) ----
#define CALIB_GYRO_SETTLE_S      5      // s de espera para que se asiente tras pulsar la tecla
#define CALIB_GYRO_CAPTURE_S     30     // s de captura
#define CALIB_GYRO_MIN_SAMPLES   500    // muestras mínimas para aceptar el resultado
#define CALIB_GYRO_MAX_STD_DPS   0.25f  // más dispersión que esto => la unidad se movió
#define CALIB_GYRO_SPEC_DPS      1.0f   // sesgo inicial máximo del datasheet a 25 °C (±1 °/s)
#define CALIB_GYRO_MAX_ABS_DPS   3.0f   // hasta aquí se guarda sin preguntar
#define CALIB_GYRO_HARD_MAX_DPS  30.0f  // más que esto no se guarda nunca (no es un cero de giroscopio normal)
#define CALIB_CONFIRM_S          20     // s para confirmar el guardado de un sesgo fuera de especificación

#define CALIB_ERR_MOTION   ESP_ERR_INVALID_STATE      // se detectó movimiento durante la captura
#define CALIB_ERR_RANGE    ESP_ERR_INVALID_RESPONSE   // sesgo fuera de especificación: solo se guarda si se confirma
#define CALIB_ERR_ABSURD   ESP_ERR_NOT_SUPPORTED      // sesgo imposible: nunca se guarda

#define CALIB_TASK_STACK         8192   // pila (bytes) de la tarea que ejecuta el menú y las calibraciones

#define CALIB_BOOT_WINDOW_S      5      // ventana de arranque para pulsar C
#define CALIB_MENU_TIMEOUT_S     60     // sin pulsar nada en el menú, se continúa solo

// Calibración del ICM (formato versión 1). Los campos del acelerómetro ya existen para no
// cambiar el formato cuando se agregue; mientras acc_valid = 0 no se aplican.
typedef struct
{
    uint8_t gyro_valid;         // 1 = sesgo del giroscopio calibrado
    uint8_t acc_valid;          // 1 = acelerómetro calibrado
    uint8_t gyro_out_of_spec;   // 1 = el sesgo superaba la especificación y se guardó tras confirmarlo
    uint8_t reserved;
    float gyro_bias_dps[3];     // se resta a la lectura
    float acc_offset_g[3];      // se resta a la lectura
    float acc_scale[3];         // la lectura restante se divide por esto (1.0 = sin corrección)
} calib_icm_t;

// Estadística acumulada (Welford) de un vector de 3 ejes.
typedef struct
{
    uint32_t n;
    double mean[3];
    double m2[3];
} stats3_t;

void stats3_init(stats3_t *s);
void stats3_add(stats3_t *s, const float v[3]);
void stats3_result(const stats3_t *s, float mean[3], float std[3]);

// Evalúa las estadísticas de un giroscopio en reposo. Devuelve ESP_OK, ESP_ERR_INVALID_SIZE (pocas
// muestras), CALIB_ERR_MOTION, CALIB_ERR_RANGE o CALIB_ERR_ABSURD. Con RANGE y OK, bias_out trae el sesgo.
esp_err_t calib_gyro_evaluate(const stats3_t *s, float bias_out[3], float std_out[3]);

// Inicia NVS y carga las calibraciones guardadas (si no hay, queda sin corrección).
esp_err_t calib_init(void);

// Resumen de lo cargado, por el monitor.
void calib_print_status(void);

const calib_icm_t *calib_icm(void);

// Aplica la calibración a una muestra del ICM (giroscopio ya; acelerómetro cuando esté calibrado).
void calib_icm_apply(icm_sample_t *s);

// Calibración interactiva del giroscopio: unidad quieta, ~35 s. Guarda en NVS si el resultado sirve.
esp_err_t calib_run_gyro(void);

// Ventana de arranque: si se pulsa C, abre el menú; si no, continúa sin calibrar.
void calib_boot_menu(bool icm_ready);

#endif // CALIB_H
