#include "bomb.h"

void bomb_init(bomb_t *dev, gpio_num_t pin, bomb_mode_t mode, float setpoint, float hysteresis) {
    dev->pin = pin;
    dev->setpoint = setpoint;
    dev->hysteresis = hysteresis;
    dev->mode = mode;
    dev->is_on = false;

    // Configurar el pin GPIO como salida
    gpio_reset_pin(dev->pin);
    gpio_set_direction(dev->pin, GPIO_MODE_OUTPUT);
    gpio_set_level(dev->pin, 0); // Nos aseguramos de que inicie apagado
}

void bomb_set_state(bomb_t *dev, bool state) {
    if (dev->is_on != state) {
        dev->is_on = state;
        gpio_set_level(dev->pin, state ? 1 : 0);
    }
}

void bomb_update(bomb_t *dev, float current_temp) {
    // Calculamos los umbrales inferior y superior
    float limit_low = dev->setpoint - (dev->hysteresis / 2.0f);
    float limit_high = dev->setpoint + (dev->hysteresis / 2.0f);

    if (dev->mode == BOMB_MODE_HEATING) {
        // Modo CALEFACCIÓN
        if (current_temp <= limit_low) {
            bomb_set_state(dev, true);  // Está muy frío, calentar
        } else if (current_temp >= limit_high) {
            bomb_set_state(dev, false); // Ya llegó a la temperatura, apagar
        }
    } else {
        // Modo REFRIGERACIÓN
        if (current_temp >= limit_high) {
            bomb_set_state(dev, true);  // Está muy caliente, enfriar
        } else if (current_temp <= limit_low) {
            bomb_set_state(dev, false); // Ya se enfrió, apagar
        }
    }
}