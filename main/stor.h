#ifndef _STORAGE_H_
#define _STORAGE_H_

#include "esp_system.h"

int32_t stor_get_i32(const char *storage_name, const char *key, int32_t default_value);
esp_err_t stor_set_i32(const char *storage_name, const char *key, const int32_t value);
int64_t stor_get_i64(const char *storage_name, const char *key, int64_t default_value);
esp_err_t stor_set_i64(const char *storage_name, const char *key, const int64_t value);

#endif