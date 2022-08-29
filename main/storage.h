#ifndef STORAGE_H
#define STORAGE_H

#include <nvs.h>

/* text functions */
const char *kv_type_str_from_nvs_type(nvs_type_t type);

bool kv_build_ns_hardware(const char *hw_id, char *buf);
bool kv_set_ns_current_hardware(const char *hw_id);
const char *kv_get_ns_hardware(void);
const char *kv_get_ns_ofp(void);
bool kv_build_ns_zone(const char *hw_id, char *buf);
bool kv_set_ns_current_zone(const char *hw_id);
const char *kv_get_ns_zone(void);

/* partition functions */
void kv_erase(const char *part_name);
void kv_init(const char *part_name);
void kv_deinit(const char *part_name);

/* wrapper functions */
void kv_clear_ns(const char *ns);
void kv_delete_key_ns(const char *ns, const char *key);

/* functions requiring an opened handle */
nvs_handle_t kv_open_ns(const char *ns);
void kv_commit(nvs_handle_t handle);
void kv_close(nvs_handle_t handle);

void kv_clear(nvs_handle_t handle);
void kv_delete_key(nvs_handle_t handle, const char *key);

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

char *kv_get_str(nvs_handle_t handle, const char *key);                  /* Returned memory MUST BE FREED (if not NULL) BY THE CALLER  */
void *kv_get_blob(nvs_handle_t handle, const char *key, size_t *length); /* Returned memory MUST BE FREED (if not NULL) BY THE CALLER  */

/* single "atomic" functions (open/action/commit/close) for single access */
void kv_ns_set_i8_atomic(char *namespace, const char *key, int8_t value);
void kv_ns_set_u8_atomic(char *namespace, const char *key, uint8_t value);
void kv_ns_set_i16_atomic(char *namespace, const char *key, int16_t value);
void kv_ns_set_u16_atomic(char *namespace, const char *key, uint16_t value);
void kv_ns_set_i32_atomic(char *namespace, const char *key, int32_t value);
void kv_ns_set_u32_atomic(char *namespace, const char *key, uint32_t value);
void kv_ns_set_i64_atomic(char *namespace, const char *key, int64_t value);
void kv_ns_set_u64_atomic(char *namespace, const char *key, uint64_t value);

void kv_ns_set_str_atomic(char *namespace, const char *key, const char *value);
void kv_ns_set_blob_atomic(char *namespace, const char *key, const void *value, size_t length);

int8_t kv_ns_get_i8_atomic(char *namespace, const char *key, int8_t def_value);
uint8_t kv_ns_get_u8_atomic(char *namespace, const char *key, uint8_t def_value);
int16_t kv_ns_get_i16_atomic(char *namespace, const char *key, int16_t def_value);
uint16_t kv_ns_get_u16_atomic(char *namespace, const char *key, uint16_t def_value);
int32_t kv_ns_get_i32_atomic(char *namespace, const char *key, int32_t def_value);
uint32_t kv_ns_get_u32_atomic(char *namespace, const char *key, uint32_t def_value);
int64_t kv_ns_get_i64_atomic(char *namespace, const char *key, int64_t def_value);
uint64_t kv_ns_get_u64_atomic(char *namespace, const char *key, uint64_t def_value);

char *kv_ns_get_str_atomic(char *namespace, const char *key);                  /* Returned memory MUST BE FREED (if not NULL) BY THE CALLER  */
void *kv_ns_get_blob_atomic(char *namespace, const char *key, size_t *length); /* Returned memory MUST BE FREED (if not NULL) BY THE CALLER  */

#endif /* STORAGE_H */