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
    mbedtls_x509_crt *current_level = pk_cert;
    for (int i = 0; i < MAX_BUNDLE_COUNT; i++)
    {
        if (certs[i] == NULL)
            continue;
        // TODO: check that current level matches cert[i]
        ESP_LOGD(TAG, "current_level %p", current_level);

        /**
         * \brief          Verify a chain of certificates.
         *
         *                 The verify callback is a user-supplied callback that
         *                 can clear / modify / add flags for a certificate. If set,
         *                 the verification callback is called for each
         *                 certificate in the chain (from the trust-ca down to the
         *                 presented crt). The parameters for the callback are:
         *                 (void *parameter, mbedtls_x509_crt *crt, int certificate_depth,
         *                 int *flags). With the flags representing current flags for
         *                 that specific certificate and the certificate depth from
         *                 the bottom (Peer cert depth = 0).
         *
         *                 All flags left after returning from the callback
         *                 are also returned to the application. The function should
         *                 return 0 for anything (including invalid certificates)
         *                 other than fatal error, as a non-zero return code
         *                 immediately aborts the verification process. For fatal
         *                 errors, a specific error code should be used (different
         *                 from MBEDTLS_ERR_X509_CERT_VERIFY_FAILED which should not
         *                 be returned at this point), or MBEDTLS_ERR_X509_FATAL_ERROR
         *                 can be used if no better code is available.
         *
         * \note           In case verification failed, the results can be displayed
         *                 using \c mbedtls_x509_crt_verify_info()
         *
         * \note           Same as \c mbedtls_x509_crt_verify_with_profile() with the
         *                 default security profile.
         *
         * \note           It is your responsibility to provide up-to-date CRLs for
         *                 all trusted CAs. If no CRL is provided for the CA that was
         *                 used to sign the certificate, CRL verification is skipped
         *                 silently, that is *without* setting any flag.
         *
         * \note           The \c trust_ca list can contain two types of certificates:
         *                 (1) those of trusted root CAs, so that certificates
         *                 chaining up to those CAs will be trusted, and (2)
         *                 self-signed end-entity certificates to be trusted (for
         *                 specific peers you know) - in that case, the self-signed
         *                 certificate doesn't need to have the CA bit set.
         *
         * \param crt      The certificate chain to be verified.
         * \param trust_ca The list of trusted CAs.
         * \param ca_crl   The list of CRLs for trusted CAs.
         * \param cn       The expected Common Name. This will be checked to be
         *                 present in the certificate's subjectAltNames extension or,
         *                 if this extension is absent, as a CN component in its
         *                 Subject name. Currently only DNS names are supported. This
         *                 may be \c NULL if the CN need not be verified.
         * \param flags    The address at which to store the result of the verification.
         *                 If the verification couldn't be completed, the flag value is
         *                 set to (uint32_t) -1.
         * \param f_vrfy   The verification callback to use. See the documentation
         *                 of mbedtls_x509_crt_verify() for more information.
         * \param p_vrfy   The context to be passed to \p f_vrfy.
         *
         * \return         \c 0 if the chain is valid with respect to the
         *                 passed CN, CAs, CRLs and security profile.
         * \return         #MBEDTLS_ERR_X509_CERT_VERIFY_FAILED in case the
         *                 certificate chain verification failed. In this case,
         *                 \c *flags will have one or more
         *                 \c MBEDTLS_X509_BADCERT_XXX or \c MBEDTLS_X509_BADCRL_XXX
         *                 flags set.
         * \return         Another negative error code in case of a fatal error
         *                 encountered during the verification process.
         */
        // int mbedtls_x509_crt_verify( mbedtls_x509_crt *crt,
        //                      mbedtls_x509_crt *trust_ca,
        //                      mbedtls_x509_crl *ca_crl,
        //                      const char *cn, uint32_t *flags,
        //                      int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
        //                      void *p_vrfy );

        /**
         * \brief          Verify a chain of certificates with respect to
         *                 a configurable security profile.
         *
         * \note           Same as \c mbedtls_x509_crt_verify(), but with explicit
         *                 security profile.
         *
         * \note           The restrictions on keys (RSA minimum size, allowed curves
         *                 for ECDSA) apply to all certificates: trusted root,
         *                 intermediate CAs if any, and end entity certificate.
         *
         * \param crt      The certificate chain to be verified.
         * \param trust_ca The list of trusted CAs.
         * \param ca_crl   The list of CRLs for trusted CAs.
         * \param profile  The security profile to use for the verification.
         * \param cn       The expected Common Name. This may be \c NULL if the
         *                 CN need not be verified.
         * \param flags    The address at which to store the result of the verification.
         *                 If the verification couldn't be completed, the flag value is
         *                 set to (uint32_t) -1.
         * \param f_vrfy   The verification callback to use. See the documentation
         *                 of mbedtls_x509_crt_verify() for more information.
         * \param p_vrfy   The context to be passed to \p f_vrfy.
         *
         * \return         \c 0 if the chain is valid with respect to the
         *                 passed CN, CAs, CRLs and security profile.
         * \return         #MBEDTLS_ERR_X509_CERT_VERIFY_FAILED in case the
         *                 certificate chain verification failed. In this case,
         *                 \c *flags will have one or more
         *                 \c MBEDTLS_X509_BADCERT_XXX or \c MBEDTLS_X509_BADCRL_XXX
         *                 flags set.
         * \return         Another negative error code in case of a fatal error
         *                 encountered during the verification process.
         */
        // int mbedtls_x509_crt_verify_with_profile( mbedtls_x509_crt *crt,
        //                      mbedtls_x509_crt *trust_ca,
        //                      mbedtls_x509_crl *ca_crl,
        //                      const mbedtls_x509_crt_profile *profile,
        //                      const char *cn, uint32_t *flags,
        //                      int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
        //                      void *p_vrfy );

        /**
         * \brief          Restartable version of \c mbedtls_crt_verify_with_profile()
         *
         * \note           Performs the same job as \c mbedtls_crt_verify_with_profile()
         *                 but can return early and restart according to the limit
         *                 set with \c mbedtls_ecp_set_max_ops() to reduce blocking.
         *
         * \param crt      The certificate chain to be verified.
         * \param trust_ca The list of trusted CAs.
         * \param ca_crl   The list of CRLs for trusted CAs.
         * \param profile  The security profile to use for the verification.
         * \param cn       The expected Common Name. This may be \c NULL if the
         *                 CN need not be verified.
         * \param flags    The address at which to store the result of the verification.
         *                 If the verification couldn't be completed, the flag value is
         *                 set to (uint32_t) -1.
         * \param f_vrfy   The verification callback to use. See the documentation
         *                 of mbedtls_x509_crt_verify() for more information.
         * \param p_vrfy   The context to be passed to \p f_vrfy.
         * \param rs_ctx   The restart context to use. This may be set to \c NULL
         *                 to disable restartable ECC.
         *
         * \return         See \c mbedtls_crt_verify_with_profile(), or
         * \return         #MBEDTLS_ERR_ECP_IN_PROGRESS if maximum number of
         *                 operations was reached: see \c mbedtls_ecp_set_max_ops().
         */
        // int mbedtls_x509_crt_verify_restartable( mbedtls_x509_crt *crt,
        //                      mbedtls_x509_crt *trust_ca,
        //                      mbedtls_x509_crl *ca_crl,
        //                      const mbedtls_x509_crt_profile *profile,
        //                      const char *cn, uint32_t *flags,
        //                      int (*f_vrfy)(void *, mbedtls_x509_crt *, int, uint32_t *),
        //                      void *p_vrfy,
        //                      mbedtls_x509_crt_restart_ctx *rs_ctx );
    }

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_random.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/rsa.h"
#include "mbedtls/error.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"

