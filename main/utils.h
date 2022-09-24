#ifndef UTILS_H
#define UTILS_H

#include <time.h>
#include <regex.h>
#include <mbedtls/md.h>
#include <esp_log.h>

#include <cJSON.h>

#define LOCALTIME_TO_STRING_BUFFER_LENGTH 64

// Some useful macros (source: linux kernel)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/*
 * A set of macro to count the number of variadic arguments
 *
 * See below for the general idea
 * https://stackoverflow.com/questions/4421681/how-to-count-the-number-of-arguments-passed-to-a-function-that-accepts-a-variabl
 *
 * See below for the optional comma using __VA_OPT__ in case zero arguments are provided
 * https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html
 *
 * See below for the fix i added for zero arguments (adding _0 to PP_128TH_ARG and using __VA_OPT__ in PP_NARG)
 * https://stackoverflow.com/questions/2124339/c-preprocessor-va-args-number-of-arguments/36015150#36015150
 *
 * Test/verify using preprocessor only : gcc -E -c api.c -o pp_foo.c
 */
#define PP_NARG(...) \
    PP_NARG_(__VA_OPT__(, ) __VA_ARGS__, PP_RSEQ_N())

#define PP_NARG_(...) \
    PP_128TH_ARG(__VA_ARGS__)

#define PP_128TH_ARG(                                  \
    _0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10,       \
    _11, _12, _13, _14, _15, _16, _17, _18, _19, _20,  \
    _21, _22, _23, _24, _25, _26, _27, _28, _29, _30,  \
    _31, _32, _33, _34, _35, _36, _37, _38, _39, _40,  \
    _41, _42, _43, _44, _45, _46, _47, _48, _49, _50,  \
    _51, _52, _53, _54, _55, _56, _57, _58, _59, _60,  \
    _61, _62, _63, _64, _65, _66, _67, _68, _69, _70,  \
    _71, _72, _73, _74, _75, _76, _77, _78, _79, _80,  \
    _81, _82, _83, _84, _85, _86, _87, _88, _89, _90,  \
    _91, _92, _93, _94, _95, _96, _97, _98, _99, _100, \
    _101, _102, _103, _104, _105, _106, _107, _108,    \
    _109, _110, _111, _112, _113, _114, _115, _116,    \
    _117, _118, _119, _120, _121, _122, _123, _124,    \
    _125, _126, _127, N, ...) N

#define PP_RSEQ_N()                                       \
    127, 126, 125, 124, 123, 122, 121, 120,               \
        119, 118, 117, 116, 115, 114, 113, 112, 111, 110, \
        109, 108, 107, 106, 105, 104, 103, 102, 101, 100, \
        99, 98, 97, 96, 95, 94, 93, 92, 91, 90,           \
        89, 88, 87, 86, 85, 84, 83, 82, 81, 80,           \
        79, 78, 77, 76, 75, 74, 73, 72, 71, 70,           \
        69, 68, 67, 66, 65, 64, 63, 62, 61, 60,           \
        59, 58, 57, 56, 55, 54, 53, 52, 51, 50,           \
        49, 48, 47, 46, 45, 44, 43, 42, 41, 40,           \
        39, 38, 37, 36, 35, 34, 33, 32, 31, 30,           \
        29, 28, 27, 26, 25, 24, 23, 22, 21, 20,           \
        19, 18, 17, 16, 15, 14, 13, 12, 11, 10,           \
        9, 8, 7, 6, 5, 4, 3, 2, 1, 0

// json helpers

enum json_helper_result
{
    JSON_HELPER_RESULT_SUCCESS = 0,
    JSON_HELPER_RESULT_NOT_FOUND = -1,
    JSON_HELPER_RESULT_INVALID = -2,
};

enum json_helper_result cjson_get_child_int(cJSON *node, const char *key, int *target);
enum json_helper_result cjson_get_child_string(cJSON *node, const char *key, char **target);

// regexp helper structure
struct re_result
{
    int count;
    char **strings;
};

// crypto structures

struct password_data
{
    mbedtls_md_type_t md_type;
    size_t iterations;
    uint8_t *salt;
    size_t salt_len;
    uint8_t *hash;
    size_t hash_len;
};

enum certificate_bundle_iter_state
{
    CBIS_END_FAIL = -1,
    CBIS_END_OK = 0,
    CBIS_IDLE = 1,
    CBIS_CERTIFICATE,
    CBIS_PRIVATE_KEY,
};

struct certificate_bundle_iter
{
    const char *current;
    int remaining;
    enum certificate_bundle_iter_state state;
    const char *block_start;
    int block_len;
};

