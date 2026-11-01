#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    bool administrator_authenticated;
    char administrator_id[32];
} management_context_t;

bool management_read_console_line(char *line, size_t size);
bool management_read_secret_line(char *line, size_t size);
void management_print_report_title(const char *title);
void management_print_time(unsigned short minute);

void management_action_manage_schedule(management_context_t *context);
void management_action_login(management_context_t *context);
void management_action_logout(management_context_t *context);
void management_action_change_pin(management_context_t *context);
void management_action_change_id(management_context_t *context);
void management_action_set_clock(management_context_t *context);
