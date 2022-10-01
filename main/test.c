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
        ESP_LOGD(TAG, "mbedtls_x509_crt_info %i", result);
        assert(result > 0);
        printf("\r\nCertificate #%i\r\n%s\r\n", index++, buf);
        it = it->next;
    }
}

void parse_cert(const unsigned char *start, size_t len, mbedtls_x509_crt *crt)
{
    // ESP_LOGI(TAG, "%.*s", len, start);
    int result = pem_parse_single_certificate((const char *)start, len, crt);
    ESP_LOGD(TAG, "pem_parse_single_certificate 0x%x", -result);
    assert(result == 0);

    mbedtls_x509_crt *it = crt;
    while (it != NULL)
    {
        ESP_LOGD(TAG, "crt %p", it);
        it = it->next;
    }
}

void parse_key(const unsigned char *start, size_t len, mbedtls_pk_context *ctx)
{
    // ESP_LOGI(TAG, "%.*s", len, start);
    ESP_LOGD(TAG, "pem_parse_single_private_key 0x%x", -pem_parse_single_private_key((const char *)start, len, NULL, 0, ctx));
}

extern const unsigned char full_crt_start[] asm("_binary_full_crt_start");
extern const unsigned char full_crt_end[] asm("_binary_full_crt_end");

extern const unsigned char full_nil_crt_start[] asm("_binary_full_nil_crt_start");
extern const unsigned char full_nil_crt_end[] asm("_binary_full_nil_crt_end");

extern const unsigned char crt_nipil_crt_start[] asm("_binary_crt_nipil_crt_start");
extern const unsigned char crt_nipil_crt_end[] asm("_binary_crt_nipil_crt_end");
extern const unsigned char crt_le_crt_start[] asm("_binary_crt_le_crt_start");
extern const unsigned char crt_le_crt_end[] asm("_binary_crt_le_crt_end");
extern const unsigned char crt_isrgx1_crt_start[] asm("_binary_crt_isrgx1_crt_start");
extern const unsigned char crt_isrgx1_crt_end[] asm("_binary_crt_isrgx1_crt_end");
extern const unsigned char nipil_key_start[] asm("_binary_nipil_key_start");
extern const unsigned char nipil_key_end[] asm("_binary_nipil_key_end");

#define MAX_BUNDLE_COUNT 3

bool is_certificate_matching_private_key(mbedtls_x509_crt *certificate, mbedtls_pk_context *private_key)
{
    if (certificate == NULL || private_key == NULL)
        return false;

    /*
     * 0 = success
     * MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE could not be checked
     * MBEDTLS_ERR_PK_BAD_INPUT_DATA invalid context
     * not zero if no match
     */
    int result = mbedtls_pk_check_pair(&certificate->pk, private_key);
    ESP_LOGD(TAG, "mbedtls_pk_check_pair %i", result);
    switch (result)
    {
    case 0:
        ESP_LOGI(TAG, "Certificate %p and private key %p match", certificate, private_key);
        return true;
    case MBEDTLS_ERR_PK_FEATURE_UNAVAILABLE:
        ESP_LOGW(TAG, "Cannot compare certificate %p and private key %p due to unavailable feature", certificate, private_key);
        return false;
    case MBEDTLS_ERR_PK_BAD_INPUT_DATA:
        ESP_LOGW(TAG, "Bad input data in private_key %p", private_key);
        return false;
    default:
        ESP_LOGD(TAG, "No key match between certificate %p and private_key %p", certificate, private_key);
        return false;
    }
}

