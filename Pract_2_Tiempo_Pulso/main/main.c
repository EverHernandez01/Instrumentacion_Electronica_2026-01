#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define BOTON_PIN GPIO_NUM_4
#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG = "EXAMEN_PULSOS";

// Variables para el tiempo y conteo
static uint32_t tiempo_ultimo_pulso = 0;
static uint32_t conteo_pulsos = 0;

// Cola para comunicar la interrupción con el programa principal
static QueueHandle_t cola_pulsos = NULL;

// Manejador de la Interrupción (ISR) - Se ejecuta en cada pulsación
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t tiempo_actual = xTaskGetTickCountFromISR();
    
    // Antibounce por software: Solo enviamos a la cola si han pasado más de 200ms
    // Esto evita que el rebote metálico genere múltiples interrupciones
    if (tiempo_actual - tiempo_ultimo_pulso > pdMS_TO_TICKS(200)) {
        xQueueSendFromISR(cola_pulsos, &tiempo_actual, NULL);
    }
    tiempo_ultimo_pulso = tiempo_actual;
}

// Tarea principal que procesa los datos y los muestra
void tarea_procesar_pulso(void* arg)
{
    uint32_t tiempo_recibido;
    uint32_t tiempo_anterior = 0;

    while(1) {
        // Espera a que la interrupción mande un dato a la cola
        if(xQueueReceive(cola_pulsos, &tiempo_recibido, portMAX_DELAY)) {
            conteo_pulsos++;
            
            if (conteo_pulsos > 1) {
                // Calculamos la diferencia en milisegundos
                uint32_t diferencia_ms = (tiempo_recibido - tiempo_anterior) * portTICK_PERIOD_MS;
                
                printf("\n-------------------------------------------");
                printf("\nPulso detectado #%lu", conteo_pulsos);
                printf("\nTiempo desde el último pulso: %lu ms", diferencia_ms);
                printf("\n-------------------------------------------\n");
            } else {
                printf("\nPrimer pulso detectado. Esperando el siguiente para medir...\n");
            }
            
            tiempo_anterior = tiempo_recibido;
        }
    }
}

void app_main(void)
{
    // 1. Configurar el GPIO con Interrupción
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_NEGEDGE,      // Interrupción en flanco de bajada
        .pin_bit_mask = (1ULL << BOTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,                     // Resistencia Pull-up interna
    };
    gpio_config(&io_conf);

    // 2. Crear la cola para pasar los tiempos (puedes guardar hasta 10 pulsos en buffer)
    cola_pulsos = xQueueCreate(10, sizeof(uint32_t));

    // 3. Crear la tarea que imprimirá en el monitor serial
    xTaskCreate(tarea_procesar_pulso, "tarea_procesar_pulso", 2048, NULL, 10, NULL);

    // 4. Instalar el servicio de interrupciones de GPIO
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    
    // 5. Asignar la función manejadora al pin específico
    gpio_isr_handler_add(BOTON_PIN, gpio_isr_handler, NULL);

    ESP_LOGI(TAG, "Sistema listo. Presiona el botón en GPIO %d para medir el tiempo.", BOTON_PIN);
}