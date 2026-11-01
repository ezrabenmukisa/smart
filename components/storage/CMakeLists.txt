set(PSA_CRYPTO_INCLUDE "$ENV{IDF_PATH}/components/mbedtls/mbedtls/tf-psa-crypto/include")

idf_component_register(
    SRCS "src/storage.c"
         "src/storage_schedule.c"
         "src/storage_admin.c"
         "src/storage_access.c"
         "src/storage_light_usage.c"
    INCLUDE_DIRS "include" "${PSA_CRYPTO_INCLUDE}"
    REQUIRES freertos fatfs sdmmc esp_driver_spi esp_timer integration mbedtls
)
