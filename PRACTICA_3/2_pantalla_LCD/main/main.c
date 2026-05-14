#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"

#include "driver/i2c_master.h"

#include "ssd1306.h"
#include "potentiometer.h"

// ======================================================
// I2C OLED CONFIG
// ======================================================

#define I2C_SDA_PIN           6
#define I2C_SCL_PIN           7
#define I2C_PORT              0

#define SSD1306_WIDTH         128
#define SSD1306_HEIGHT        64
#define SSD1306_I2C_ADDRESS   0x3C

// ======================================================
// CLOCK VARIABLES
// ======================================================

static volatile uint8_t hours = 1;
static volatile uint8_t minutes = 11;
static volatile uint8_t seconds = 30;

// ======================================================
// DISPLAY VARIABLES
// ======================================================

static i2c_master_bus_handle_t bus_handle;
static i2c_ssd1306_handle_t display;

static const char *TAG = "MAIN";

// ======================================================
// CLOCK TASK
// ======================================================

void clock_task(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    while (1)
    {
        seconds++;

        if (seconds >= 60)
        {
            seconds = 0;
            minutes++;
        }

        if (minutes >= 60)
        {
            minutes = 0;
            hours++;
        }

        if (hours >= 24)
        {
            hours = 0;
        }

        vTaskDelayUntil(
            &last_wake_time,
            pdMS_TO_TICKS(1000)
        );
    }
}

// ======================================================
// DISPLAY TASK
// ======================================================

void display_task(void *pvParameters)
{
    char time_buffer[20];
    char adc_buffer[20];

    uint16_t adc_value = 0;

    uint32_t adc_sum = 0;

    uint8_t local_hours;
    uint8_t local_minutes;
    uint8_t local_seconds;

    uint8_t serial_counter = 0;

    while (1)
    {
        // ==================================================
        // ADC FILTER
        // ==================================================

        adc_sum = 0;

        for (int i = 0; i < 8; i++)
        {
            adc_sum += potentiometer_read();
        }

        adc_value = adc_sum / 8;

        // ==================================================
        // LOCAL CLOCK COPY
        // ==================================================

        local_hours = hours;
        local_minutes = minutes;
        local_seconds = seconds;

        // ==================================================
        // FORMAT STRINGS
        // ==================================================

        snprintf(
            time_buffer,
            sizeof(time_buffer),
            "hora: %02d:%02d:%02d",
            local_hours,
            local_minutes,
            local_seconds
        );

        snprintf(
            adc_buffer,
            sizeof(adc_buffer),
            "POT: %4d",
            adc_value
        );

        // ==================================================
        // SERIAL EACH SECOND
        // ==================================================

        serial_counter++;

        if (serial_counter >= 5)
        {
            serial_counter = 0;

            ESP_LOGI(
                TAG,
                "Hora: %s | POT: %d",
                time_buffer,
                adc_value
            );
        }

        // ==================================================
        // OLED
        // ==================================================

        ESP_ERROR_CHECK(
            i2c_ssd1306_buffer_clear(&display)
        );

        // ------------------------------
        // CLOCK
        // ------------------------------

        ESP_ERROR_CHECK(
            i2c_ssd1306_buffer_text(
                &display,
                0,
                0,
                time_buffer,
                false
            )
        );

        // ------------------------------
        // ADC
        // ------------------------------

        ESP_ERROR_CHECK(
            i2c_ssd1306_buffer_text(
                &display,
                0,
                24,
                adc_buffer,
                false
            )
        );

        // ==================================================
        // SEND BUFFER TO OLED
        // ==================================================

        ESP_ERROR_CHECK(
            i2c_ssd1306_buffer_to_ram(&display)
        );

        // ==================================================
        // OLED REFRESH
        // ==================================================

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

// ======================================================
// APP MAIN
// ======================================================

void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando sistema");

    // ==================================================
    // I2C CONFIG
    // ==================================================

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true
    };

    ESP_ERROR_CHECK(
        i2c_new_master_bus(
            &bus_config,
            &bus_handle
        )
    );

    // ==================================================
    // SSD1306 CONFIG
    // ==================================================

    i2c_ssd1306_config_t ssd1306_config = {
        .i2c_device_address = SSD1306_I2C_ADDRESS,
        .i2c_scl_speed_hz = 400000,
        .width = SSD1306_WIDTH,
        .height = SSD1306_HEIGHT,
        .wise = SSD1306_TOP_TO_BOTTOM
    };

    ESP_ERROR_CHECK(
        i2c_ssd1306_init(
            bus_handle,
            ssd1306_config,
            &display
        )
    );

    ESP_LOGI(TAG, "OLED inicializada");

    // ==================================================
    // ADC INIT
    // ==================================================

    potentiometer_init();

    ESP_LOGI(TAG, "Potenciometro inicializado");

    // ==================================================
    // CREATE TASKS
    // ==================================================

    xTaskCreate(
        clock_task,
        "clock_task",
        4096,
        NULL,
        5,
        NULL
    );

    xTaskCreate(
        display_task,
        "display_task",
        4096,
        NULL,
        5,
        NULL
    );
}