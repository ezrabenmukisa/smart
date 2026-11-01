#include <stdio.h>
#include <string.h>

#include "access_schedule.h"
#include "integration.h"
#include "management_reports.h"
#include "room_control.h"
#include "storage.h"

static const char *access_result_name(access_result_t result)
{
    switch (result) {
    case ACCESS_GRANTED_ENTRY: return "ENTRY GRANTED";
    case ACCESS_GRANTED_EXIT: return "EXIT GRANTED";
    case ACCESS_DENIED_UNKNOWN_CARD: return "DENIED: UNKNOWN CARD";
    case ACCESS_DENIED_ALREADY_INSIDE: return "DENIED: ALREADY INSIDE";
    case ACCESS_DENIED_NOT_INSIDE: return "DENIED: NOT INSIDE";
    case ACCESS_DENIED_OUTSIDE_SCHEDULE: return "DENIED: OUTSIDE SCHEDULE";
    default: return "UNKNOWN EVENT";
    }
}

static const char *door_name(access_door_t door)
{
    return door == ACCESS_DOOR_ENTRY ? "ENTRY" : "EXIT";
}

static void print_report_title(const char *title)
{
    printf("\n-------------------- %s --------------------\n", title);
}

static void print_time(uint16_t minute)
{
    printf("%02u:%02u", (unsigned)(minute / 60), (unsigned)(minute % 60));
}

typedef struct {
    const char *source;
    size_t count;
} history_context_t;

static void print_persisted_record(const char *record, void *context)
{
    history_context_t *history = context;
    if ((strcmp(history->source, "ACCESS") == 0 &&
         (strcmp(record, "seq,uptime_ms,door,result,uid") == 0 ||
          strcmp(record, "seq,uptime_ms,simulated_time,door,result,uid") == 0)) ||
        (strcmp(history->source, "SYSTEM") == 0 &&
         strcmp(record, "uptime_ms,type,details") == 0))
        return;

    if (strcmp(history->source, "ACCESS") == 0) {
        unsigned long sequence, uptime_ms;
        char time_text[6], door[16], result[32], uid[64];
        if (sscanf(record, "%lu,%lu,%5[^,],%15[^,],%31[^,],%63s",
                   &sequence, &uptime_ms, time_text, door, result, uid) == 6) {
            printf("  #%lu | Time %s simulated | %s | %s | UID %s\n",
                   sequence, time_text, door, result, uid);
            history->count++;
            return;
        }
        if (sscanf(record, "%lu,%lu,%15[^,],%31[^,],%63s",
                   &sequence, &uptime_ms, door, result, uid) == 5) {
            printf("  #%lu | Time unavailable (older record) | %lu ms | %s | %s | UID %s\n",
                   sequence, uptime_ms, door, result, uid);
            history->count++;
            return;
        }
    } else {
        unsigned long uptime_ms;
        char type[32], details[128];
        if (sscanf(record, "%lu,%31[^,],%127[^\n]", &uptime_ms, type, details) == 3) {
            printf("  %lu ms | %-10s | %s\n", uptime_ms, type, details);
            history->count++;
            return;
        }
    }

    printf("  %s\n", record);
    history->count++;
}

void management_report_show_occupancy(void)
{
    building_status_t status;
    print_report_title("BUILDING OCCUPANCY REPORT");
    if (integration_get_building_status(&status))
        printf("People inside building : %lu\n", (unsigned long)status.people_inside);
    else
        printf("Status                 : unavailable\n");
}

void management_report_show_room_status(void)
{
    const room_id_t rooms[] = { ROOM_1, ROOM_2 };
    print_report_title("ROOM STATUS REPORT");
    for (size_t i = 0; i < sizeof(rooms) / sizeof(rooms[0]); i++) {
        room_status_t status;
        if (!integration_get_latest(rooms[i], &status)) {
            printf("Room %u                 : waiting for first update\n", (unsigned)rooms[i]);
            continue;
        }
        printf("Room %u                 : Occupancy %-8s | Light %s\n",
               (unsigned)status.room,
               status.occupied ? "OCCUPIED" : "EMPTY",
               status.light_on ? "ON" : "OFF");
    }
}

void management_report_show_recent_access(void)
{
    access_event_t events[8];
    size_t count = integration_get_recent_access(events, 8);
    print_report_title("RECENT ACCESS REPORT");
    if (count == 0) {
        printf("Status                 : no access attempts recorded\n");
        return;
    }
    printf("Records                : %u (newest first)\n", (unsigned)count);
    for (size_t i = 0; i < count; i++) {
        printf("%2u. Event #%lu | Time %02u:%02u simulated | Uptime %lu ms | Door %-5s | Result %-24s | UID:",
               (unsigned)(i + 1),
               (unsigned long)events[i].seq,
               (unsigned)(events[i].time_minute / 60),
               (unsigned)(events[i].time_minute % 60),
               (unsigned long)events[i].timestamp_ms,
               door_name(events[i].door),
               access_result_name(events[i].result));
        for (size_t j = 0; j < events[i].uid_len; j++)
            printf(" %02X", events[i].uid[j]);
        printf("\n");
    }
}

void management_report_show_light_usage(void)
{
    uint64_t room1_ms, room2_ms;
    print_report_title("ESTIMATED LIGHT USAGE REPORT");
    if (!room_control_get_light_usage_ms(&room1_ms, &room2_ms)) {
        printf("Status                 : unavailable\n");
        return;
    }
    printf("Measurement            : estimated light-on duration\n");
    printf("Room 1                 : %llu seconds\n", (unsigned long long)(room1_ms / 1000));
    printf("Room 2                 : %llu seconds\n", (unsigned long long)(room2_ms / 1000));
}

void management_report_show_event_history(void)
{
    print_report_title("PERSISTED EVENT HISTORY");
    if (!storage_is_ready()) {
        printf("Storage                : unavailable; history cannot be read\n");
        return;
    }
    history_context_t access = { .source = "ACCESS", .count = 0 };
    history_context_t system = { .source = "SYSTEM", .count = 0 };
    printf("Storage                : SD card\n");
    printf("Access records:\n");
    bool access_ok = storage_visit_access_records(print_persisted_record, &access);
    printf("System records:\n");
    bool system_ok = storage_visit_system_records(print_persisted_record, &system);
    if (!access_ok || !system_ok) printf("Warning                : one or more history files could not be read\n");
    if (access.count == 0 && system.count == 0) printf("Status                 : no persisted events recorded\n");
}

void management_report_show_schedule(void)
{
    access_schedule_t schedule;
    print_report_title("ACCESS SCHEDULE STATUS");
    printf("Simulated current time : ");
    print_time(access_schedule_get_clock_minute());
    printf("\nSchedule status       : ");
    if (!access_schedule_get(&schedule)) {
        printf("unavailable\n");
        return;
    }
    if (!schedule.configured) {
        printf("not configured; access is unrestricted\n");
        return;
    }
    printf("configured\n");
    char scheduled_by[32];
    uint16_t saved_start, saved_end;
    if (storage_load_schedule(&saved_start, &saved_end, scheduled_by, sizeof(scheduled_by))) {
        if (saved_start == schedule.start_minute && saved_end == schedule.end_minute) {
            printf("Access window          : ");
            print_time(saved_start); printf(" - "); print_time(saved_end);
            printf("\nScheduled by           : %s\n", scheduled_by);
        } else {
            printf("\nWarning                : active window differs from saved window\nLast saved by           : %s\n", scheduled_by);
        }
    } else {
        printf("Persistence            : active in memory; no saved administrator record\n");
    }
}
