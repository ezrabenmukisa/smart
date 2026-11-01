#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "storage.h"
#include "storage_internal.h"

bool storage_save_schedule(uint16_t start_minute, uint16_t end_minute, const char *admin_id)
{
    if (!storage_is_ready() || start_minute >= end_minute || end_minute >= 1440 || !storage_valid_admin_id(admin_id))
        return false;
    storage_lock_take();
    FILE *file = fopen(STORAGE_SCHEDULE_FILE, "w");
    bool ok = file && fprintf(file, "%u %u %s\n", start_minute, end_minute, admin_id) > 0;
    if (file && fclose(file) != 0) ok = false;
    storage_lock_give();
    return ok;
}

bool storage_load_schedule(uint16_t *start_minute, uint16_t *end_minute,
                           char *admin_id, size_t admin_id_capacity)
{
    if (!storage_is_ready() || !start_minute || !end_minute || !admin_id || admin_id_capacity == 0) return false;
    storage_lock_take();
    FILE *file = fopen(STORAGE_SCHEDULE_FILE, "r");
    unsigned start, end;
    char stored_admin_id[32] = "UNKNOWN";
    int fields = file ? fscanf(file, "%u %u %31s", &start, &end, stored_admin_id) : 0;
    bool ok = file && fields >= 2 && start < end && end < 1440;
    if (ok && !storage_valid_admin_id(stored_admin_id)) ok = false;
    if (ok && strlen(stored_admin_id) >= admin_id_capacity) ok = false;
    if (file) fclose(file);
    if (ok) {
        *start_minute = (uint16_t)start;
        *end_minute = (uint16_t)end;
        memcpy(admin_id, stored_admin_id, strlen(stored_admin_id) + 1);
    }
    storage_lock_give();
    return ok;
}
