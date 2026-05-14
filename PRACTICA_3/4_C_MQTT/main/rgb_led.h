#pragma once

/**
 * @file rgb_led.h
 * @brief Módulo de control del LED RGB WS2812 via RMT.
 *
 * El WS2812 es un LED RGB inteligente que recibe datos en serie por un único
 * pin usando un protocolo de temporización precisa. El periférico RMT (Remote
 * Control Transceiver) del ESP32-C6 genera esas señales con precisión de
 * nanosegundos sin ocupar la CPU.
 *
 * Pin del LED en ESP32-C6: GPIO 15
 *
 * Dependencias ESP-IDF: led_strip (componente idf-component-manager)
 * En idf_component.yml debes tener:
 *   espressif/led_strip: "^2.5.0"
 */

#include "esp_err.h"
#include <stdint.h>

/**
 * @brief Inicializa el controlador RMT y el LED WS2812.
 * Debe llamarse una vez antes de usar rgb_led_set() o rgb_led_off().
 *
 * @return ESP_OK si la inicialización fue exitosa.
 */
esp_err_t rgb_led_init(void);

/**
 * @brief Establece el color del LED RGB.
 *
 * @param r Componente rojo   (0–255)
 * @param g Componente verde  (0–255)
 * @param b Componente azul   (0–255)
 */
void rgb_led_set(uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Apaga el LED (equivalente a rgb_led_set(0, 0, 0)).
 */
void rgb_led_off(void);
