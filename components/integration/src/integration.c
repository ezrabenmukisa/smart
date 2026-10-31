#include <string.h>
#include "integration.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"

#define ACCESS_LOG_SIZE 16

static const char *TAG = "INTEGRATION";

static QueueHandle_t s_queue;
static SemaphoreHandle_t s_lock;          // protects s_latest and the light-time data
static room_status_t s_latest[3];

static bool     s_light_is_on[3];
static uint32_t s_light_since_ms[3];
static uint32_t s_light_on_ms[3];

static SemaphoreHandle_t s_access_lock;   // protects the access log and occupancy
static access_event_t s_access_log[ACCESS_LOG_SIZE];
static uint32_t s_access_seq;
static int s_occupancy;

static uint32_t now_ms(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

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
    copy.timestamp_ms = now_ms();

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
    e.timestamp_ms = now_ms();
    if (uid && uid_len > 0) {
        e.uid_len = (uid_len > sizeof(e.uid)) ? sizeof(e.uid) : uid_len;
        memcpy(e.uid, uid, e.uid_len);
    }

    xSemaphoreTake(s_access_lock, portMAX_DELAY);
    e.seq = ++s_access_seq;
    s_access_log[(e.seq - 1) % ACCESS_LOG_SIZE] = e;
    if (result == ACCESS_GRANTED_ENTRY) {
        s_occupancy++;
    } else if (result == ACCESS_GRANTED_EXIT && s_occupancy > 0) {
        s_occupancy--;
    }
    int occ = s_occupancy;
    xSemaphoreGive(s_access_lock);

    ESP_LOGI(TAG, "Event #%lu | Building occupancy: %d", (unsigned long)e.seq, occ);
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

int integration_get_occupancy(void)
{
    if (!s_access_lock) return 0;
    xSemaphoreTake(s_access_lock, portMAX_DELAY);
    int occ = s_occupancy;
    xSemaphoreGive(s_access_lock);
    return occ;
}

uint32_t integration_get_light_on_ms(room_id_t room)
{
    if (!valid_room(room)) return 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    uint32_t total = s_light_on_ms[room];
    if (s_light_is_on[room]) {
        total += now_ms() - s_light_since_ms[room];   // light still on: include time so far
    }
    xSemaphoreGive(s_lock);
    return total;
}

static void status_task(void *arg)
{
    room_status_t s;
    for (;;) {
        if (!integration_receive_room_status(&s, 1000)) continue;

        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (s.light_on && !s_light_is_on[s.room]) {
            s_light_is_on[s.room] = true;
            s_light_since_ms[s.room] = s.timestamp_ms;
        } else if (!s.light_on && s_light_is_on[s.room]) {
            s_light_is_on[s.room] = false;
            s_light_on_ms[s.room] += s.timestamp_ms - s_light_since_ms[s.room];
        }
        xSemaphoreGive(s_lock);

        ESP_LOGI(TAG, "Room %d | occupied=%s | light=%s", (int)s.room,
                 s.occupied ? "YES" : "NO", s.light_on ? "ON" : "OFF");
    }
}

void integration_start(void)
{
    xTaskCreate(status_task, "int_status", 3072, NULL, 4, NULL);
}