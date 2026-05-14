
idf_component_register(SRCS "app_main.c" "rgb_led.c"
                    PRIV_REQUIRES mqtt nvs_flash esp_netif json driver led_strip
                    INCLUDE_DIRS ".")