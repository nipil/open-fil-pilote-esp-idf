#include <stdio.h>
#include <esp_log.h>

#include "utils.h"

char buf[4096];

const char TAG[] = "test";

void print_cert_chain(mbedtls_x509_crt *crt)
{
    int index = 0, result;
    mbedtls_x509_crt *it = crt;
    while (it != NULL)
    {
        result = mbedtls_x509_crt_info(buf, sizeof(buf) - 1, "\t", it);
        ESP_LOGV(TAG, "mbedtls_x509_crt_info %i", result);
        assert(result > 0);
        printf("\r\nCertificate #%i\r\n%s\r\n", index++, buf);
        it = it->next;
    }
}
