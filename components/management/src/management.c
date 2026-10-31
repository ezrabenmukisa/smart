#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "management.h"

/* ---------- Console input ---------- */

static int read_choice(void)
{
    char line[32];

    while (1)
    {
        printf("\nEnter choice: ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(100));
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

        if (errno == ERANGE || choice < 0 || choice > 10)
        {
            printf("Invalid choice. Enter a number from 0 to 10.\n");
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
    char line[8];

    printf("\nPress Enter to return to the menu...");
    fflush(stdout);

    while (fgets(line, sizeof(line), stdin) == NULL)
    {
        clearerr(stdin);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ---------- Menu display ---------- */

static void print_menu(void)
{
    printf("\n===== CoCIS SMART CONTROL SYSTEM =====\n");
    printf("1. View Building Occupancy\n");
    printf("2. View Room Status and Lights\n");
    printf("3. Manage Access Schedule\n");
    printf("4. View Recent Access Records\n");
    printf("5. View Light Usage\n");
    printf("6. View Complete Event History\n");
    printf("7. Administrator Login\n");
    printf("8. View Saved Access Schedule\n");
    printf("9. Administrator Logout\n");
    printf("10. Change Administrator PIN\n");
    printf("0. Exit Console\n");
}

/* ---------- Menu actions ---------- */
/*
 * These are placeholders until the corresponding component APIs
 * are connected. Keep each action's implementation in its component
 * and call that component's documented function here.
 */

static void show_occupancy(void)
{
    printf("\nOccupancy view: connect this to the occupancy component.\n");
}

static void show_room_status(void)
{
    printf("\nRoom status and lights: connect this to the room-control component.\n");
}

static void manage_access_schedule(void)
{
    printf("\nSchedule management: connect this to the schedule/storage handlers.\n");
}

static void show_recent_records(void)
{
    printf("\nRecent records: connect this to the storage component.\n");
}

static void show_light_usage(void)
{
    printf("\nLight usage: connect this to the room-control/storage components.\n");
}

static void show_event_history(void)
{
    printf("\nEvent history: connect this to the storage component.\n");
}

static void administrator_login(void)
{
    printf("\nAdministrator login: connect this to the authentication handler.\n");
}

static void show_saved_schedule(void)
{
    printf("\nSaved schedule: connect this to the schedule/storage handlers.\n");
}

static void administrator_logout(void)
{
    printf("\nAdministrator logout: connect this to the authentication handler.\n");
}

static void change_administrator_pin(void)
{
    printf("\nChange administrator PIN: connect this to the authentication handler.\n");
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
            show_occupancy();
            break;

        case 2:
            show_room_status();
            break;

        case 3:
            manage_access_schedule();
            break;

        case 4:
            show_recent_records();
            break;

        case 5:
            show_light_usage();
            break;

        case 6:
            show_event_history();
            break;

        case 7:
            administrator_login();
            break;

        case 8:
            show_saved_schedule();
            break;

        case 9:
            administrator_logout();
            break;

        case 10:
            change_administrator_pin();
            break;

        case 0:
            printf("\nExiting management console.\n");
            vTaskDelete(NULL);
            return;

        default:
            printf("\nInvalid choice. Please select 0 to 10.\n");
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