#ifndef STORAGE_H
#define STORAGE_H

#include <nvs.h>

/* macros for single value access */

#define kvh_set(type_name, ns, key, val, ...)                           \
    do                                                                  \
    {                                                                   \
        nvs_handle_t nvs_h;                                             \
        nvs_h = kv_open_ns(ns);                                         \
        kv_set_##type_name(nvs_h, key, val __VA_OPT__(, ) __VA_ARGS__); \
        kv_commit(nvs_h);                                               \
        kv_close(nvs_h);                                                \
    } while (0)

#define kvh_get(target, type_name, ns, key, ...)                            \
    do                                                                      \
    {                                                                       \
        nvs_handle_t nvs_h;                                                 \
        nvs_h = kv_open_ns(ns);                                             \
        target = kv_get_##type_name(nvs_h, key __VA_OPT__(, ) __VA_ARGS__); \
        kv_close(nvs_h);                                                    \
    } while (0)

/* functions */

void part_list(void);

void kv_erase(void);
void kv_stats(void);
void kv_init(void);
void kv_list_ns(const char *ns);

nvs_handle_t kv_open_ns(const char *ns);
void kv_commit(nvs_handle_t handle);
void kv_close(nvs_handle_t handle);

void kv_set_i8(nvs_handle_t handle, const char *key, int8_t value);
void kv_set_u8(nvs_handle_t handle, const char *key, uint8_t value);
void kv_set_i16(nvs_handle_t handle, const char *key, int16_t value);
void kv_set_u16(nvs_handle_t handle, const char *key, uint16_t value);
void kv_set_i32(nvs_handle_t handle, const char *key, int32_t value);
void kv_set_u32(nvs_handle_t handle, const char *key, uint32_t value);
void kv_set_i64(nvs_handle_t handle, const char *key, int64_t value);
void kv_set_u64(nvs_handle_t handle, const char *key, uint64_t value);

void kv_set_str(nvs_handle_t handle, const char *key, const char *value);
void kv_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length);

int8_t kv_get_i8(nvs_handle_t handle, const char *key, int8_t def_value);
uint8_t kv_get_u8(nvs_handle_t handle, const char *key, uint8_t def_value);
int16_t kv_get_i16(nvs_handle_t handle, const char *key, int16_t def_value);
uint16_t kv_get_u16(nvs_handle_t handle, const char *key, uint16_t def_value);
int32_t kv_get_i32(nvs_handle_t handle, const char *key, int32_t def_value);
uint32_t kv_get_u32(nvs_handle_t handle, const char *key, uint32_t def_value);
int64_t kv_get_i64(nvs_handle_t handle, const char *key, int64_t def_value);
uint64_t kv_get_u64(nvs_handle_t handle, const char *key, uint64_t def_value);

/* Returned memory MUST BE FREED (or owned) BY THE CALLER */
char *kv_get_str(nvs_handle_t handle, const char *key);
void *kv_get_blob(nvs_handle_t handle, const char *key, size_t *length);

#endif /* STORAGE_H */