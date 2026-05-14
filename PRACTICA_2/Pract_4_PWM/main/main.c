#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

// ===== CONFIGURACIÓN ADC =====
#define ADC_CHANNEL_LDR_1   ADC_CHANNEL_5   // GPIO 5 (Izquierdo)
#define ADC_CHANNEL_LDR_2   ADC_CHANNEL_0   // GPIO 0 (Derecho)

// ===== CONFIGURACIÓN SERVO (LEDC) =====
#define SERVO_GPIO          18              // Pin de señal del Servo
#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT 
#define LEDC_FREQUENCY      50              // 50Hz

// Duty para 13 bits (0 a 8191)
#define SERVO_MIN_DUTY      205             // 0.5ms (0°)
#define SERVO_MAX_DUTY      1024            // 2.5ms (180°)

// ===== GPIO LEDs =====
#define LED_1               4               // Indica luz a la izquierda
#define LED_2               7               // Indica luz a la derecha

// ===== PARÁMETROS FÍSICOS =====
#define VCC                 3.3
#define ADC_MAX             4095.0
#define R_FIXED             10000.0         // 10kΩ

// ===== UMBRALES DE LÓGICA =====
#define RATIO_THRESHOLD     0.58            // Umbral de desbalance
#define MIN_SUM_ADC         150             // Umbral de oscuridad total
#define SERVO_STEP          8               // Grados a mover por ciclo

// Variables Globales
adc_oneshot_unit_handle_t adc_handle;
int current_angle = 90;                     // Posición inicial

// Función para mapear ángulo (0-180) a duty cycle
static uint32_t angle_to_duty(int angle) {
    return SERVO_MIN_DUTY + ((SERVO_MAX_DUTY - SERVO_MIN_DUTY) * angle) / 180;
}

void init_servo() {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = SERVO_GPIO,
        .duty           = angle_to_duty(current_angle),
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
}

void app_main(void) {
    // ---- 1. Inicializar ADC ----
    adc_oneshot_unit_init_cfg_t init_config = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    adc_oneshot_chan_cfg_t adc_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_LDR_1, &adc_config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_LDR_2, &adc_config));

    // ---- 2. Configurar GPIOs (LEDs) ----
    gpio_reset_pin(LED_1);
    gpio_reset_pin(LED_2);
    gpio_set_direction(LED_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_2, GPIO_MODE_OUTPUT);

    // ---- 3. Inicializar Servo ----
    init_servo();

    int adc_ldr1, adc_ldr2;
    printf("=== Sistema Seguidor de Luz con Servo Iniciado ===\n");

    while (1) {
        // Lectura ADC
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_LDR_1, &adc_ldr1));
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_LDR_2, &adc_ldr2));

        // Cálculo de Voltajes y Resistencias
        float v1 = (adc_ldr1 / ADC_MAX) * VCC;
        float v2 = (adc_ldr2 / ADC_MAX) * VCC;
        float r_ldr1 = (v1 > 0.05) ? R_FIXED * ((VCC / v1) - 1.0) : 1000000.0;
        float r_ldr2 = (v2 > 0.05) ? R_FIXED * ((VCC / v2) - 1.0) : 1000000.0;

        int sum_adc = adc_ldr1 + adc_ldr2;
        
        if (sum_adc < MIN_SUM_ADC) {
            // OSCURIDAD: No mover servo, apagar LEDs
            gpio_set_level(LED_1, 0);
            gpio_set_level(LED_2, 0);
            printf("[MODO: OSCURIDAD] ");
        } 
        else {
            float ratio1 = (float)adc_ldr1 / sum_adc;

            if (ratio1 > RATIO_THRESHOLD) {
                // MÁS LUZ EN IZQUIERDA (LDR 1)
                gpio_set_level(LED_1, 1);
                gpio_set_level(LED_2, 0);
                current_angle -= SERVO_STEP; // Girar hacia la izquierda
                printf("[MODO: IZQUIERDA] ");
            } 
            else if (ratio1 < (1.0 - RATIO_THRESHOLD)) {
                // MÁS LUZ EN DERECHA (LDR 2)
                gpio_set_level(LED_1, 0);
                gpio_set_level(LED_2, 1);
                current_angle += SERVO_STEP; // Girar hacia la derecha
                printf("[MODO: DERECHA  ] ");
            } 
            else {
                // EQUILIBRADO
                gpio_set_level(LED_1, 0);
                gpio_set_level(LED_2, 0);
                printf("[MODO: CENTRADO ] ");
            }
        }

        // Limitar ángulo entre 0 y 180
        if (current_angle > 180) current_angle = 180;
        if (current_angle < 0) current_angle = 0;

        // Actualizar posición del servo
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, angle_to_duty(current_angle));
        ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

        // Monitor Serial
        printf("R1: %8.1f | R2: %8.1f | Angulo: %d\n", r_ldr1, r_ldr2, current_angle);

        vTaskDelay(pdMS_TO_TICKS(20)); // Ajuste de velocidad de respuesta
    }
}