#include <stdio.h>
#include "room_control.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include <stdbool.h>
#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "integration.h"
#include "esp_timer.h"
#include "storage.h"

#define ROOM1_SENSOR GPIO_NUM_15
#define ROOM1_LED GPIO_NUM_1

#define ROOM2_SENSOR GPIO_NUM_16
#define ROOM2_LED GPIO_NUM_2

TimerHandle_t room1_timer;
TimerHandle_t room2_timer;
QueueHandle_t room_queue;
QueueHandle_t usage_queue;
SemaphoreHandle_t room_mutex;

typedef struct {
    room_id_t room;
    uint64_t elapsed_ms;
} light_usage_event_t;

typedef struct{
    uint32_t id;
    gpio_num_t sensor;
    gpio_num_t led;
    bool occupied;
    bool light_on;
    int64_t light_on_since_us;
    uint64_t light_usage_us;
}room_t;

static void report_room(room_t *room)
   {
       room_status_t s = { .room = (room_id_t)room->id };
       if (xSemaphoreTake(room_mutex, portMAX_DELAY) == pdTRUE) {
           s.occupied = room->occupied;
           s.light_on = room->light_on;
           xSemaphoreGive(room_mutex);
       }
       integration_publish_room_status(&s);
   }

room_t room1={
    .id=1,
    .sensor=ROOM1_SENSOR,
    .led=ROOM1_LED,
    .occupied=false,
    .light_on=false,
};

room_t room2={
    .id=2,
    .sensor=ROOM2_SENSOR,
    .led=ROOM2_LED,
    .occupied=false,
    .light_on=false,
};
 


TaskHandle_t light_task_handle;


static void IRAM_ATTR sensor_isr_handler(void *arg){

     room_t *room = (room_t *)arg;
     uint32_t room_id = room->id;
    BaseType_t higher_priority_task_woken=pdFALSE;

  xQueueSendFromISR(
        room_queue,
        &room_id,
        &higher_priority_task_woken
    );

    if(higher_priority_task_woken){
        portYIELD_FROM_ISR();
    }
}

void timer_callback(TimerHandle_t timer){
    room_t *room = (room_t *)pvTimerGetTimerID(timer);
    uint64_t elapsed_ms = 0;
     if (xSemaphoreTake(room_mutex, portMAX_DELAY) == pdTRUE) {
        if (room->light_on) {
            uint64_t elapsed_us = (uint64_t)(esp_timer_get_time() - room->light_on_since_us);
            room->light_usage_us += elapsed_us;
            elapsed_ms = elapsed_us / 1000;
        }
        room->occupied = false;
        room->light_on = false;

        xSemaphoreGive(room_mutex);
    }

    if (elapsed_ms > 0 && usage_queue) {
        light_usage_event_t event = {
            .room = (room_id_t)room->id,
            .elapsed_ms = elapsed_ms,
        };
        xQueueSend(usage_queue, &event, 0);
    }

    gpio_set_level(room->led, 0);
    report_room(room);
    ESP_LOGI("ROOM", "Room %lu | Occupied: NO | Light: OFF",
             (unsigned long)room->id);
}

static void light_usage_storage_task(void *arg)
{
    (void)arg;
    light_usage_event_t event;
    while (1) {
        if (xQueueReceive(usage_queue, &event, portMAX_DELAY) == pdPASS && storage_is_ready()) {
            if (!storage_add_light_usage_ms(event.room, event.elapsed_ms))
                ESP_LOGW("ROOM", "Could not save light usage to SD card");
            storage_append_system_event("LIGHT", event.room == ROOM_1 ? "ROOM1 OFF" : "ROOM2 OFF");
        }
    }
}

void light_task(void *pvParameters){

     uint32_t room_id;
    while(1){
     if (xQueueReceive(room_queue, &room_id, portMAX_DELAY) == pdPASS) {
        if (room_id == 1) {
            bool light_started = false;
             if (xSemaphoreTake(room_mutex, portMAX_DELAY) == pdTRUE) {
                    room1.occupied = true;
                    if (!room1.light_on) {
                        room1.light_on_since_us = esp_timer_get_time();
                        light_started = true;
                    }
                    room1.light_on = true;

                    xSemaphoreGive(room_mutex);
                }
            if (light_started && storage_is_ready())
                storage_append_system_event("LIGHT", "ROOM1 ON");
            gpio_set_level(ROOM1_LED, 1);
            xTimerReset(room1_timer, 0);
              report_room(&room1);
            ESP_LOGI("ROOM 1", "Occupied: %s | Light: ON",
             room1.occupied ? "YES" : "NO");
        }

        else if (room_id == 2) {
            bool light_started = false;
             if (xSemaphoreTake(room_mutex, portMAX_DELAY) == pdTRUE) {
                    room2.occupied = true;
                    if (!room2.light_on) {
                        room2.light_on_since_us = esp_timer_get_time();
                        light_started = true;
                    }
                    room2.light_on = true;

                    xSemaphoreGive(room_mutex);
                }

            if (light_started && storage_is_ready())
                storage_append_system_event("LIGHT", "ROOM2 ON");

            gpio_set_level(ROOM2_LED, 1);
            xTimerReset(room2_timer, 0);
              report_room(&room2);
            ESP_LOGI("ROOM 2", "Occupied: %s | Light: ON",
             room2.occupied ? "YES" : "NO");
        }
}}
}

