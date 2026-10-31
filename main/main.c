#include "rfid_reader.h"
#include "access_control.h"

void app_main(void)
{
    access_control_init();
    rfid_reader_init();
}