#define SELF_SIGNED_CERT_RSA_KEY_SIZE 2048
#define PEM_BUFFER_SIZE SELF_SIGNED_CERT_RSA_KEY_SIZE
#define SERIAL_NUMBER "1"

#define CERT_NOT_BEFORE "20200101000000"
#define CERT_NOT_AFTER "20401231235959"

unsigned char output_buf[PEM_BUFFER_SIZE]; // should not be allocated on the stack

bool gen_self_signed(void)
{
    int ret = 0;
    mbedtls_pk_context key;

    mbedtls_mpi N, P, Q, D, E, DP, DQ, QP;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;

    ESP_LOGI(TAG, "sizeof mpi %i", sizeof(mbedtls_mpi));

    unsigned char custom[32];
    esp_fill_random(custom, sizeof(custom));

    mbedtls_mpi_init(&N);
    mbedtls_mpi_init(&P);
    mbedtls_mpi_init(&Q);
    mbedtls_mpi_init(&D);
    mbedtls_mpi_init(&E);
    mbedtls_mpi_init(&DP);
    mbedtls_mpi_init(&DQ);
    mbedtls_mpi_init(&QP);

    mbedtls_pk_init(&key);

    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    memset(buf, 0, sizeof(buf));

    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, custom, sizeof(custom));
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        printf(" failed\n  ! mbedtls_ctr_drbg_seed returned -0x%04x\n", (unsigned int)-ret);
        goto cleanup;
    }

    ESP_LOGI(TAG, "entropy seed done");

    ret = mbedtls_pk_setup(&key, mbedtls_pk_info_from_type(MBEDTLS_PK_RSA));
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        ESP_LOGW(TAG, " failed\n  !  mbedtls_pk_setup returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

    ESP_LOGI(TAG, "pk setup done");

    ret = mbedtls_rsa_gen_key(mbedtls_pk_rsa(key), mbedtls_ctr_drbg_random, &ctr_drbg, SELF_SIGNED_CERT_RSA_KEY_SIZE, 65537);

    vTaskDelay(10 / portTICK_RATE_MS); // feed the watchdog

    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        ESP_LOGW(TAG, " failed\n  !  mbedtls_rsa_gen_key returned -0x%04x", (unsigned int)-ret);
        goto cleanup;
    }

    ESP_LOGI(TAG, "rsa gen key done");

    mbedtls_rsa_context *rsa = mbedtls_pk_rsa(key);
    if (rsa == NULL)
    {
        ESP_LOGW(TAG, " failed\n  !  mbedtls_pk_rsa returned NULL");
        goto cleanup;
    }

    ESP_LOGI(TAG, "rsa context done");

    ret = mbedtls_rsa_export(rsa, &N, &P, &Q, &D, &E);
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        ESP_LOGW(TAG, " failed\n  ! could not export RSA parameters part1\n\n");
        goto cleanup;
    }

    ESP_LOGI(TAG, "rsa context done");

    ret = mbedtls_rsa_export_crt(rsa, &DP, &DQ, &QP);
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        ESP_LOGW(TAG, " failed\n  ! could not export RSA parameters part 2\n\n");
        goto cleanup;
    }

    ESP_LOGI(TAG, "rsa export crt done");

    memset(output_buf, 0, PEM_BUFFER_SIZE);
    ret = mbedtls_pk_write_key_pem(&key, output_buf, PEM_BUFFER_SIZE);
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        ESP_LOGW(TAG, " pem failed\n");
        goto cleanup;
    }

    ESP_LOGE(TAG, "PEM KEY\r\n%s", (char *)output_buf);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    mbedtls_x509write_cert crt;
    mbedtls_mpi serial;
    char name[] = "CN=openfilpilote.local,O=local,C=FR";

    mbedtls_mpi_init(&serial);
    mbedtls_x509write_crt_init(&crt);

    memset(buf, 0, 1024);

    ret = mbedtls_mpi_read_string(&serial, 10, SERIAL_NUMBER);
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        mbedtls_strerror(ret, buf, 1024);
        ESP_LOGW(TAG, " failed\n  !  mbedtls_mpi_read_string "
                      "returned -0x%04x - %s\n\n",
                 (unsigned int)-ret, buf);
        goto cleanup;
    }

    ESP_LOGI(TAG, "read serial done");

    mbedtls_x509write_crt_set_subject_key(&crt, &key);
    ESP_LOGI(TAG, "set subject key done");

    mbedtls_x509write_crt_set_issuer_key(&crt, &key);
    ESP_LOGI(TAG, "set issuer key done");

    ret = mbedtls_x509write_crt_set_subject_name(&crt, name);
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        mbedtls_strerror(ret, buf, 1024);
        ESP_LOGW(TAG, " failed\n  !  mbedtls_x509write_crt_set_subject_name "
                      "returned -0x%04x - %s\n\n",
                 (unsigned int)-ret, buf);
        goto cleanup;
    }
    ESP_LOGI(TAG, "set subject name done");

    ret = mbedtls_x509write_crt_set_issuer_name(&crt, name);
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        mbedtls_strerror(ret, buf, 1024);
        ESP_LOGW(TAG, " failed\n  !  mbedtls_x509write_crt_set_issuer_name "
                      "returned -0x%04x - %s\n\n",
                 (unsigned int)-ret, buf);
        goto cleanup;
    }
    ESP_LOGI(TAG, "set issuer name done");

    mbedtls_x509write_crt_set_version(&crt, MBEDTLS_X509_CRT_VERSION_3);
    ESP_LOGI(TAG, "set version done");

    mbedtls_x509write_crt_set_md_alg(&crt, MBEDTLS_MD_SHA256);
    ESP_LOGI(TAG, "set digest alg done");

    ret = mbedtls_x509write_crt_set_serial(&crt, &serial);
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        mbedtls_strerror(ret, buf, 1024);
        ESP_LOGW(TAG, " failed\n  !  mbedtls_x509write_crt_set_serial "
                      "returned -0x%04x - %s\n\n",
                 (unsigned int)-ret, buf);
        goto cleanup;
    }
    ESP_LOGI(TAG, "set serial done");

    ret = mbedtls_x509write_crt_set_validity(&crt, CERT_NOT_BEFORE, CERT_NOT_AFTER);
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        mbedtls_strerror(ret, buf, 1024);
        ESP_LOGW(TAG, " failed\n  !  mbedtls_x509write_crt_set_validity "
                      "returned -0x%04x - %s\n\n",
                 (unsigned int)-ret, buf);
        goto cleanup;
    }
    ESP_LOGI(TAG, "set validity done");

