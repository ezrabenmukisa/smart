#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "access_schedule.h"
#include "storage.h"
#include "management_internal.h"

#define DEFAULT_ADMIN_PIN "2468"

static bool parse_time(const char *text, uint16_t *minute)
{
    int hour, min;
    char extra;
    if (sscanf(text, "%d:%d%c", &hour, &min, &extra) != 2 ||
        hour < 0 || hour > 23 || min < 0 || min > 59)
        return false;
    *minute = (uint16_t)(hour * 60 + min);
    return true;
}

static bool valid_admin_id(const char *id)
{
    size_t length = strlen(id);
    if (length == 0 || length > 31) return false;
    for (size_t i = 0; i < length; i++) {
        unsigned char c = (unsigned char)id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    }
    return true;
}

static bool ensure_administrator_authenticated(management_context_t *context);

void management_action_manage_schedule(management_context_t *context)
{
    management_print_report_title("ACCESS SCHEDULE CONFIGURATION");
    if (!ensure_administrator_authenticated(context)) return;
    char start_text[32], end_text[32];
    uint16_t start, end;
    printf("\nStart time (HH:MM): "); fflush(stdout);
    if (!management_read_console_line(start_text, sizeof(start_text)) || !parse_time(start_text, &start)) {
        printf("Status                 : invalid start time; use HH:MM\n");
        return;
    }
    printf("End time (HH:MM): "); fflush(stdout);
    if (!management_read_console_line(end_text, sizeof(end_text)) || !parse_time(end_text, &end) || start >= end) {
        printf("Status                 : invalid schedule; start must be earlier than end\n");
        return;
    }
    if (!storage_save_schedule(start, end, context->administrator_id)) {
        printf("Status                 : save failed; schedule unchanged\n");
        printf("Storage                : SD card unavailable or write failed\n");
        return;
    }
    if (access_schedule_set(start, end)) {
        uint16_t now = access_schedule_get_clock_minute();
        char details[112];
        snprintf(details, sizeof(details),
                 "by=%s;window=%02u:%02u-%02u:%02u;clock=%02u:%02u",
                 context->administrator_id,
                 (unsigned)(start / 60), (unsigned)(start % 60),
                 (unsigned)(end / 60), (unsigned)(end % 60),
                 (unsigned)(now / 60), (unsigned)(now % 60));
        storage_append_system_event("SCHEDULE", details);
        printf("Status                 : saved and active\n");
        printf("Access window          : "); management_print_time(start); printf(" - "); management_print_time(end); printf("\n");
        printf("Scheduled by           : %s\n", context->administrator_id);
        printf("Applies to             : entry and exit\n");
        printf("Storage                : SD card\n");
        printf("Current simulated time : "); management_print_time(now); printf("\n");
        if (now >= start && now < end) {
            printf("Access right now       : within schedule\n");
        } else if (now < start) {
            printf("Access right now       : outside schedule; opens at ");
            management_print_time(start); printf("\n");
        } else {
            printf("Access right now       : outside schedule; next window starts at ");
            management_print_time(start); printf(" tomorrow\n");
        }
    } else {
        printf("Status                 : saved, but activation failed\n");
    }
}

void management_action_login(management_context_t *context)
{
    management_print_report_title("ADMINISTRATOR LOGIN");
    if (context->administrator_authenticated) {
        printf("Status                 : already authenticated\n");
        return;
    }
    char id[32], pin[32];
    printf("\nAdministrator ID: "); fflush(stdout);
    if (!management_read_console_line(id, sizeof(id)) || !valid_admin_id(id)) {
        printf("Invalid administrator ID. Use letters, digits, _ or -.\n");
        return;
    }
    printf("Administrator PIN : "); fflush(stdout);
    if (!management_read_secret_line(pin, sizeof(pin))) {
        printf("PIN input is too long.\n");
        return;
    }
    if (!storage_is_ready()) {
        printf("Status                 : login unavailable; SD storage is not ready\n");
        return;
    }
    bool pin_matches;
    if (storage_admin_pin_is_configured()) {
        printf("Verifying PIN securely; please wait...\n");
        fflush(stdout);
        pin_matches = storage_verify_admin_pin(pin);
        printf("PIN verification complete.\n");
    } else {
        pin_matches = strcmp(pin, DEFAULT_ADMIN_PIN) == 0;
    }
    if (strcmp(id, context->administrator_id) == 0 && pin_matches) {
        context->administrator_authenticated = true;
        char details[64];
        snprintf(details, sizeof(details), "id=%s;login=success", context->administrator_id);
        storage_append_system_event("ADMIN", details);
        printf("Status                 : login successful\n");
        printf("Administrator          : %s\n", context->administrator_id);
    } else {
        storage_append_system_event("ADMIN", "login=failed");
        printf("Status                 : login failed; check ID and PIN\n");
    }
}