// time conversions
void time_to_localtime(time_t *val, struct tm *timeinfo);
void localtime_to_string(struct tm *timeinfo, char *buf, int buf_len);

// time logging
void log_current_localtime(const char *tag);

// regex
void log_regerror(const char *TAG, regex_t *re, int res);

// creates a copy of a substring, which MUST BE FREED BY THE CALLER
char *substr(const char *src, int offset, int length);

/*
 * Joins strings using separator
 * - result MUST BE FREED BY THE CALLER
 * - every variable argument must be a (char *)
 * - parameter'sep' can be NULL to disable separators
 *
 * Use the macro if you want the pre-compiler to automatically compute the number of arguments
 * Use the function itself if/when you want to specify the number of arguments yourself
 */
#define joinstr(sep, ...) \
    joinstr_nargs(sep, PP_NARG(__VA_ARGS__) __VA_OPT__(, ) __VA_ARGS__)
char *joinstr_nargs(char *sep, int nargs, ...);

/*
 * Match a string, and handle memory management for captures
 *
 * Returns NULL on failure
 *
 * The returned pointer (if not NULL) MUST BE FREED BY THE CALLER using re_free()
 */
struct re_result *re_match(const char *re, const char *str);
void re_free(struct re_result *r);

/* safer accessors */
int re_get_int(struct re_result *result, int index);
char *re_get_string(struct re_result *result, int index);

/* comparison functions */
inline int min_int(const int a, const int b) { return ((a < b) ? a : b); }

/*
 * string splitting functions
 *
 * Returns NULL on failure
 *
 * The returned pointer (if not NULL) MUST BE FREED BY THE CALLER using split_string_free()
 */
struct split_result
{
    int count;
    char **strings;
};

void split_string_free(struct split_result *splits);
struct split_result *split_string(const char *str, char sep);

/*
 * decode application/x-www-form-urlencoded
 *
 * Returns NULL on failure
 *
 * The returned pointer (if not NULL) MUST BE FREED BY THE CALLER using form_data_free()
 */

struct ofp_form_param
{
    char *name;
    char *value;
};

struct ofp_form_data
{
    int count;
    struct ofp_form_param *params;
};

void form_data_free(struct ofp_form_data *data);
struct ofp_form_data *form_data_parse(const char *data);
char *form_data_get_str(struct ofp_form_data *data, const char *name);

/*
 * Converts a URL-encoded string back to plaintext
 *
 * Returned pointer (if not NULL) MUST BE FREED BY THE CALLER
 */
char *form_data_decode_str(const char *str);

/*
 * Converts an hexadecimal ASCII character [a-fA-F0-9] to its corresponding (0..15) value
 * Returns -1 if provided character is not hexadecimal
 */
int hex_char_to_val(const char c);

/*
 * Verifies the format of a string and if correct, converts to its value
 * Returns true if successful, false if the format is invalid (spaces, etc)
 */
bool parse_int(const char *str, int *target);

/* wait functions */
void wait_ms(uint32_t ms);
void wait_sec(uint32_t sec);

/* crypto functions */
bool hmac_md(mbedtls_md_type_t md_type, const uint8_t *salt, size_t salt_len, const uint8_t *data, size_t data_len, uint8_t *output, uint8_t *output_len);
bool hmac_md_iterations(mbedtls_md_type_t md_type, const uint8_t *salt, size_t salt_len, const uint8_t *data, size_t data_len, uint8_t *output, uint8_t *output_len, unsigned int iterations);

void password_log(struct password_data *pwd, const char *tag, esp_log_level_t log_level);
struct password_data *password_init(const char *cleartext);
bool password_free(struct password_data *pwd);
bool password_verify(struct password_data *pwd, const char *cleartext);
char *password_to_string(struct password_data *pwd);
struct password_data *password_from_string(const char *str);

int certificate_check_public_single(const unsigned char *cert_str, size_t cert_len);
int certificate_check_private(const unsigned char *key_str, size_t key_len, unsigned char *key_pass, size_t key_pass_len);

/*
 * (PEM) Certificate bundle iterator function
 *
 * "next" returns true each time it finds a block, and false at the end of on failure
 * "state" tells the kind of block, or the final iteration result (OK or FAIL)
 */
bool certificate_bundle_iter_next(struct certificate_bundle_iter *it);
struct certificate_bundle_iter *certificate_bundle_iter_init(const void *buf, size_t len);
void certificate_bundle_iter_free(struct certificate_bundle_iter *it);
void certificate_bundle_iter_log(struct certificate_bundle_iter *it, const char *tag, esp_log_level_t level);

#endif /* UTILS_H */
