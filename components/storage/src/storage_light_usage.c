#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "storage.h"
#include "storage_internal.h"

bool storage_get_light_usage_ms(uint64_t *room1_ms, uint64_t *room2_ms)
{
    if (!storage_is_ready() || !room1_ms || !room2_ms) return false;
    storage_lock_take();
    FILE *file = fopen(STORAGE_LIGHT_FILE, "r");
    unsigned long long first = 0, second = 0;
    bool ok = !file || fscanf(file, "%llu %llu", &first, &second) == 2;
    if (file) fclose(file);
    if (ok) {
        *room1_ms = (uint64_t)first;
        *room2_ms = (uint64_t)second;
    }
    storage_lock_give();
    return ok;
}

bool storage_add_light_usage_ms(room_id_t room, uint64_t elapsed_ms)
{
    if (!storage_is_ready() || (room != ROOM_1 && room != ROOM_2)) return false;
    storage_lock_take();
    FILE *file = fopen(STORAGE_LIGHT_FILE, "r");
    unsigned long long totals[2] = { 0, 0 };
    bool ok = !file || fscanf(file, "%llu %llu", &totals[0], &totals[1]) == 2;
    if (file) fclose(file);
    if (ok) {
        totals[room - ROOM_1] += (unsigned long long)elapsed_ms;
        file = fopen(STORAGE_LIGHT_FILE, "w");
        ok = file && fprintf(file, "%llu %llu\n", totals[0], totals[1]) > 0;
        if (file && fclose(file) != 0) ok = false;
    }
    storage_lock_give();
    return ok;
}
