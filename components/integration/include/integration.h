#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { ROOM_1 = 1, ROOM_2 = 2 } room_id_t;

typedef struct {
    room_id_t room;
    bool occupied;
    bool light_on;
    uint32_t timestamp_ms;
} room_status_t;

typedef enum { ACCESS_DOOR_ENTRY = 0, ACCESS_DOOR_EXIT = 1 } access_door_t;   // same values as access_control's door_id_t

typedef enum {
    ACCESS_GRANTED_ENTRY,
    ACCESS_GRANTED_EXIT,
    ACCESS_DENIED_UNKNOWN_CARD,
    ACCESS_DENIED_ALREADY_INSIDE,
    ACCESS_DENIED_NOT_INSIDE,
} access_result_t;

typedef struct {
    uint32_t seq;
    uint32_t timestamp_ms;
    access_door_t door;
    access_result_t result;
    uint8_t uid[4];
    uint8_t uid_len;
} access_event_t;

void integration_init(void);
void integration_start(void);   // NEW: starts the task that consumes room status updates

bool integration_publish_room_status(const room_status_t *s);
bool integration_receive_room_status(room_status_t *out, uint32_t wait_ms);
bool integration_get_latest(room_id_t room, room_status_t *out);

bool integration_post_access_event(access_door_t door, const uint8_t *uid, uint8_t uid_len, access_result_t result);
size_t integration_get_recent_access(access_event_t *out, size_t max);

int      integration_get_occupancy(void);                  // NEW: people currently inside
uint32_t integration_get_light_on_ms(room_id_t room);      // NEW: total light-on time (estimate)                                              