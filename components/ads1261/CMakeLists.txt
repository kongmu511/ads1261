idf_component_register(
    SRCS "ads1261.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_common freertos
    PRIV_REQUIRES esp_timer
)
