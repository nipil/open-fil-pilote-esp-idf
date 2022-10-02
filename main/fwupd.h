#ifndef FWUPD_H
#define FWUPD_H

#include <esp_ota_ops.h>

struct fwupd_data
{
    esp_ota_handle_t update_handle;
    const esp_partition_t *next_partition;
    bool headers_checked;
};

esp_err_t fwupd_begin(struct fwupd_data *data);
esp_err_t fwupd_write(struct fwupd_data *data, const void *buf, size_t len);
esp_err_t fwupd_end(struct fwupd_data *data);

void fwupd_log_part_info(void);

#endif /* FWUPD_H */
