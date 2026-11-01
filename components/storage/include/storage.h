#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "integration.h"

typedef void (*storage_record_callback_t)(const char *record, void *context);

bool storage_init(void);
bool storage_is_ready(void);
bool storage_save_schedule(uint16_t start_minute, uint16_t end_minute, const char *admin_id);
bool storage_load_schedule(uint16_t *start_minute, uint16_t *end_minute,
                           char *admin_id, size_t admin_id_capacity);
bool storage_save_admin_id(const char *admin_id);
bool storage_load_admin_id(char *admin_id, size_t capacity);
bool storage_save_admin_pin(const char *pin);
bool storage_admin_pin_is_configured(void);
bool storage_verify_admin_pin(const char *pin);
bool storage_append_access_event(const access_event_t *event);
bool storage_append_system_event(const char *type, const char *details);
bool storage_add_light_usage_ms(room_id_t room, uint64_t elapsed_ms);
bool storage_get_light_usage_ms(uint64_t *room1_ms, uint64_t *room2_ms);
bool storage_visit_access_records(storage_record_callback_t callback, void *context);
bool storage_visit_system_records(storage_record_callback_t callback, void *context);
