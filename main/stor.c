#include "nvs_flash.h"
#include "nvs.h"

#include "esp_system.h"
#include "esp_log.h"

#include "stor.h"

static const char *TAG = "STOR";

nvs_handle_t open_stor(const char *name)
{
  nvs_handle_t storage_handle;

  ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle... ");
  esp_err_t err = nvs_open(name, NVS_READWRITE, &storage_handle);

  if (err != ESP_OK)
  {
    ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    abort();
  }

  return storage_handle;
}

int32_t stor_get_i32(const char *storage_name, const char *key, int32_t default_value)
{
  nvs_handle_t storage_handle = open_stor(storage_name);

  int32_t value = default_value;

  esp_err_t err = nvs_get_i32(storage_handle, key, &value);
  switch (err)
  {
  case ESP_OK:
    ESP_LOGI(TAG, "Done");
    ESP_LOGI(TAG, "Int32 value = %d", value);
    break;

  case ESP_ERR_NVS_NOT_FOUND:
    ESP_LOGI(TAG, "The value is not initialized yet, setting default value %i!", value);
    stor_set_i32(storage_name, key, default_value);
    // value = default_value;
    break;

  default:
    ESP_LOGI(TAG, "Error (%s) reading!", esp_err_to_name(err));
  }

  nvs_close(storage_handle);

  return value;
}

esp_err_t stor_set_i32(const char *storage_name, const char *key, const int32_t value)
{
  nvs_handle_t storage_handle = open_stor(storage_name);

  esp_err_t err = nvs_set_i32(storage_handle, key, value);
  if (err == ESP_OK)
  {
    nvs_commit(storage_handle);
    ESP_LOGI(TAG, "The value %i for key %s has been written!", value, key);
  }
  else
  {
    ESP_LOGI(TAG, "Error (%s) writing!", esp_err_to_name(err));
  }

  nvs_close(storage_handle);

  return err;
}

int64_t stor_get_i64(const char *storage_name, const char *key, int64_t default_value)
{
  nvs_handle_t storage_handle = open_stor(storage_name);

  int64_t value = default_value;

  esp_err_t err = nvs_get_i64(storage_handle, key, &value);
  switch (err)
  {
  case ESP_OK:
    ESP_LOGI(TAG, "Done");
    ESP_LOGI(TAG, "Int64 value = %lli", value);
    break;

  case ESP_ERR_NVS_NOT_FOUND:
    ESP_LOGI(TAG, "The value is not initialized yet, setting default value %lli!", value);
    stor_set_i64(storage_name, key, default_value);
    // value = default_value;
    break;

  default:
    ESP_LOGI(TAG, "Error (%s) reading!", esp_err_to_name(err));
  }

  nvs_close(storage_handle);

  return value;
}

esp_err_t stor_set_i64(const char *storage_name, const char *key, const int64_t value)
{
  nvs_handle_t storage_handle = open_stor(storage_name);

  esp_err_t err = nvs_set_i64(storage_handle, key, value);
  if (err == ESP_OK)
  {
    nvs_commit(storage_handle);
    ESP_LOGI(TAG, "The value %lli for key %s has been written!", value, key);
  }
  else
  {
    ESP_LOGI(TAG, "Error (%s) writing!", esp_err_to_name(err));
  }

  nvs_close(storage_handle);

  return err;
}