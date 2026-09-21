idf_component_register(
    SRCS
        "main.c"
        "imu_spi.c"
        "icm42670p.c"
        "scl3400.c"
        "oled_view.c"
        "qmc5883l.c"
        "mag_display.c"
    INCLUDE_DIRS
        "."
    REQUIRES
        driver
        esp_timer
        nvs_flash      # se usa desde la etapa de calibración (guardar los valores)
        helper_i2c     # componente copiado del proyecto viejo
        ssd1306        # componente copiado del proyecto viejo
)
