#ifndef DRIVER_SSD1306_H
#define DRIVER_SSD1306_H

#include <stdint.h>
#include "driver/i2c.h"
#include "esp_err.h"

// Direcciones I2C comunes para la SSD1306
#define SSD1306_I2C_ADDR_0  0x3C
#define SSD1306_I2C_ADDR_1  0x3D

// Resolución de la pantalla
#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      64

// Estructura principal del dispositivo
typedef struct {
    i2c_port_t i2c_port;
    uint8_t i2c_addr;
    uint8_t buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
} ssd1306_t;

/**
 * @brief Inicializa la pantalla SSD1306
 */
esp_err_t ssd1306_init(ssd1306_t *dev, i2c_port_t port, uint8_t addr);

/**
 * @brief Limpia el buffer interno (no actualiza la pantalla inmediatamente)
 */
void ssd1306_clear(ssd1306_t *dev);

/**
 * @brief Dibuja un píxel en el buffer interno
 */
void ssd1306_draw_pixel(ssd1306_t *dev, int x, int y, uint8_t color);

/**
 * @brief Dibuja una línea usando el algoritmo de Bresenham
 */
void ssd1306_draw_line(ssd1306_t *dev, int x0, int y0, int x1, int y1, uint8_t color);

/**
 * @brief Dibuja el contorno de un rectángulo
 */
void ssd1306_draw_rect(ssd1306_t *dev, int x, int y, int w, int h, uint8_t color);

/**
 * @brief Dibuja un solo carácter ASCII en la pantalla (fuente 5x8)
 * @param color 1 para texto normal, 0 para texto invertido
 */
void ssd1306_draw_char(ssd1306_t *dev, int x, int y, char c, uint8_t color);

/**
 * @brief Dibuja una cadena de texto (String)
 * @param color 1 para texto normal, 0 para texto invertido
 */
void ssd1306_draw_string(ssd1306_t *dev, int x, int y, const char *str, uint8_t color);

/**
 * @brief Envía el buffer completo a la pantalla por I2C
 */
esp_err_t ssd1306_update(ssd1306_t *dev);

#endif // DRIVER_SSD1306_H