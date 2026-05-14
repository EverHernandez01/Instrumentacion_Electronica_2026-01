#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_log.h"
#include "mqtt_client.h"

#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_random.h"
#include "rgb_led.h"



// MQTT Topics
#define TOPIC_SENSOR  "iot/sensor"
#define TOPIC_CONTROL "iot/control"

static const char *TAG = "mqtt_iot";
static esp_mqtt_client_handle_t mqtt_client = NULL;
/*
static void handle_control_message(const char *data, int data_len)
{
    // Null-terminate the incoming data for safe parsing
    char buf[64];
    int len = data_len < (int)sizeof(buf) - 1 ? data_len : (int)sizeof(buf) - 1;
    memcpy(buf, data, len);
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (root != NULL) {
        cJSON *state_item = cJSON_GetObjectItem(root, "state");
        if (state_item != NULL && cJSON_IsNumber(state_item)) {
            int state = state_item->valueint;
            gpio_set_level(BLINK_GPIO, state);
            ESP_LOGI(TAG, "Actuating Command Received. LED State: %d", state);
        }
        cJSON_Delete(root);
    }
}*/

static void handle_control_message(const char *data, int data_len)
{
    char buf[128];

    int len = data_len < (int)sizeof(buf) - 1
              ? data_len
              : (int)sizeof(buf) - 1;

    memcpy(buf, data, len);

    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);

    if (root != NULL) {

        cJSON *state_item = cJSON_GetObjectItem(root, "state");
        cJSON *r_item     = cJSON_GetObjectItem(root, "r");
        cJSON *g_item     = cJSON_GetObjectItem(root, "g");
        cJSON *b_item     = cJSON_GetObjectItem(root, "b");

        if (
            cJSON_IsNumber(state_item) &&
            cJSON_IsNumber(r_item) &&
            cJSON_IsNumber(g_item) &&
            cJSON_IsNumber(b_item)
        ) {

            int state = state_item->valueint;

            int r = r_item->valueint;
            int g = g_item->valueint;
            int b = b_item->valueint;

            if (state == 1) {

                rgb_led_set(r, g, b);

                ESP_LOGI(
                    TAG,
                    "LED ON -> R:%d G:%d B:%d",
                    r, g, b
                );

            } else {

                rgb_led_off();

                ESP_LOGI(TAG, "LED OFF");
            }
        }

        cJSON_Delete(root);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to MQTT broker");
        // Subscribe to the control topic (like registering a POST handler in HTTP)
        esp_mqtt_client_subscribe(client, TOPIC_CONTROL, 1);
        ESP_LOGI(TAG, "Subscribed to: %s", TOPIC_CONTROL);
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "Received message on: %.*s", event->topic_len, event->topic);
        // Route message to the appropriate handler (like URI dispatching in HTTP)
        if (strncmp(event->topic, TOPIC_CONTROL, event->topic_len) == 0) {
            handle_control_message(event->data, event->data_len);
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from MQTT broker");
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error occurred");
        break;

    default:
        break;
    }
}

static void sensor_publish_task(void *pvParameters)
{
    while (1) {
        if (mqtt_client != NULL) {
            // Generate a dummy temperature between 20.0 and 29.9
            float temp = 20.0f + (esp_random() % 100) / 10.0f;

            char buffer[64];
            snprintf(buffer, sizeof(buffer), "{\"temperature\": %.1f}", temp);

            esp_mqtt_client_publish(mqtt_client, TOPIC_SENSOR, buffer, 0, 0, 0);
            ESP_LOGI(TAG, "Published: %s -> %s", TOPIC_SENSOR, buffer);
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // Publish every 2 seconds
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize Actuator Hardware (LED) — same as Lab 0
    rgb_led_init();

    // Connect to Wi-Fi — same as Lab 0
    ESP_ERROR_CHECK(example_connect());

    // Configure and start MQTT client
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://10.177.54.102:1883",
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    // Start the sensor publishing task
    xTaskCreate(sensor_publish_task, "sensor_pub", 4096, NULL, 5, NULL);
}