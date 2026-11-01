#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "storage.h"
#include "storage_internal.h"

#include <stdint.h>
#include "esp_timer.h"

bool storage_append_access_event(const access_event_t *event)
{
    if (!storage_is_ready() || !event || event->time_minute >= 1440) return false;
    storage_lock_take();
    FILE *file = fopen(STORAGE_ACCESS_FILE, "a");
    bool ok = file != NULL;
    if (ok) {
        if (fseek(file, 0, SEEK_END) != 0) ok = false;
        else if (ftell(file) == 0)
            ok = fprintf(file, "seq,uptime_ms,simulated_time,door,result,uid\n") > 0;
    }
    const char *door = event->door == ACCESS_DOOR_ENTRY ? "ENTRY" : "EXIT";
    const char *result = "UNKNOWN";
    switch (event->result) {
    case ACCESS_GRANTED_ENTRY: result = "GRANTED_ENTRY"; break;
    case ACCESS_GRANTED_EXIT: result = "GRANTED_EXIT"; break;
    case ACCESS_DENIED_UNKNOWN_CARD: result = "DENIED_UNKNOWN_CARD"; break;
    case ACCESS_DENIED_ALREADY_INSIDE: result = "DENIED_ALREADY_INSIDE"; break;
    case ACCESS_DENIED_NOT_INSIDE: result = "DENIED_NOT_INSIDE"; break;
    case ACCESS_DENIED_OUTSIDE_SCHEDULE: result = "DENIED_OUTSIDE_SCHEDULE"; break;
    default: break;
    }
    char time_text[9];
    snprintf(time_text, sizeof(time_text), "%02u:%02u",
             (unsigned)(event->time_minute / 60),
             (unsigned)(event->time_minute % 60));
    if (ok) ok = fprintf(file, "%lu,%lu,%s,%s,%s,",
                              (unsigned long)event->seq,
                              (unsigned long)event->timestamp_ms,
                              time_text, door, result) >= 0;
    if (ok) {
        for (size_t i = 0; i < event->uid_len; i++)
            if (fprintf(file, "%02X", event->uid[i]) < 0) ok = false;
        if (fputc('\n', file) == EOF) ok = false;
    }
    if (file && fclose(file) != 0) ok = false;
    storage_lock_give();
    return ok;
}

bool storage_append_system_event(const char *type, const char *details)
{
    if (!storage_is_ready() || !type || !details) return false;
    storage_lock_take();
    FILE *file = fopen(STORAGE_SYSTEM_FILE, "a");
    bool ok = file != NULL;
    if (ok) {
        if (fseek(file, 0, SEEK_END) != 0) ok = false;
        else if (ftell(file) == 0) ok = fprintf(file, "uptime_ms,type,details\n") > 0;
    }
    if (ok) ok = fprintf(file, "%lu,%s,%s\n",
                              (unsigned long)(esp_timer_get_time() / 1000),
                              type, details) >= 0;
    if (file && fclose(file) != 0) ok = false;
    storage_lock_give();
    return ok;
}

static bool visit_records(const char *path, storage_record_callback_t callback, void *context)
{
    if (!storage_is_ready() || !callback) return false;
    storage_lock_take();
    FILE *file = fopen(path, "r");
    if (!file) {
        storage_lock_give();
        return true; // Empty history is valid.
    }
    char line[192];
    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\r\n")] = '\0';
        callback(line, context);
    }
    bool ok = !ferror(file);
    fclose(file);
    storage_lock_give();
    return ok;
}

bool storage_visit_access_records(storage_record_callback_t callback, void *context)
{
    return visit_records(STORAGE_ACCESS_FILE, callback, context);
}

bool storage_visit_system_records(storage_record_callback_t callback, void *context)
{
    return visit_records(STORAGE_SYSTEM_FILE, callback, context);
}
