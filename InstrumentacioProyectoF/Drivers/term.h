#ifndef DRIVER_TERMISTOR_H
#define DRIVER_TERMISTOR_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// Parámetros del termistor y circuito
#define TERM_R_SERIES       10000.0f  // Resistencia en serie (10k Ohms)
#define TERM_NOMINAL_RES    10000.0f  // Resistencia del termistor a 25°C (10k Ohms)
#define TERM_NOMINAL_TEMP   25.0f     // Temperatura nominal en °C
#define TERM_BETA_COEFF     3950.0f   // Coeficiente Beta del termistor (3950 es el más común)
#define TERM_VCC_MV         3300.0f   // Voltaje de alimentación en milivoltios (3.3V)

// Estructura del dispositivo termistor
typedef struct {
    adc_oneshot_unit_handle_t adc_handle;
    adc_cali_handle_t cali_handle;
    adc_channel_t adc_channel;
    bool do_calibration;
} termistor_t;

/**
 * @brief Inicializa el ADC para leer el termistor
 * @param dev Puntero a la estructura del termistor
 * @param unit Unidad ADC (ej. ADC_UNIT_1)
 * @param channel Canal ADC correspondiente al pin (Para GPIO2 en ESP32-C6 es ADC_CHANNEL_2)
 * @return ESP_OK si es exitoso
 */
esp_err_t termistor_init(termistor_t *dev, adc_unit_t unit, adc_channel_t channel);

/**
 * @brief Lee la temperatura en grados Celsius
 * @param dev Puntero a la estructura del termistor
 * @param out_temp Puntero donde se guardará la temperatura leída
 * @return ESP_OK si es exitoso
 */
esp_err_t termistor_read_celsius(termistor_t *dev, float *out_temp);

#endif // DRIVER_TERMISTOR_H