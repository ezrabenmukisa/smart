#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "rc522.h"
#include "rc522_picc.h"
#include "driver/rc522_spi.h"

#define PIN_CS    5
#define PIN_RST   4
#define PIN_SCK   12
#define PIN_MOSI  11
#define PIN_MISO  13

static const char *TAG = "entrance";

static rc522_driver_handle_t driver = NULL;
static rc522_handle_t scanner = NULL;

static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "RFID card detected");
        rc522_picc_print(picc);
    } else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "RFID card removed");
    }
}

void app_main(void)
{
    //esp_log_level_set("rc522", ESP_LOG_ERROR);
    ESP_LOGI(TAG, "Starting entrance controller");

    // Explicitly configure using the driver's exact expected parameters
    rc522_spi_config_t driver_config = {
        .host_id = SPI2_HOST,
        .bus_config = &(spi_bus_config_t){
            .mosi_io_num = PIN_MOSI,
            .miso_io_num = PIN_MISO,
            .sclk_io_num = PIN_SCK,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
        },
        .dev_config = {
            .spics_io_num = PIN_CS,
            .clock_speed_hz = 500000,
            .mode = 0,
            .queue_size = 3,
        },
        .rst_io_num = PIN_RST,
    };

    ESP_ERROR_CHECK(rc522_spi_create(&driver_config, &driver));
    ESP_ERROR_CHECK(rc522_driver_install(driver));

    rc522_config_t scanner_config = {
        .driver = driver,
    };

    ESP_ERROR_CHECK(rc522_create(&scanner_config, &scanner));

    ESP_ERROR_CHECK(rc522_register_events(
        scanner,
        RC522_EVENT_PICC_STATE_CHANGED,
        on_picc_state_changed,
        NULL
    ));

    ESP_ERROR_CHECK(rc522_start(scanner));

    ESP_LOGI(TAG, "RC522 ready. Scan a card.");
}