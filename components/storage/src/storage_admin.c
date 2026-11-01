#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "storage.h"
#include "storage_internal.h"

#include <ctype.h>
#include <stdint.h>
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/task.h"
#include "psa/crypto.h"
#define ADMIN_PIN_ITERATIONS 4096
#define ADMIN_PIN_SALT_BYTES 16
#define ADMIN_PIN_HASH_BYTES 32
static const char *TAG = "storage";

static void bytes_to_hex(const uint8_t *bytes, size_t length, char *hex)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < length; i++) {
        hex[i * 2] = digits[bytes[i] >> 4];
        hex[i * 2 + 1] = digits[bytes[i] & 0x0f];
    }
    hex[length * 2] = '\0';
}

static int hex_to_bytes(const char *hex, uint8_t *bytes, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        unsigned value;
        if (sscanf(hex + i * 2, "%2x", &value) != 1) return -1;
        bytes[i] = (uint8_t)value;
    }
    return hex[length * 2] == '\0' ? 0 : -1;
}

static bool derive_admin_pin_hash(const char *pin, const uint8_t *salt,
                                  size_t salt_length, uint32_t iterations,
                                  uint8_t *hash, size_t hash_length)
{
    if (!pin || !salt || !hash || salt_length > ADMIN_PIN_SALT_BYTES ||
        hash_length != ADMIN_PIN_HASH_BYTES || iterations == 0)
        return false;

    psa_status_t status = psa_crypto_init();
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key = 0;
    size_t pin_length = strlen(pin);
    uint8_t first_input[ADMIN_PIN_SALT_BYTES + 4];
    uint8_t u[ADMIN_PIN_HASH_BYTES] = { 0 };
    uint8_t next_u[ADMIN_PIN_HASH_BYTES] = { 0 };
    size_t output_length = 0;
    bool ok = false;

    if (status != PSA_SUCCESS) goto cleanup;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attributes, pin_length * 8);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    status = psa_import_key(&attributes, (const uint8_t *)pin, pin_length, &key);
    if (status != PSA_SUCCESS) goto cleanup;

    memcpy(first_input, salt, salt_length);
    first_input[salt_length] = 0;
    first_input[salt_length + 1] = 0;
    first_input[salt_length + 2] = 0;
    first_input[salt_length + 3] = 1;
    status = psa_mac_compute(key, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                             first_input, salt_length + 4,
                             u, sizeof(u), &output_length);
    if (status != PSA_SUCCESS || output_length != sizeof(u)) goto cleanup;
    memcpy(hash, u, sizeof(u));

    for (uint32_t iteration = 1; iteration < iterations; iteration++) {
        status = psa_mac_compute(key, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                                 u, sizeof(u), next_u, sizeof(next_u),
                                 &output_length);
        if (status != PSA_SUCCESS || output_length != sizeof(next_u)) goto cleanup;
        for (size_t i = 0; i < sizeof(u); i++) {
            hash[i] ^= next_u[i];
        }
        memcpy(u, next_u, sizeof(u));

        // Keep the console and idle task responsive during the PBKDF2 loop.
        if ((iteration % 64) == 0) vTaskDelay(1);
    }
    ok = true;

cleanup:
    if (key != 0) psa_destroy_key(key);
    memset(first_input, 0, sizeof(first_input));
    memset(u, 0, sizeof(u));
    memset(next_u, 0, sizeof(next_u));
    if (!ok) memset(hash, 0, hash_length);
    return ok;
}

bool storage_valid_admin_id(const char *admin_id)
{
    if (!admin_id) return false;
    size_t length = strlen(admin_id);
    if (length == 0 || length > 31) return false;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)admin_id[i];
        if (!isalnum(c) && c != '_' && c != '-') return false;
    }
    return true;
}

bool storage_save_admin_id(const char *admin_id)
{
    if (!storage_is_ready() || !storage_valid_admin_id(admin_id)) return false;
    storage_lock_take();
    FILE *file = fopen(STORAGE_ADMIN_ID_FILE, "w");
    bool ok = file && fprintf(file, "%s\n", admin_id) > 0;
    if (file && fclose(file) != 0) ok = false;
    storage_lock_give();
    return ok;
}

bool storage_load_admin_id(char *admin_id, size_t capacity)
{
    if (!storage_is_ready() || !admin_id || capacity == 0) return false;
    storage_lock_take();
    FILE *file = fopen(STORAGE_ADMIN_ID_FILE, "r");
    bool ok = file && fgets(admin_id, (int)capacity, file) != NULL;
    if (file) fclose(file);
    storage_lock_give();
    if (!ok) return false;
    admin_id[strcspn(admin_id, "\r\n")] = '\0';
    return storage_valid_admin_id(admin_id) && strlen(admin_id) < capacity;
}

