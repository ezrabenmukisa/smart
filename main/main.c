#include "rfid_reader.h"
#include "room_control.h"

void app_main(void)
{
    rfid_reader_init();
    room_control_init();
}