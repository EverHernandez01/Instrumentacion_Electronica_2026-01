#include "ven.h"

// Configuración base del PWM
#define VEN_LEDC_MODE       LEDC_LOW_SPEED_MODE
#define VEN_LEDC_DUTY_RES   LEDC_TIMER_8_BIT // Resolución de 8 bits (Valores de 0 a 255)
#define VEN_LEDC_FREQ_HZ    5000             // Frecuencia de 5kHz (Ideal para control de motores DC)

esp_err_t ven_init(ven_t *dev, gpio_num_t pin, float temp_start, float temp_max) {
    dev->pin = pin;
    dev->temp_start = temp_start;
    dev->temp_max = temp_max;
    dev->channel = LEDC_CHANNEL_0; // Usamos el canal 0 del LEDC
    dev->timer = LEDC_TIMER_0;
    dev->current_speed_pct = 0;

    // 1. Configurar el temporizador del PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = VEN_LEDC_MODE,
        .timer_num        = dev->timer,
        .duty_resolution  = VEN_LEDC_DUTY_RES,
        .freq_hz          = VEN_LEDC_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t err = ledc_timer_config(&ledc_timer);
    if (err != ESP_OK) return err;

    // 2. Configurar el pin físico y asignarle el temporizador
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = VEN_LEDC_MODE,
        .channel        = dev->channel,
        .timer_sel      = dev->timer,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = dev->pin,
        .duty           = 0, // Inicia totalmente apagado
        .hpoint         = 0
    };
    return ledc_channel_config(&ledc_channel);
}

void ven_update(ven_t *dev, float current_temp) {
    uint32_t duty = 0;

    // Determinar la potencia necesaria según la temperatura
    if (current_temp <= dev->temp_start) {
        // Está más frío que el punto de arranque -> Apagado
        duty = 0;
        dev->current_speed_pct = 0;
    } else if (current_temp >= dev->temp_max) {
        // Superó el límite máximo -> 100% de fuerza
        duty = 255;
        dev->current_speed_pct = 100;
    } else {
        // Zona intermedia (25°C - 30°C) -> Velocidad fija al 50%
        duty = 128;  // 50% de 255
        dev->current_speed_pct = 50;
    }

    // Aplicar el voltaje modulado (PWM) al hardware
    ledc_set_duty(VEN_LEDC_MODE, dev->channel, duty);
    ledc_update_duty(VEN_LEDC_MODE, dev->channel);
}