bool storage_save_admin_pin(const char *pin)
{
    if (!storage_is_ready() || !pin || strlen(pin) < 4 || strlen(pin) > 12) return false;
    for (const char *p = pin; *p; p++) if (*p < '0' || *p > '9') return false;

    uint8_t salt[ADMIN_PIN_SALT_BYTES], hash[ADMIN_PIN_HASH_BYTES];
    char salt_hex[ADMIN_PIN_SALT_BYTES * 2 + 1];
    char hash_hex[ADMIN_PIN_HASH_BYTES * 2 + 1];
    esp_fill_random(salt, sizeof(salt));
    if (!derive_admin_pin_hash(pin, salt, sizeof(salt), ADMIN_PIN_ITERATIONS,
                               hash, sizeof(hash)))
        return false;
    bytes_to_hex(salt, sizeof(salt), salt_hex);
    bytes_to_hex(hash, sizeof(hash), hash_hex);

    storage_lock_take();
    FILE *file = fopen(STORAGE_PIN_FILE, "w");
    bool ok = file && fprintf(file, "pbkdf2-sha256$%u$%s$%s\n",
                              ADMIN_PIN_ITERATIONS, salt_hex, hash_hex) > 0;
    if (file && fclose(file) != 0) ok = false;
    storage_lock_give();
    memset(hash, 0, sizeof(hash));
    return ok;
}

bool storage_admin_pin_is_configured(void)
{
    if (!storage_is_ready()) return false;
    storage_lock_take();
    FILE *file = fopen(STORAGE_PIN_FILE, "r");
    bool configured = file != NULL;
    if (file) fclose(file);
    storage_lock_give();
    return configured;
}

bool storage_verify_admin_pin(const char *pin)
{
    if (!storage_is_ready() || !pin || strlen(pin) < 4 || strlen(pin) > 12) return false;
    for (const char *p = pin; *p; p++) if (*p < '0' || *p > '9') return false;

    char record[128];
    storage_lock_take();
    FILE *file = fopen(STORAGE_PIN_FILE, "r");
    bool read_ok = file && fgets(record, sizeof(record), file) != NULL;
    if (file) fclose(file);
    storage_lock_give();
    if (!read_ok) return false;
    record[strcspn(record, "\r\n")] = '\0';

    if (strncmp(record, "pbkdf2-sha256$", 14) != 0) {
        // Upgrade PIN files created by older firmware after a successful login.
        if (strcmp(record, pin) != 0) return false;
        if (!storage_save_admin_pin(pin))
            ESP_LOGW(TAG, "legacy PIN verified but hash migration could not be saved");
        return true;
    }

    unsigned iterations;
    char salt_hex[ADMIN_PIN_SALT_BYTES * 2 + 1];
    char hash_hex[ADMIN_PIN_HASH_BYTES * 2 + 1];
    char extra;
    if (sscanf(record + 14, "%u$%32[0-9a-fA-F]$%64[0-9a-fA-F]%c",
               &iterations, salt_hex, hash_hex, &extra) != 3 ||
        iterations < 1000 || iterations > 12000 ||
        strlen(salt_hex) != ADMIN_PIN_SALT_BYTES * 2 ||
        strlen(hash_hex) != ADMIN_PIN_HASH_BYTES * 2)
        return false;

    uint8_t salt[ADMIN_PIN_SALT_BYTES], expected[ADMIN_PIN_HASH_BYTES];
    uint8_t actual[ADMIN_PIN_HASH_BYTES];
    if (hex_to_bytes(salt_hex, salt, sizeof(salt)) != 0 ||
        hex_to_bytes(hash_hex, expected, sizeof(expected)) != 0 ||
        !derive_admin_pin_hash(pin, salt, sizeof(salt), iterations,
                               actual, sizeof(actual)))
        return false;

    uint8_t difference = 0;
    for (size_t i = 0; i < sizeof(actual); i++) difference |= actual[i] ^ expected[i];
    memset(actual, 0, sizeof(actual));
    memset(expected, 0, sizeof(expected));
    bool matches = difference == 0;
    if (matches && iterations != ADMIN_PIN_ITERATIONS && !storage_save_admin_pin(pin))
        ESP_LOGW(TAG, "verified PIN but could not update its hash parameters");
    return matches;
}
