idf_component_register(
    SRCS "src/room_control.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio freertos
)