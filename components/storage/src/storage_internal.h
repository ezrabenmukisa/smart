#pragma once

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define STORAGE_MOUNT_POINT "/sdcard"
#define STORAGE_ACCESS_FILE STORAGE_MOUNT_POINT "/access.csv"
#define STORAGE_SYSTEM_FILE STORAGE_MOUNT_POINT "/system.csv"
#define STORAGE_SCHEDULE_FILE STORAGE_MOUNT_POINT "/schedule.txt"
#define STORAGE_PIN_FILE STORAGE_MOUNT_POINT "/admin.pin"
#define STORAGE_ADMIN_ID_FILE STORAGE_MOUNT_POINT "/admin.id"
#define STORAGE_LIGHT_FILE STORAGE_MOUNT_POINT "/light_usage.txt"

/* Private storage-component state shared by the persistence modules. */
void storage_lock_take(void);
void storage_lock_give(void);
bool storage_valid_admin_id(const char *admin_id);