static bool ensure_administrator_authenticated(management_context_t *context)
{
    if (context->administrator_authenticated) return true;
    printf("Administrator login is required to continue.\n");
    management_action_login(context);
    if (!context->administrator_authenticated)
        printf("Action canceled          : administrator authentication failed\n");
    return context->administrator_authenticated;
}

void management_action_logout(management_context_t *context)
{
    management_print_report_title("ADMINISTRATOR LOGOUT");
    context->administrator_authenticated = false;
    char details[48];
    snprintf(details, sizeof(details), "id=%s;logout", context->administrator_id);
    storage_append_system_event("ADMIN", details);
    printf("Status                 : logged out\n");
}

void management_action_change_pin(management_context_t *context)
{
    management_print_report_title("CHANGE ADMINISTRATOR PIN");
    if (!ensure_administrator_authenticated(context)) return;
    char new_pin[32], confirm[32];
    printf("New PIN (4-12 digits, input hidden): "); fflush(stdout);
    if (!management_read_secret_line(new_pin, sizeof(new_pin))) return;
    size_t length = strlen(new_pin);
    if (length < 4 || length > 12) {
        printf("PIN must contain 4 to 12 digits.\n");
        return;
    }
    for (size_t i = 0; i < length; i++) {
        if (new_pin[i] < '0' || new_pin[i] > '9') {
            printf("PIN must contain digits only.\n");
            return;
        }
    }
    printf("Confirm new PIN (input hidden): "); fflush(stdout);
    if (!management_read_secret_line(confirm, sizeof(confirm)) || strcmp(new_pin, confirm) != 0) {
        printf("Status                 : PIN entries do not match\n");
        return;
    }
    printf("PIN confirmed. Saving the protected credential; please wait...\n");
    fflush(stdout);
    if (!storage_save_admin_pin(new_pin)) {
        printf("\nPIN CHANGE FAILED: the current PIN remains active.\n");
        return;
    }
    char details[64];
    snprintf(details, sizeof(details), "id=%s;PIN changed", context->administrator_id);
    storage_append_system_event("ADMIN", details);
    printf("\nPIN CHANGE SUCCESSFUL\n");
    printf("The new PIN is active and stored securely on the SD card.\n");
}

void management_action_change_id(management_context_t *context)
{
    management_print_report_title("CHANGE ADMINISTRATOR ID");
    if (!ensure_administrator_authenticated(context)) return;
    char new_id[32];
    printf("\nNew administrator ID (letters, digits, _ or -): "); fflush(stdout);
    if (!management_read_console_line(new_id, sizeof(new_id)) || !valid_admin_id(new_id)) {
        printf("Status                 : invalid administrator ID\n");
        return;
    }
    if (!storage_save_admin_id(new_id)) {
        printf("Status                 : save failed; administrator ID unchanged\n");
        return;
    }
    char details[80];
    snprintf(details, sizeof(details), "old_id=%s;new_id=%s", context->administrator_id, new_id);
    storage_append_system_event("ADMIN_ID", details);
    memcpy(context->administrator_id, new_id, strlen(new_id) + 1);
    printf("Status                 : administrator ID changed and saved\n");
    printf("Administrator          : %s\n", context->administrator_id);
}

void management_action_set_clock(management_context_t *context)
{
    management_print_report_title("SET SIMULATED SYSTEM TIME");
    if (!ensure_administrator_authenticated(context)) return;
    char text[32];
    uint16_t minute;
    printf("\nSet simulated time (HH:MM): "); fflush(stdout);
    if (!management_read_console_line(text, sizeof(text)) || !parse_time(text, &minute)) {
        printf("Status                 : invalid time; use HH:MM\n");
        return;
    }
    if (access_schedule_set_clock(minute)) {
        printf("Status                 : simulated clock updated\n");
        printf("Current time           : "); management_print_time(minute); printf("\n");
    }
}
