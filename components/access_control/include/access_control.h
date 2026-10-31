#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DOOR_ENTRY = 0,
    DOOR_EXIT  = 1,
} door_id_t;

// Sets up the servos and LEDs. Call once at startup.
void access_control_init(void);

// Checks the card. If allowed: opens that door's servo, green LED on, auto-close.
// If not: red LED flashes, nothing moves. Returns true if access was granted.
bool access_handle_scan(door_id_t door, const uint8_t *uid, uint8_t len);

// Only the check, with no servo or LED action.
bool access_is_authorized(const uint8_t *uid, uint8_t len);