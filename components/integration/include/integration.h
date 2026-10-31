#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { ROOM_1 = 1, ROOM_2 = 2 } room_id_t;

typedef struct {
    room_id_t room;        
    bool occupied;         
    bool light_on;         
    uint32_t timestamp_ms; 
} room_status_t;

void integration_init(void);                                         
bool integration_publish_room_status(const room_status_t *s);       
bool integration_receive_room_status(room_status_t *out, uint32_t wait_ms); 
bool integration_get_latest(room_id_t room, room_status_t *out);     