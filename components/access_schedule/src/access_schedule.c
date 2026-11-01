#include "access_schedule.h"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_lock;
static access_schedule_t s_schedule = {
    .start_minute = 0,
    .end_minute = 1439,
    .configured = false,
};
static uint16_t s_clock_minute;
static int64_t s_clock_anchor_us;

void access_schedule_init(void)
{
    if (s_lock) return;
    s_lock = xSemaphoreCreateMutex();
    s_clock_anchor_us = esp_timer_get_time();
}

bool access_schedule_set(uint16_t start_minute, uint16_t end_minute)
{
    if (!s_lock || start_minute >= 1440 || end_minute >= 1440 || start_minute >= end_minute)
        return false;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_schedule.start_minute = start_minute;
    s_schedule.end_minute = end_minute;
    s_schedule.configured = true;
    xSemaphoreGive(s_lock);
    return true;
}

bool access_schedule_get(access_schedule_t *out)
{
    if (!out || !s_lock) return false;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_schedule;
    xSemaphoreGive(s_lock);
    return true;
}

uint16_t access_schedule_get_clock_minute(void)
{
    if (!s_lock) return 0;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    int64_t elapsed_minutes = (esp_timer_get_time() - s_clock_anchor_us) / 60000000LL;
    uint16_t minute = (uint16_t)((s_clock_minute + elapsed_minutes) % 1440);
    xSemaphoreGive(s_lock);
    return minute;
}

bool access_schedule_is_allowed_now(void)
{
    access_schedule_t schedule;
    if (!access_schedule_get(&schedule)) return false;
    if (!schedule.configured) return true;

    uint16_t now = access_schedule_get_clock_minute();
    return now >= schedule.start_minute && now < schedule.end_minute;
}

bool access_schedule_set_clock(uint16_t minute_of_day)
{
    if (!s_lock || minute_of_day >= 1440) return false;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_clock_minute = minute_of_day;
    s_clock_anchor_us = esp_timer_get_time();
    xSemaphoreGive(s_lock);
    return true;
}
