#include <stdio.h>

#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdmmc_cmd.h"
#include "storage.h"
#include "storage_internal.h"

#define STORAGE_PIN 8
#define STORAGE_SCK 9
#define STORAGE_MOSI 17
#define STORAGE_MISO 18

static const char *TAG = "storage";
static bool s_ready;
static SemaphoreHandle_t s_lock;

void storage_lock_take(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
}

void storage_lock_give(void)
{
    xSemaphoreGive(s_lock);
}

bool storage_init(void)
{
    if (s_ready) return true;
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return false;

    spi_bus_config_t bus = {
        .mosi_io_num = STORAGE_MOSI,
        .miso_io_num = STORAGE_MISO,
        .sclk_io_num = STORAGE_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    esp_err_t err = spi_bus_initialize(SPI3_HOST, &bus, SDSPI_DEFAULT_DMA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD SPI bus init failed: %s", esp_err_to_name(err));
        return false;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.gpio_cs = STORAGE_PIN;
    slot.host_id = SPI3_HOST;
    esp_vfs_fat_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };
    sdmmc_card_t *card = NULL;
    err = esp_vfs_fat_sdspi_mount(STORAGE_MOUNT_POINT, &host, &slot,
                                  &mount_config, &card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %s", esp_err_to_name(err));
        spi_bus_free(SPI3_HOST);
        return false;
    }

    s_ready = true;
    ESP_LOGI(TAG, "SD card mounted");
    return true;
}

bool storage_is_ready(void)
{
    return s_ready;
}