bool room_control_get_light_usage_ms(uint64_t *room1_ms, uint64_t *room2_ms)
{
    if (!room1_ms || !room2_ms || !room_mutex) return false;
    if (xSemaphoreTake(room_mutex, portMAX_DELAY) != pdTRUE) return false;

    uint64_t room1_us = room1.light_usage_us;
    uint64_t room2_us = room2.light_usage_us;
    int64_t now = esp_timer_get_time();
    if (room1.light_on) room1_us += now - room1.light_on_since_us;
    if (room2.light_on) room2_us += now - room2.light_on_since_us;
    *room1_ms = room1_us / 1000;
    *room2_ms = room2_us / 1000;

    xSemaphoreGive(room_mutex);
    return true;
}

void room_control_init(void)
{

    uint64_t room1_ms = 0, room2_ms = 0;
    if (storage_get_light_usage_ms(&room1_ms, &room2_ms)) {
        room1.light_usage_us = room1_ms * 1000;
        room2.light_usage_us = room2_ms * 1000;
    }

    ESP_LOGW("ROOM_CONTROL", "Room automation initialising");

gpio_config_t sensor_conf={
    .pin_bit_mask=(1ULL<<ROOM1_SENSOR)|(1ULL<<ROOM2_SENSOR),
    .mode=GPIO_MODE_INPUT,
    .pull_down_en=GPIO_PULLDOWN_DISABLE,
    .pull_up_en=GPIO_PULLUP_DISABLE,
    .intr_type=GPIO_INTR_POSEDGE
};
if(gpio_config(&sensor_conf)!=ESP_OK){
printf("Sensor configuration failed");
return;
}

gpio_config_t led_conf={
    .pin_bit_mask=(1ULL<<ROOM1_LED)|(1ULL<<ROOM2_LED),
    .mode=GPIO_MODE_OUTPUT,
    .pull_down_en=GPIO_PULLDOWN_DISABLE,
    .pull_up_en=GPIO_PULLUP_DISABLE,
    .intr_type=GPIO_INTR_DISABLE
};
if(gpio_config(&led_conf)!=ESP_OK){
printf("LED configuration failed");
return;
}

gpio_set_level(ROOM1_LED, 0);
gpio_set_level(ROOM2_LED, 0);




room1_timer=xTimerCreate(
    "Timer",
    pdMS_TO_TICKS(20000),
    pdFALSE,
    &room1,
    timer_callback

);
if(room1_timer==NULL){
    printf("Timer creation failed");
    return ;
}

room2_timer=xTimerCreate(
    "Timer",
    pdMS_TO_TICKS(20000),
    pdFALSE,
    &room2,
    timer_callback

);
if(room2_timer==NULL){
    printf("Timer creation failed");
    return ;
}
room_queue = xQueueCreate(10, sizeof(uint32_t));

if (room_queue == NULL) {
    printf("Queue creation failed");
    return;
}

usage_queue = xQueueCreate(4, sizeof(light_usage_event_t));
if (usage_queue == NULL) {
    printf("Light usage storage queue creation failed");
    return;
}

room_mutex = xSemaphoreCreateMutex();

if (room_mutex == NULL) {
    printf("Mutex creation failed");
    return;
}

/* Publish the known startup state so management can report both rooms
 * before the first PIR motion event arrives. */
report_room(&room1);
report_room(&room2);

if(xTaskCreate(light_task,"light_task",2048,NULL,5,&light_task_handle)!=pdPASS){
    printf("Task creation failed");
    return;
}

if(xTaskCreate(light_usage_storage_task,"light_usage_storage",6144,NULL,4,NULL)!=pdPASS){
    printf("Light usage storage task creation failed");
    return;
}



if (gpio_install_isr_service(0)!=ESP_OK){
    printf("ISR service installation failed");
    return;
}

if(gpio_isr_handler_add(room1.sensor,sensor_isr_handler,&room1)!=ESP_OK){
    printf("ISR handler registration failed");
    return;
}
if(gpio_isr_handler_add(room2.sensor,sensor_isr_handler,&room2)!=ESP_OK){
    printf("ISR handler registration failed");
    return;

    ESP_LOGW("ROOM_CONTROL", "Room automation initialised");
}
}
