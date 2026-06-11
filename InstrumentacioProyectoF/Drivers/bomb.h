#ifndef DRIVER_BOMB_H
#define DRIVER_BOMB_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

// Define si el actuador se usa para calentar o para enfriar
typedef enum {
    BOMB_MODE_HEATING = 0, // Calefacción: Enciende cuando la temp baja (Ej: Resistencia térmica)
    BOMB_MODE_COOLING = 1  // Refrigeración: Enciende cuando la temp sube (Ej: Ventilador o bomba de agua)
} bomb_mode_t;

// Estructura del actuador
typedef struct {
    gpio_num_t pin;
    float setpoint;     // Temperatura objetivo
    float hysteresis;   // Ventana de tolerancia (Ej: 1.0 grado)
    bomb_mode_t mode;   // Modo de operación
    bool is_on;         // Estado actual físico
} bomb_t;

/**
 * @brief Inicializa el pin de la bomba/actuador
 * @param dev Puntero a la estructura
 * @param pin Número de GPIO (Ej: 5)
 * @param mode BOMB_MODE_HEATING o BOMB_MODE_COOLING
 * @param setpoint Temperatura a la que queremos llegar
 * @param hysteresis Rango total de la ventana en grados
 */
void bomb_init(bomb_t *dev, gpio_num_t pin, bomb_mode_t mode, float setpoint, float hysteresis);

/**
 * @brief Lee la temperatura y decide si encender o apagar la bomba
 * @param dev Puntero a la estructura
 * @param current_temp Temperatura actual leída del termistor
 */
void bomb_update(bomb_t *dev, float current_temp);

/**
 * @brief Fuerza el encendido o apagado manualmente
 */
void bomb_set_state(bomb_t *dev, bool state);

#endif // DRIVER_BOMB_H