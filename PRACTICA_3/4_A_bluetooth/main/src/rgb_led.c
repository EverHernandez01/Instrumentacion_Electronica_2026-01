/**
 * @file rgb_led.c
 * @brief Control del LED RGB WS2812 via RMT.
 *
 * ¿Por qué RMT y no GPIO directo?
 * El WS2812 necesita pulsos de 0.4µs y 0.8µs con ±150ns de tolerancia.
 * Con GPIO + delays de software, FreeRTOS puede interrumpir el timing y
 * corromper la señal. RMT es un periférico dedicado que genera los pulsos
 * en hardware, sin intervención de la CPU una vez iniciada la transmisión.
 *
 * ¿Por qué led_strip y no RMT directamente?
 * El componente led_strip de Espressif abstrae el protocolo WS2812 sobre RMT.
 * Nos da funciones de alto nivel (set_pixel, refresh) sin manejar bits a mano.
 */

#include "rgb_led.h"
#include "led_strip.h"
#include "esp_log.h"

/* Pin GPIO del LED RGB en la ESP32-C6 */
#define RGB_LED_GPIO    15

/* Número de LEDs en la tira — en este caso solo hay 1 LED onboard */
#define RGB_LED_COUNT   1

static const char *TAG = "rgb_led";

/* Handle del controlador de la tira LED.
 * 'static' significa que esta variable solo existe dentro de este archivo.
 * app_main no puede acceder a ella directamente — debe usar las funciones
 * de la API pública (rgb_led_set, rgb_led_off). Esto es encapsulamiento. */
static led_strip_handle_t led_strip = NULL;

/* --------------------------------------------------------------------------
 * Inicialización
 * -------------------------------------------------------------------------- */
esp_err_t rgb_led_init(void)
{
    /* Configuración del controlador de la tira LED.
     *
     * led_model: WS2812 usa el protocolo de un solo hilo (single-wire).
     * max_leds:  cuántos LEDs hay en la tira.
     * strip_gpio_num: pin de datos.
     * rmt_config.clk_src: fuente de reloj para el periférico RMT.
     * rmt_config.resolution_hz: resolución del timer RMT — 10MHz da 100ns
     *   de precisión, suficiente para el protocolo WS2812. */
    led_strip_config_t strip_cfg = {
        .strip_gpio_num = RGB_LED_GPIO,
        .max_leds       = RGB_LED_COUNT,
        .led_model      = LED_MODEL_WS2812,
        /* El WS2812 recibe los bytes en orden G-R-B, no R-G-B.
         * El componente led_strip maneja esta conversión automáticamente. */
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_cfg = {
        .clk_src       = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, /* 10 MHz */
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Apagamos el LED al inicio para tener un estado conocido */
    led_strip_clear(led_strip);
    ESP_LOGI(TAG, "LED RGB WS2812 inicializado en GPIO %d", RGB_LED_GPIO);
    return ESP_OK;
}

/* --------------------------------------------------------------------------
 * Establece el color del LED
 * -------------------------------------------------------------------------- */
void rgb_led_set(uint8_t r, uint8_t g, uint8_t b)
{
    if (led_strip == NULL) {
        ESP_LOGW(TAG, "rgb_led_set llamado antes de rgb_led_init()");
        return;
    }

    /* set_pixel escribe el color en un buffer interno — todavía no cambia el LED.
     * El índice 0 es el primer (y único) LED de la tira. */
    led_strip_set_pixel(led_strip, 0, r, g, b);

    /* refresh() envía el buffer por RMT al LED físico.
     * Sin este paso, el LED no cambia. */
    led_strip_refresh(led_strip);

    ESP_LOGI(TAG, "LED → R:%d G:%d B:%d", r, g, b);
}

/* --------------------------------------------------------------------------
 * Apaga el LED
 * -------------------------------------------------------------------------- */
void rgb_led_off(void)
{
    if (led_strip == NULL) return;

    /* clear() pone todos los píxeles en (0,0,0) y hace el refresh automáticamente */
    led_strip_clear(led_strip);
    ESP_LOGI(TAG, "LED apagado");
}
