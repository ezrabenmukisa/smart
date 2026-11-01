#include <string.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "access_control.h"
#include "access_schedule.h"
#include "integration.h"
#include "storage.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "esp_log.h"

#define UID_LEN 4

#define SERVO_ENTRY_PIN 14
#define SERVO_EXIT_PIN  19
#define LED_GREEN_PIN   48
#define LED_RED_PIN     10

#define SERVO_CLOSED_US 500     // about 0 degrees
#define SERVO_OPEN_US   1500    // about 90 degrees
#define OPEN_TIME_MS    3000
#define DENY_TIME_MS    1500

static SemaphoreHandle_t s_lock;

static const char *TAG = "access_control";
static const char *s_door_name[2] = { "ENTRY", "EXIT" };

static esp_timer_handle_t s_close_timer[2];
static esp_timer_handle_t s_red_timer;

static const int s_servo_pin[2] = { SERVO_ENTRY_PIN, SERVO_EXIT_PIN };
static const ledc_channel_t s_channel[2] = { LEDC_CHANNEL_0, LEDC_CHANNEL_1 };


static const uint8_t ALLOWED[][UID_LEN] = {
    { 0x01, 0x02, 0x03, 0x04 },   // Blue
    { 0x11, 0x22, 0x33, 0x44 },   // Green
    { 0x55, 0x66, 0x77, 0x88 },   // Yellow
    { 0xAA, 0xBB, 0xCC, 0xDD },   // Red
};

#define NUM_CARDS (sizeof(ALLOWED) / sizeof(ALLOWED[0]))
static bool s_inside[NUM_CARDS];      // all false: everyone starts outside

// Returns the card's position in ALLOWED, or -1 if it isn't on the list.
static int find_card(const uint8_t *uid, uint8_t len)
{
    if (uid == NULL || len != UID_LEN) {
        return -1;
    }
    for (size_t i = 0; i < NUM_CARDS; i++) {
        if (memcmp(uid, ALLOWED[i], UID_LEN) == 0) {
            return (int)i;
        }
    }
    return -1;
}



bool access_is_authorized(const uint8_t *uid, uint8_t len)
{
    return find_card(uid, len) >= 0;
}

// 50 Hz = 20000 us period, 14-bit duty resolution
static void servo_write_us(door_id_t door, uint32_t us)
{
    uint32_t duty = (us * ((1u << 14) - 1)) / 20000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, s_channel[door], duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, s_channel[door]);
}

static void close_cb(void *arg)
{
    door_id_t door = (door_id_t)(intptr_t)arg;
    servo_write_us(door, SERVO_CLOSED_US);
    gpio_set_level(LED_GREEN_PIN, 0);
}

static void red_cb(void *arg)
{
    gpio_set_level(LED_RED_PIN, 0);
}

void access_control_init(void)
{
    s_lock = xSemaphoreCreateMutex();

    gpio_config_t leds = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << LED_GREEN_PIN) | (1ULL << LED_RED_PIN),
    };
    ESP_ERROR_CHECK(gpio_config(&leds));
    gpio_set_level(LED_GREEN_PIN, 0);
    gpio_set_level(LED_RED_PIN, 0);

    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    for (int i = 0; i < 2; i++) {
        ledc_channel_config_t ch = {
            .gpio_num = s_servo_pin[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = s_channel[i],
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ch));
        servo_write_us((door_id_t)i, SERVO_CLOSED_US);

        esp_timer_create_args_t t = {
            .callback = close_cb,
            .arg = (void *)(intptr_t)i,
            .name = "door_close",
        };
        ESP_ERROR_CHECK(esp_timer_create(&t, &s_close_timer[i]));
    }

    esp_timer_create_args_t red = { .callback = red_cb, .name = "red_off" };
    ESP_ERROR_CHECK(esp_timer_create(&red, &s_red_timer));

    ESP_LOGI(TAG, "doors closed, indicators ready");
}

bool access_handle_scan(door_id_t door, const uint8_t *uid, uint8_t len)
{
    int idx = find_card(uid, len);
    bool ok = false;
    bool outside_schedule = idx >= 0 && !access_schedule_is_allowed_now();
    const char *reason = "unknown card";

    if (outside_schedule) {
        reason = "outside access schedule";
    } else if (idx >= 0) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (door == DOOR_ENTRY) {
            if (!s_inside[idx]) {
                s_inside[idx] = true;
                ok = true;
                reason = "entered";
            } else {
                reason = "already inside";
            }
        } else {
            if (s_inside[idx]) {
                s_inside[idx] = false;
                ok = true;
                reason = "exited";
            } else {
                reason = "has not entered";
            }
        }
        xSemaphoreGive(s_lock);
    }

    ESP_LOGI(TAG, "[%s] %s (%s)", s_door_name[door],
             ok ? "ACCESS GRANTED" : "ACCESS DENIED", reason);

    // report the event to the shared integration component
    access_result_t r;
    if (ok)                      r = (door == DOOR_ENTRY) ? ACCESS_GRANTED_ENTRY : ACCESS_GRANTED_EXIT;
    else if (idx < 0)            r = ACCESS_DENIED_UNKNOWN_CARD;
    else if (outside_schedule)   r = ACCESS_DENIED_OUTSIDE_SCHEDULE;
    else if (door == DOOR_ENTRY) r = ACCESS_DENIED_ALREADY_INSIDE;
    else                         r = ACCESS_DENIED_NOT_INSIDE;
    access_event_t event;
    uint16_t scan_time_minute = access_schedule_get_clock_minute();
    if (integration_post_access_event((access_door_t)door, uid, len, r,
                                      scan_time_minute, &event) &&
        storage_is_ready() && !storage_append_access_event(&event)) {
        ESP_LOGW(TAG, "could not save access event to SD card");
    }
    
    if (ok) {
        servo_write_us(door, SERVO_OPEN_US);
        gpio_set_level(LED_GREEN_PIN, 1);
        gpio_set_level(LED_RED_PIN, 0);
        esp_timer_stop(s_close_timer[door]);
        esp_timer_start_once(s_close_timer[door], OPEN_TIME_MS * 1000ULL);
    } else {
        gpio_set_level(LED_RED_PIN, 1);
        esp_timer_stop(s_red_timer);
        esp_timer_start_once(s_red_timer, DENY_TIME_MS * 1000ULL);
    }
    return ok;
}
