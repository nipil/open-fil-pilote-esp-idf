#ifndef STORAGE_H
#define STORAGE_H

#include <nvs.h>

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

#endif /* STORAGE_H */