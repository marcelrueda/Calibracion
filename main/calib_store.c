// calib_store.c
#include "calib_store.h"

#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "CALIB_STORE";

#define NVS_NAMESPACE  "calib"
#define BLOB_MAGIC     0x424C4143u   // "CALB" en little-endian

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t len;
    uint32_t crc;      // CRC32 del contenido
} blob_header_t;

// CRC-32 estándar (el mismo de zlib), bit a bit: es corto y no depende de la ROM del chip.
static uint32_t crc32_calc(const uint8_t *data, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
        {
            crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
        }
    }
    return ~crc;
}

esp_err_t calib_store_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "NVS corrupta o de otra version: se borra (se pierden las calibraciones guardadas)");
        ret = nvs_flash_erase();
        if (ret == ESP_OK)
        {
            ret = nvs_flash_init();
        }
    }
    return ret;
}

esp_err_t calib_store_save(const char *key, uint16_t version, const void *payload, size_t len)
{
    if (key == NULL || payload == NULL || len == 0 || len > CALIB_STORE_MAX_PAYLOAD)
    {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t buf[sizeof(blob_header_t) + CALIB_STORE_MAX_PAYLOAD];
    const blob_header_t h = {
        .magic = BLOB_MAGIC,
        .version = version,
        .len = (uint16_t)len,
        .crc = crc32_calc((const uint8_t *)payload, len),
    };
    memcpy(buf, &h, sizeof(h));
    memcpy(buf + sizeof(h), payload, len);

    nvs_handle_t nh;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nh);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = nvs_set_blob(nh, key, buf, sizeof(h) + len);
    if (ret == ESP_OK)
    {
        ret = nvs_commit(nh);
    }
    nvs_close(nh);
    return ret;
}

esp_err_t calib_store_load(const char *key, uint16_t version, void *payload, size_t len)
{
    if (key == NULL || payload == NULL || len == 0 || len > CALIB_STORE_MAX_PAYLOAD)
    {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nh;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nh);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_ERR_NOT_FOUND;      // aún no existe el espacio de nombres: nada guardado
    }
    if (ret != ESP_OK)
    {
        return ret;
    }

    uint8_t buf[sizeof(blob_header_t) + CALIB_STORE_MAX_PAYLOAD];
    size_t sz = sizeof(buf);
    ret = nvs_get_blob(nh, key, buf, &sz);
    nvs_close(nh);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        return ESP_ERR_NOT_FOUND;
    }
    if (ret == ESP_ERR_NVS_INVALID_LENGTH)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    if (ret != ESP_OK)
    {
        return ret;
    }

    blob_header_t h;
    if (sz < sizeof(h))
    {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(&h, buf, sizeof(h));
    if (h.magic != BLOB_MAGIC || sz != sizeof(h) + h.len)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    if (h.version != version)
    {
        return ESP_ERR_INVALID_VERSION;
    }
    if (h.len != len)
    {
        return ESP_ERR_INVALID_SIZE;
    }
    if (crc32_calc(buf + sizeof(h), h.len) != h.crc)
    {
        return ESP_ERR_INVALID_CRC;
    }
    memcpy(payload, buf + sizeof(h), len);
    return ESP_OK;
}

esp_err_t calib_store_erase(const char *key)
{
    nvs_handle_t nh;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nh);
    if (ret != ESP_OK)
    {
        return ret;
    }
    ret = nvs_erase_key(nh, key);
    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        ret = ESP_OK;                  // ya no había nada que borrar
    }
    if (ret == ESP_OK)
    {
        ret = nvs_commit(nh);
    }
    nvs_close(nh);
    return ret;
}
