#ifndef DRIVER_VEN_H
#define DRIVER_VEN_H

#include <stdint.h>
#include "driver/ledc.h"
#include "esp_err.h"

// Estructura del ventilador PWM
typedef struct {
    gpio_num_t pin;
    ledc_channel_t channel;
    ledc_timer_t timer;
    float temp_start;          // Temperatura donde el ventilador empieza a girar (0%)
    float temp_max;            // Temperatura donde llega a su máxima capacidad (100%)
    uint8_t current_speed_pct; // Velocidad actual en porcentaje (0-100) para mostrar en pantalla
} ven_t;

/**
 * @brief Inicializa el pin del ventilador con PWM (LEDC)
 * @param dev Puntero a la estructura
 * @param pin Número de GPIO (Ej: 4)
 * @param temp_start Temperatura de arranque
 * @param temp_max Temperatura de 100% de potencia
 */
esp_err_t ven_init(ven_t *dev, gpio_num_t pin, float temp_start, float temp_max);

/**
 * @brief Actualiza la velocidad del ventilador proporcionalmente a la temperatura
 * @param dev Puntero a la estructura
 * @param current_temp Temperatura actual leída del termistor
 */
void ven_update(ven_t *dev, float current_temp);

#endif // DRIVER_VEN_H