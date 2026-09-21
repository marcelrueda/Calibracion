// calib_store.h
// Guardado de calibraciones en NVS. Cada calibración es un bloque con cabecera
// (marca, versión, longitud y CRC32) para descartar datos dañados o de otra versión.
#ifndef CALIB_STORE_H
#define CALIB_STORE_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define CALIB_STORE_MAX_PAYLOAD  128

// Inicia NVS. Si está corrupta o es de otra versión de ESP-IDF, la BORRA (y con ella las calibraciones).
esp_err_t calib_store_init(void);

esp_err_t calib_store_save(const char *key, uint16_t version, const void *payload, size_t len);

// Devuelve ESP_OK, ESP_ERR_NOT_FOUND (nunca guardado), ESP_ERR_INVALID_CRC (dañado),
// ESP_ERR_INVALID_VERSION (otra versión) o ESP_ERR_INVALID_SIZE (otro tamaño).
esp_err_t calib_store_load(const char *key, uint16_t version, void *payload, size_t len);

esp_err_t calib_store_erase(const char *key);

#endif // CALIB_STORE_H
