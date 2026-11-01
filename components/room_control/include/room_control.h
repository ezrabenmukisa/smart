#ifndef ROOM_CONTROL_H
#define ROOM_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

void room_control_init(void);
bool room_control_get_light_usage_ms(uint64_t *room1_ms, uint64_t *room2_ms);

#endif
