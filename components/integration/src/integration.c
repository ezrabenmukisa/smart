#include <string.h>                                          
#include "integration.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define ACCESS_LOG_SIZE 16                                   

static QueueHandle_t s_queue;
static SemaphoreHandle_t s_lock;
static room_status_t s_latest[3];   

static SemaphoreHandle_t s_access_lock;                      
static access_event_t s_access_log[ACCESS_LOG_SIZE];         
static uint32_t s_access_seq;                               

static bool valid_room(room_id_t room)
{
    return s_lock && s_queue && room >= ROOM_1 && room <= ROOM_2;
}

void integration_init(void) {
    s_queue = xQueueCreate(10, sizeof(room_status_t));
    s_lock  = xSemaphoreCreateMutex();
    s_access_lock = xSemaphoreCreateMutex();                
}


bool integration_publish_room_status(const room_status_t *s) {
    if (!s || !valid_room(s->room)) return false;

    room_status_t copy = *s;
    copy.timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_latest[copy.room] = copy;
    xSemaphoreGive(s_lock);
    return xQueueSend(s_queue, &copy, 0) == pdTRUE;
}

bool integration_receive_room_status(room_status_t *out, uint32_t wait_ms) {
    if (!out || !s_queue) return false;
    return xQueueReceive(s_queue, out, pdMS_TO_TICKS(wait_ms)) == pdTRUE;
}

/* false = invalid room, or that room has not published yet */
bool integration_get_latest(room_id_t room, room_status_t *out) {
    if (!out || !valid_room(room)) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_latest[room];
    xSemaphoreGive(s_lock);
    return out->room != 0;
}

bool integration_post_access_event(access_door_t door, const uint8_t *uid, uint8_t uid_len, access_result_t result)
{
    if (!s_access_lock || door < ACCESS_DOOR_ENTRY || door > ACCESS_DOOR_EXIT) return false;

    access_event_t e = { .door = door, .result = result };
    e.timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (uid && uid_len > 0) {
        e.uid_len = (uid_len > sizeof(e.uid)) ? sizeof(e.uid) : uid_len;
        memcpy(e.uid, uid, e.uid_len);
    }

    xSemaphoreTake(s_access_lock, portMAX_DELAY);
    e.seq = ++s_access_seq;
    s_access_log[(e.seq - 1) % ACCESS_LOG_SIZE] = e;         
    xSemaphoreGive(s_access_lock);
    return true;
}


size_t integration_get_recent_access(access_event_t *out, size_t max)
{
    if (!out || max == 0 || !s_access_lock) return 0;

    xSemaphoreTake(s_access_lock, portMAX_DELAY);
    size_t stored = (s_access_seq < ACCESS_LOG_SIZE) ? s_access_seq : ACCESS_LOG_SIZE;
    size_t n = (stored < max) ? stored : max;
    for (size_t i = 0; i < n; i++) {
        out[i] = s_access_log[(s_access_seq - 1 - i) % ACCESS_LOG_SIZE];
    }
    xSemaphoreGive(s_access_lock);
    return n;
}