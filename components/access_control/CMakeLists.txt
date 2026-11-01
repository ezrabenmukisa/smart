idf_component_register(SRCS "src/access_control.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_driver_gpio esp_driver_ledc esp_timer freertos integration access_schedule storage)
