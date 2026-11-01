idf_component_register(
    SRCS "src/management.c" "src/management_reports.c" "src/management_actions.c"
    INCLUDE_DIRS "include"
    REQUIRES integration room_control access_schedule storage
)
