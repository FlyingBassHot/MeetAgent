idf_component_register(
    SRCS "app_main.c" "board_lcd4.c"
    INCLUDE_DIRS "."
    REQUIRES
        espui
        protocol
        state
        networking
        nvs_flash
        waveshare__esp32_s3_touch_lcd_4
)
