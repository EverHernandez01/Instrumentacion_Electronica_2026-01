#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "driver/gpio.h"

// ===== CONFIGURACIÓN ADC =====
#define ADC_CHANNEL_LDR_1   ADC_CHANNEL_5   // GPIO 5 (Izquierdo)
#define ADC_CHANNEL_LDR_2   ADC_CHANNEL_0   // GPIO 0 (Derecho)

// ===== GPIO LEDs =====
#define LED_1   4  // Indica luz a la izquierda
#define LED_2   7  // Indica luz a la derecha

// ===== PARÁMETROS FÍSICOS =====
#define VCC 3.3
#define ADC_MAX 4095.0
#define R_FIXED 10000.0   // Resistencia fija de 10kΩ en el divisor

// ===== UMBRALES DE LÓGICA =====
#define RATIO_THRESHOLD 0.58  // Sensibilidad: >58% de luz en un lado activa LED
#define MIN_SUM_ADC 150       // Debajo de esto se considera oscuridad total
#define HIGH_LIGHT_ADC 3200   // Umbral para considerar "Intensidad Lumínica Alta"

adc_oneshot_unit_handle_t adc_handle;

void app_main(void)
{
    // ---- 1. Inicializar Unidad ADC ----
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

    // ---- 2. Configurar Canales ADC (12 bits, Atenuación 11dB para 3.3V) ----
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_LDR_1, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_LDR_2, &config));

    // ---- 3. Configurar GPIOs para LEDs ----
    gpio_reset_pin(LED_1);
    gpio_reset_pin(LED_2);
    gpio_set_direction(LED_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_2, GPIO_MODE_OUTPUT);

    int adc_ldr1, adc_ldr2;

    printf("=== Sistema de Monitoreo LDR Iniciado ===\n");

    while (1)
    {
        // ---- A. Lectura de Sensores (Necesaria para los cálculos) ----
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_LDR_1, &adc_ldr1));
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL_LDR_2, &adc_ldr2));

        // ---- B. Escalamiento a Voltaje (V) ----
        float v1 = (adc_ldr1 / ADC_MAX) * VCC;
        float v2 = (adc_ldr2 / ADC_MAX) * VCC;

        // ---- C. Cálculo de Resistencia (Ohm) ----
        float r_ldr1 = (v1 > 0.01) ? R_FIXED * ((VCC / v1) - 1.0) : 1000000.0;
        float r_ldr2 = (v2 > 0.01) ? R_FIXED * ((VCC / v2) - 1.0) : 1000000.0;

        // ---- D. Lógica de Control de LEDs (Posición del Foco) ----
        int sum_adc = adc_ldr1 + adc_ldr2;
        
        if (sum_adc < MIN_SUM_ADC) 
        {
            // Estado: Oscuridad
            gpio_set_level(LED_1, 0);
            gpio_set_level(LED_2, 0);
            printf("[ESTADO: OSCURIDAD] ");
        } 
        else 
        {
            float ratio1 = (float)adc_ldr1 / sum_adc;

            if (ratio1 > RATIO_THRESHOLD) 
            {
                gpio_set_level(LED_1, 1);
                gpio_set_level(LED_2, 0);
                printf("[ESTADO: IZQUIERDA] ");
            } 
            else if (ratio1 < (1.0 - RATIO_THRESHOLD)) 
            {
                gpio_set_level(LED_1, 0);
                gpio_set_level(LED_2, 1);
                printf("[ESTADO: DERECHA  ] ");
            } 
            else 
            {
                gpio_set_level(LED_1, 0);
                gpio_set_level(LED_2, 0);
                printf("[ESTADO: CENTRADO ] ");
            }
        }

        // ---- E. Verificación de Intensidad Lumínica Alta ----
        if (adc_ldr1 > HIGH_LIGHT_ADC || adc_ldr2 > HIGH_LIGHT_ADC) {
            printf("!! ALTA INTENSIDAD !! ");
        }

        // ---- F. Monitor Serial Limpio (ADC Oculto) ----
        // Solo muestra Voltaje y Resistencia para una presentación clara
        printf("V1: %.2fV | V2: %.2fV | R1: %.1f Ohm | R2: %.1f Ohm\n", 
               v1, v2, r_ldr1, r_ldr2);

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}