#if defined(MBEDTLS_SHA1_C)
    ret = mbedtls_x509write_crt_set_subject_key_identifier(&crt);
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        mbedtls_strerror(ret, buf, 1024);
        ESP_LOGW(TAG, " failed\n  !  mbedtls_x509write_crt_set_subject"
                      "_key_identifier returned -0x%04x - %s\n\n",
                 (unsigned int)-ret, buf);
        goto cleanup;
    }
    ESP_LOGI(TAG, "set subj id done");

    ret = mbedtls_x509write_crt_set_authority_key_identifier(&crt);
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret != 0)
    {
        mbedtls_strerror(ret, buf, 1024);
        ESP_LOGW(TAG, " failed\n  !  mbedtls_x509write_crt_set_authority_"
                      "key_identifier returned -0x%04x - %s\n\n",
                 (unsigned int)-ret, buf);
        goto cleanup;
    }
    ESP_LOGI(TAG, "set issuer id done");
#endif // MBEDTLS_SHA1_C

    ret = mbedtls_x509write_crt_pem(&crt, output_buf, PEM_BUFFER_SIZE, mbedtls_ctr_drbg_random, &ctr_drbg);
    vTaskDelay(10 / portTICK_RATE_MS); // feed the watchdog
    ESP_LOGI(TAG, "ret %i", ret);
    if (ret < 0)
    {
        mbedtls_strerror(ret, buf, 1024);
        ESP_LOGW(TAG, " failed\n  !  write_certificate -0x%04x - %s\n\n",
                 (unsigned int)-ret, buf);
        goto cleanup;
    }
    ESP_LOGI(TAG, "write pem done");

    ESP_LOGE(TAG, "PEM CERT\r\n%s", (char *)output_buf);

    ret = 0;

cleanup:

    if (ret != 0)
    {
#ifdef MBEDTLS_ERROR_C
        mbedtls_strerror(ret, buf, sizeof(buf));
        ESP_LOGW(TAG, " - %s\n", buf);
#else
        ESP_LOGW(TAG, "\n");
#endif
    }

    mbedtls_x509write_crt_free(&crt);
    mbedtls_mpi_free(&serial);

    mbedtls_mpi_free(&N);
    mbedtls_mpi_free(&P);
    mbedtls_mpi_free(&Q);
    mbedtls_mpi_free(&D);
    mbedtls_mpi_free(&E);
    mbedtls_mpi_free(&DP);
    mbedtls_mpi_free(&DQ);
    mbedtls_mpi_free(&QP);

    mbedtls_pk_free(&key);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);

    ESP_LOGI(TAG, "cleanup done");
    return ret == 0;
}