void do_test()
{
    esp_log_level_set(TAG, ESP_LOG_VERBOSE); // debug

    ESP_LOGI(TAG, "full_crt");
    mbedtls_x509_crt full_crt;
    parse_cert(full_crt_start, full_crt_end - full_crt_start, &full_crt);

    print_cert_chain(&full_crt);

    ESP_LOGI(TAG, "full_nil_crt");
    mbedtls_x509_crt full_nil_crt;
    parse_cert(full_nil_crt_start, full_nil_crt_end - full_nil_crt_start, &full_nil_crt);

    ESP_LOGI(TAG, "crt_nipil_crt");
    mbedtls_x509_crt crt_nipil_crt;
    parse_cert(crt_nipil_crt_start, crt_nipil_crt_end - crt_nipil_crt_start, &crt_nipil_crt);

    ESP_LOGI(TAG, "crt_le_crt");
    mbedtls_x509_crt crt_le_crt;
    parse_cert(crt_le_crt_start, crt_le_crt_end - crt_le_crt_start, &crt_le_crt);

    ESP_LOGI(TAG, "crt_isrgx1_crt");
    mbedtls_x509_crt crt_isrgx1_crt;
    parse_cert(crt_isrgx1_crt_start, crt_isrgx1_crt_end - crt_isrgx1_crt_start, &crt_isrgx1_crt);

    ESP_LOGI(TAG, "nipil_key");
    mbedtls_pk_context nipil_key;
    parse_key(nipil_key_start, nipil_key_end - nipil_key_start, &nipil_key);

    mbedtls_x509_crt *certs[MAX_BUNDLE_COUNT] = {&full_nil_crt, &crt_le_crt, &crt_isrgx1_crt};
    mbedtls_pk_context *key = &nipil_key;

    // searching (and remove) certificates matching the private key
    mbedtls_x509_crt *pk_cert = NULL;
    for (int i = 0; i < MAX_BUNDLE_COUNT; i++)
    {
        bool result = is_certificate_matching_private_key(certs[i], key);
        ESP_LOGD(TAG, "is_certificate_matching_private_key %i", result);
        if (result)
        {
            pk_cert = certs[i];
            certs[i] = NULL;
        }
    }

    ESP_LOGD(TAG, "pk_cert %p", pk_cert);
    if (pk_cert == NULL)
    {
        ESP_LOGD(TAG, "no certificate found matching for provided private key");
        goto cleanup;
    }

    print_cert_chain(pk_cert);

    // search for parent certificate
    mbedtls_x509_crt *current = pk_cert;
    bool found;
    int non_empty_count;
    do
    {
        non_empty_count = 0;
        found = false;
        for (int i = 0; i < MAX_BUNDLE_COUNT; i++)
        {
            if (certs[i] == NULL)
                continue;
            ESP_LOGV(TAG, "iter %i candidate %p current %p", i, certs[i], current);
            non_empty_count++;
            uint32_t flags;
            int ret = mbedtls_x509_crt_verify(pk_cert, certs[i], NULL, NULL, &flags, NULL, NULL);
            ESP_LOGD(TAG, "mbedtls_x509_crt_verify ret 0x%x flags 0x%08x", -ret, flags);

            // MBEDTLS_ERR_X509_CERT_VERIFY_FAILED
            if (ret != 0)
                continue;

            // candidate signed the current cert
            found = true;
            current->next = certs[i];
            current = certs[i];
            certs[i] = NULL;
        }
    } while (found);

    // print remaining certificate count
    if (non_empty_count != 0)
    {
        printf("\nRemaining certificates which were not linked to the chain: %i", non_empty_count);
        goto cleanup;
    }

    // resulting chain
    print_cert_chain(pk_cert);

    // TODO: beware for cleanup, when certs are chained, the free on the leaf frees all parents, so maybe unlink the cert before freeing them

cleanup:
    ESP_LOGI(TAG, "Cleaning now.");

    mbedtls_x509_crt_free(&full_crt);
    ESP_LOGD(TAG, "Done full_crt, cleaning next.");
    mbedtls_x509_crt_free(&crt_nipil_crt);
    ESP_LOGD(TAG, "Done crt_nipil_crt, cleaning next.");
    mbedtls_x509_crt_free(&crt_le_crt);
    ESP_LOGD(TAG, "Done crt_le_crt, cleaning next.");
    mbedtls_x509_crt_free(&crt_isrgx1_crt);
    ESP_LOGD(TAG, "Done crt_isrgx1_crt, cleaning next.");
    mbedtls_pk_free(&nipil_key);
    ESP_LOGD(TAG, "Done.");
}
