#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "ble_server.h"
#include "rgb_led.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    esp_err_t ret = rgb_led_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "No se pudo inicializar el LED RGB: %s", esp_err_to_name(ret));
    }

    ble_init();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
