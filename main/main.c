#include "rfid_reader.h"
#include "access_control.h"
#include "room_control.h"
#include "integration.h"

void app_main(void)
{
    integration_init();
    integration_start();      // NEW
    access_control_init();
    rfid_reader_init();
    room_control_init();
}