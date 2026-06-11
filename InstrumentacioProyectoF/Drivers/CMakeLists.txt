idf_component_register(SRCS "ssd1306.c" "term.c" "bomb.c" "ven.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver  esp_adc)