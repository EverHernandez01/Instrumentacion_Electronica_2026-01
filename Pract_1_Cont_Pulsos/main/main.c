#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

// Puedes usar el GPIO 4 (Pin físico cercano a GND en la mayoría de C6)
#define BOTON_PIN GPIO_NUM_4 

void app_main(void)
{
    // Configuración del Pin: Entrada con resistencia Pull-up activa
    gpio_reset_pin(BOTON_PIN);
    gpio_set_direction(BOTON_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOTON_PIN, GPIO_PULLUP_ONLY);

    int contador = 0;
    int estado_anterior = 1; // 1 porque el pull-up lo mantiene en alto

    printf("--- Iniciando Contador de Pulsos en ESP32-C6 ---\n");

    while (1) {
        int estado_actual = gpio_get_level(BOTON_PIN);

        // Detectamos el flanco de bajada (cuando presionas el botón)
        if (estado_anterior == 1 && estado_actual == 0) {
            contador++;
            printf("Botón presionado. Conteo total: %d\n", contador);
            
            // Anti-rebote (debounce) simple: esperamos un momento
            vTaskDelay(pdMS_TO_TICKS(150)); 
        }

        estado_anterior = estado_actual;

        // Pequeña pausa para no estresar el procesador
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}