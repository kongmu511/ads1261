idf_component_register(
    SRCS "uart_cmd.c" "loadcell.c" "main.c"
    INCLUDE_DIRS "."
    REQUIRES freertos esp_system driver esp_common ads1261 esp_timer spi_flash
)
