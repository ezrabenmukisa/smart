#include "management.h"
#include "rfid_reader.h"
#include "access_control.h"
#include "room_control.h"
#include "integration.h"
#include "access_schedule.h"
#include "storage.h"

void app_main(void)
{
    integration_init();
    access_schedule_init();
    if (storage_init()) {
        uint16_t start_minute, end_minute;
        char scheduled_by[32];
        if (storage_load_schedule(&start_minute, &end_minute,
                                 scheduled_by, sizeof(scheduled_by))) {
            access_schedule_set(start_minute, end_minute);
        }
    }
    access_control_init();
    rfid_reader_init();
    room_control_init();
    management_init();
}
