#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_log.h"
#include "rc522.h"
#include "rc522_picc.h"
#include "driver/rc522_spi.h"
#include "rfid_reader.h"
#include "access_control.h"

#define PIN_SCK   12
#define PIN_MOSI  11
#define PIN_MISO  13

#define ENTRY_CS  5
#define ENTRY_RST 4
#define EXIT_CS   6
#define EXIT_RST  7

static const char *TAG = "rfid_reader";

typedef struct {
    const char *name;
    door_id_t door;
    rc522_driver_handle_t driver;
    rc522_handle_t scanner;
} reader_t;

static reader_t s_entry = { .name = "ENTRY", .door = DOOR_ENTRY };
static reader_t s_exit = { .name = "EXIT", .door = DOOR_EXIT };

static void on_picc_state_changed(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    reader_t *reader = (reader_t *)arg;
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        access_handle_scan(reader->door, picc->uid.value, picc->uid.length);
        rc522_picc_print(picc);
    }
}

static void start_reader(reader_t *reader, int cs, int rst, bool init_bus)
{
    spi_bus_config_t bus = {
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = PIN_MISO,
        .sclk_io_num = PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    rc522_spi_config_t driver_config = {
        .host_id = SPI2_HOST,
        .bus_config = init_bus ? &bus : NULL,
        .dev_config = {
            .spics_io_num = cs,
            .clock_speed_hz = 500000,
            .mode = 0,
            .queue_size = 3,
        },
        .rst_io_num = rst,
    };

    ESP_ERROR_CHECK(rc522_spi_create(&driver_config, &reader->driver));
    ESP_ERROR_CHECK(rc522_driver_install(reader->driver));

    rc522_config_t scanner_config = { .driver = reader->driver };
    ESP_ERROR_CHECK(rc522_create(&scanner_config, &reader->scanner));
    ESP_ERROR_CHECK(rc522_register_events(reader->scanner, RC522_EVENT_PICC_STATE_CHANGED,
                                          on_picc_state_changed, reader));
    ESP_ERROR_CHECK(rc522_start(reader->scanner));

    ESP_LOGI(TAG, "[%s] reader ready", reader->name);
}

void rfid_reader_init(void)
{
    start_reader(&s_entry, ENTRY_CS, ENTRY_RST, true);   // first reader creates the SPI bus
    start_reader(&s_exit,  EXIT_CS,  EXIT_RST,  false);  // second reader shares it
}