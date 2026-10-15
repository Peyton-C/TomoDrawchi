idf_component_register(
    SRCS
        "main.c"
        "switch_hid.c"
        "serial_parser.c"
    INCLUDE_DIRS "."
    REQUIRES
        tinyusb
        esp_driver_uart
        esp_timer
        freertos
)