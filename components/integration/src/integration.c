#include "integration.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static QueueHandle_t s_queue;
static SemaphoreHandle_t s_lock;
static room_status_t s_latest[3];   

static bool valid_room(room_id_t room)
{
    return s_lock && s_queue && room >= ROOM_1 && room <= ROOM_2;
}

void integration_init(void) {
    s_queue = xQueueCreate(10, sizeof(room_status_t));
    s_lock  = xSemaphoreCreateMutex();
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


bool integration_get_latest(room_id_t room, room_status_t *out) {
    if (!out || !valid_room(room)) return false;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_latest[room];
    xSemaphoreGive(s_lock);
    return out->room != 0;
}