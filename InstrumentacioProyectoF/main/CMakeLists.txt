idf_component_register(SRCS "main.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES "Drivers" "nvs_flash" "esp_wifi" "esp_netif" "esp_event" "mqtt")