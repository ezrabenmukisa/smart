#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint16_t start_minute;
    uint16_t end_minute;
    bool configured;
} access_schedule_t;

void access_schedule_init(void);
bool access_schedule_set(uint16_t start_minute, uint16_t end_minute);
bool access_schedule_get(access_schedule_t *out);
bool access_schedule_is_allowed_now(void);
bool access_schedule_set_clock(uint16_t minute_of_day);
uint16_t access_schedule_get_clock_minute(void);
