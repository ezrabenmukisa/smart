idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES access_control rfid_reader room_control integration management access_schedule storage
)
