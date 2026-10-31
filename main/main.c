#include "rfid_reader.h"
#include "room_control.h"
#include "integration.h"

void app_main(void)
{
    integration_init();
    rfid_reader_init();
    room_control_init();
}