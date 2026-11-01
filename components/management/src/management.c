#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "management.h"
#include "management_reports.h"
#include "management_internal.h"
#include "storage.h"

static management_context_t management_context = {
    .administrator_authenticated = false,
    .administrator_id = "admin"
};

void management_print_time(unsigned short minute)
{
    printf("%02u:%02u", (unsigned)(minute / 60), (unsigned)(minute % 60));
}

void management_print_report_title(const char *title)
{
    printf("\n-------------------- %s --------------------\n", title);
}

/* ---------- Console input ---------- */

static bool read_console_line_mode(char *line, size_t size, bool mask_input)
{
    static bool skip_lf_after_cr;
    size_t length = 0;
    bool overflow = false;

    if (size == 0) return false;

    while (1) {
        int ch = getchar();
        if (ch == EOF) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (skip_lf_after_cr) {
            skip_lf_after_cr = false;
            if (ch == '\n') continue;
        }

        if (ch == '\r' || ch == '\n') {
            if (ch == '\r') skip_lf_after_cr = true;
            line[length] = '\0';
            printf("\r\n");
            return !overflow;
        }

        if (ch == '\b' || ch == 127) {
            if (length > 0) {
                length--;
                printf("\b \b");
            }
            continue;
        }

        if (ch >= 32 && ch <= 126) {
            if (length + 1 < size) {
                line[length++] = (char)ch;
                putchar(mask_input ? '*' : ch);
                fflush(stdout);
            } else {
                overflow = true;
            }
        }
    }
}

bool management_read_console_line(char *line, size_t size)
{
    return read_console_line_mode(line, size, false);
}

bool management_read_secret_line(char *line, size_t size)
{
    return read_console_line_mode(line, size, true);
}

static int read_choice(void)
{
    char line[32];

    while (1)
    {
        printf("\nEnter choice: ");
        fflush(stdout);

        if (!management_read_console_line(line, sizeof(line))) {
            printf("Input is too long. Please enter a number from 0 to 12.\n");
            continue;
        }

        char *end = NULL;
        errno = 0;
        long choice = strtol(line, &end, 10);

        if (end == line)
        {
            printf("Invalid input. Please enter a number.\n");
            continue;
        }

        if (errno == ERANGE || choice < 0 || choice > 12)
        {
            printf("Invalid choice. Enter a number from 0 to 12.\n");
            continue;
        }

        while (*end == ' ' || *end == '\t' ||
               *end == '\r' || *end == '\n')
        {
            end++;
        }

        if (*end != '\0')
        {
            printf("Invalid input. Enter only the menu number.\n");
            continue;
        }

        return (int)choice;
    }
}

static void wait_for_enter(void)
{
    char line[32];

    printf("\nPress Enter to return to the menu...");
    fflush(stdout);

    (void)management_read_console_line(line, sizeof(line));
}

/* ---------- Menu display ---------- */

static void print_menu(void)
{
    printf("\n===== CoCIS SMART CONTROL SYSTEM =====\n");
    printf("1. View Building Occupancy\n");
    printf("2. View Room Status and Lights\n");
    printf("3. Manage Access Schedule\n");
    printf("4. View Recent Access Records\n");
    printf("5. View Estimated Light Usage\n");
    printf("6. View Persisted Event History\n");
    printf("7. Administrator Login (ID and PIN)\n");
    printf("8. View Current Access Schedule and Clock\n");
    printf("9. Administrator Logout\n");
    printf("10. Change Administrator PIN\n");
    printf("11. Set Simulated Current Time (for testing)\n");
    printf("12. Change Administrator ID\n");
    printf("0. Exit Console\n");
}

/* ---------- Management task ---------- */

static void management_task(void *arg)
{
    (void)arg;

    while (1)
    {
        print_menu();

        int choice = read_choice();

        switch (choice)
        {
        case 1:
            management_report_show_occupancy();
            break;

        case 2:
            management_report_show_room_status();
            break;

        case 3:
            management_action_manage_schedule(&management_context);
            break;

        case 4:
            management_report_show_recent_access();
            break;

        case 5:
            management_report_show_light_usage();
            break;

        case 6:
            management_report_show_event_history();
            break;

        case 7:
            management_action_login(&management_context);
            break;

        case 8:
            management_report_show_schedule();
            break;

        case 9:
            management_action_logout(&management_context);
            break;

        case 10:
            management_action_change_pin(&management_context);
            break;

        case 11:
            management_action_set_clock(&management_context);
            break;

        case 12:
            management_action_change_id(&management_context);
            break;

        case 0:
            printf("\nExiting management console.\n");
            vTaskDelete(NULL);
            return;

        default:
            printf("\nInvalid choice. Please select 0 to 12.\n");
            break;
        }

        /*
         * Keep the result visible. The next menu is shown only after
         * the user presses Enter.
         */
        wait_for_enter();
    }
}

/* ---------- Public initialization ---------- */

void management_init(void)
{
    if (storage_is_ready()) {
        char saved_id[sizeof(management_context.administrator_id)];
        if (storage_load_admin_id(saved_id, sizeof(saved_id)))
            memcpy(management_context.administrator_id, saved_id, strlen(saved_id) + 1);
    }

    BaseType_t result = xTaskCreate(
        management_task,
        "management_task",
        4096,
        NULL,
        5,
        NULL);

    if (result != pdPASS)
    {
        printf("ERROR: Failed to create management task.\n");
    }
}
