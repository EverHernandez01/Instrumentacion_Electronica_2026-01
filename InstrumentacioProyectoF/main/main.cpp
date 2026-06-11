#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <array>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_netif.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "driver/i2c.h"

extern "C" {
#include "ssd1306.h"
#include "term.h"
#include "bomb.h"
#include "ven.h"
}

#include <Espressif_MQTT_Client.h>
#include <Server_Side_RPC.h>
#include <ThingsBoard.h>

// --- CONFIGURACION WiFi ---
constexpr char WIFI_SSID[] = "PEDRO VASQUEZ  1";
constexpr char WIFI_PASSWORD[] = "R3sUNAL2023";

// --- CONFIGURACION ThingsBoard ---
constexpr char TOKEN[] = "MnCIBDPhkA3jXFXS8iVY";
constexpr char THINGSBOARD_SERVER[] = "192.168.1.25";
constexpr uint16_t THINGSBOARD_PORT = 1884U;
constexpr uint16_t MAX_MESSAGE_SEND_SIZE = 256U;
constexpr uint16_t MAX_MESSAGE_RECEIVE_SIZE = 256U;

// --- CONFIGURACION I2C (OLED) ---
#define I2C_MASTER_SDA_IO           22
#define I2C_MASTER_SCL_IO           21
#define I2C_MASTER_NUM              I2C_NUM_0
#define I2C_MASTER_FREQ_HZ          400000

// --- RPC ---
constexpr char RPC_SET_PWM[] = "setPWM";
constexpr char RPC_SET_BOMBILLA[] = "setBombilla";
constexpr uint8_t MAX_RPC_SUBSCRIPTIONS = 2U;
constexpr uint8_t MAX_RPC_RESPONSE = 2U;

// --- TELEMETRIA ---
constexpr char TEMPERATURE_KEY[] = "temperatura";
constexpr char BOMBILLA_KEY[] = "bombilla";
constexpr char VENTILADOR_KEY[] = "ventilador";

static const char *TAG = "MAIN";

// --- ESTRUCTURA DE CONTEXTO DEL SISTEMA ---
typedef struct {
    ssd1306_t     pantalla;
    termistor_t   sensor;
    bomb_t        bombillo;
    ven_t         ventilador;
} system_context_t;

// Puntero global usado por los callbacks RPC
static system_context_t *g_ctx = nullptr;

// --- INSTANCIAS ThingsBoard ---
Espressif_MQTT_Client<> mqttClient;
Server_Side_RPC<MAX_RPC_SUBSCRIPTIONS, MAX_RPC_RESPONSE> rpc;
const std::array<IAPI_Implementation*, 1U> apis = {
    &rpc
};
ThingsBoard tb(mqttClient, MAX_MESSAGE_RECEIVE_SIZE, MAX_MESSAGE_SEND_SIZE, Default_Max_Stack_Size, apis);

// --- SETPOINT & ESTADOS ---
static const float setpoint_temperatura = 24.0f;
static bool wifi_connected = false;
static bool rpc_subscribed = false;

// --- CALLBACKS WiFi ---
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_connected = false;
        ESP_LOGW(TAG, "WiFi desconectado. Reintentando...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        wifi_connected = true;
        ESP_LOGI(TAG, "WiFi conectado. IP obtenida.");
    }
}

// --- INICIALIZACION WiFi ---
static void wifi_init(void) {
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_WIFI_STA();
    esp_netif_t *netif = esp_netif_new(&netif_cfg);
    assert(netif);

    ESP_ERROR_CHECK(esp_netif_attach_wifi_station(netif));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_default_wifi_sta_handlers());
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_cfg = {};
    strncpy(reinterpret_cast<char*>(wifi_cfg.sta.ssid), WIFI_SSID, sizeof(wifi_cfg.sta.ssid));
    strncpy(reinterpret_cast<char*>(wifi_cfg.sta.password), WIFI_PASSWORD, sizeof(wifi_cfg.sta.password));

    ESP_LOGI(TAG, "Conectando a WiFi: %s...", wifi_cfg.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
}

// --- INICIALIZACION I2C (OLED) ---
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = I2C_MASTER_FREQ_HZ,
        },
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) return err;
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

// =========================================================================
// CALLBACKS RPC (Server-Side)
// =========================================================================

// RPC: setPWM(int 0..255) -> actualiza ciclo de trabajo del ventilador
static void rpc_setPWM(const JsonVariantConst &data, JsonDocument &response) {
    int pwm_value = data.as<int>();
    ESP_LOGI(TAG, "RPC setPWM recibido: %d", pwm_value);

    if (pwm_value < 0)   pwm_value = 0;
    if (pwm_value > 255) pwm_value = 255;

    if (g_ctx != nullptr) {
        uint32_t duty = (uint32_t)pwm_value;
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
        g_ctx->ventilador.current_speed_pct = (uint8_t)((pwm_value * 100) / 255);
    }
    response["result"] = "ok";
    response["pwm"]   = pwm_value;
}

// RPC: setBombilla(bool/int) -> enciende/apaga el bombillo
static void rpc_setBombilla(const JsonVariantConst &data, JsonDocument &response) {
    bool state = data.as<bool>();
    ESP_LOGI(TAG, "RPC setBombilla recibido: %s", state ? "ON" : "OFF");

    if (g_ctx != nullptr) {
        bomb_set_state(&(g_ctx->bombillo), state);
    }
    response["result"] = "ok";
    response["estado"]  = state;
}

// =========================================================================
// APP MAIN
// =========================================================================
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "[APP] Iniciando sistema de control de temperatura con ThingsBoard...");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    // --- 1. Inicializar NVS, red, eventos ---
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // --- 2. Asignacion dinamica del contexto del sistema ---
    system_context_t *ctx = (system_context_t *)malloc(sizeof(system_context_t));
    if (ctx == NULL) {
        ESP_LOGE(TAG, "Error critico: No hay memoria suficiente.");
        return;
    }
    g_ctx = ctx;

    // --- 3. Inicializar hardware local ---
    ESP_LOGI(TAG, "Iniciando hardware...");
    ESP_ERROR_CHECK(i2c_master_init());

    esp_err_t err = ssd1306_init(&(ctx->pantalla), I2C_MASTER_NUM, SSD1306_I2C_ADDR_0);
    if (err == ESP_OK) {
        ssd1306_clear(&(ctx->pantalla));
        ssd1306_draw_string(&(ctx->pantalla), 10, 10, "Sist. Control TB", 1);
        ssd1306_draw_string(&(ctx->pantalla), 10, 30, "Conectando WiFi...", 1);
        ssd1306_draw_rect(&(ctx->pantalla), 8, 8, 112, 40, 1);
        ssd1306_update(&(ctx->pantalla));
    }

    termistor_init(&(ctx->sensor), ADC_UNIT_1, ADC_CHANNEL_2);
    bomb_init(&(ctx->bombillo), (gpio_num_t)5, BOMB_MODE_HEATING, 24.5f, 1.0f);
    ven_init(&(ctx->ventilador), (gpio_num_t)4, 25.0f, 30.0f);

    // --- 4. Conectar WiFi ---
    wifi_init();

// --- 5. BUCLE PRINCIPAL (OPTIMIZADO PARA MQTT) ---
    float temperatura = 0.0;
    char buf_temp[32];
    char buf_bombillo[16];
    char buf_ventilador[16];
    
    uint32_t sensor_tick_ms = 0;
    uint8_t telemetry_tick = 0;

    while (1) {
        // --- PROCESAR mensajes MQTT entrantes (RPC) continuamente ---
        tb.loop();

        // --- RECONEXION WiFi ---
        if (!wifi_connected) {
            ESP_LOGW(TAG, "WiFi desconectado. Reintentando...");
            esp_wifi_connect();
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // --- RECONEXION ThingsBoard ---
        if (!tb.connected()) {
            // CRITICO: resetear suscripciones porque se pierden al desconectar MQTT
            rpc_subscribed = false;
            ESP_LOGI(TAG, "Conectando a ThingsBoard (%s:%d)...", THINGSBOARD_SERVER, THINGSBOARD_PORT);
            if (tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
                ESP_LOGI(TAG, "Conectado a ThingsBoard.");
                if (err == ESP_OK) {
                    ssd1306_clear(&(ctx->pantalla));
                    ssd1306_draw_string(&(ctx->pantalla), 10, 10, "TB Conectado!", 1);
                    ssd1306_update(&(ctx->pantalla));
                }
            } else {
                ESP_LOGW(TAG, "Fallo conexion ThingsBoard. Reintentando en 5s...");
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;
            }
        }

        // --- SUSCRIPCION RPC ---
        if (!rpc_subscribed) {
            const std::array<RPC_Callback, MAX_RPC_SUBSCRIPTIONS> callbacks = {
                RPC_Callback{ RPC_SET_PWM,      rpc_setPWM },
                RPC_Callback{ RPC_SET_BOMBILLA, rpc_setBombilla }
            };
            if (rpc.RPC_Subscribe(callbacks.begin(), callbacks.end())) {
                rpc_subscribed = true;
                ESP_LOGI(TAG, "RPC suscrito: setPWM, setBombilla");
            }
        }

        // --- EJECUTAR LOGICA DE SENSOR Y TELEMETRIA CADA 0.5 SEGUNDOS (500 ms) ---
        if (sensor_tick_ms >= 500) {
            sensor_tick_ms = 0; // Reiniciar contador

            if (termistor_read_celsius(&(ctx->sensor), &temperatura) == ESP_OK) {
                // --- LOGICA BOMBILLA CON HISTERESIS ---
                // Histeresis = 1.0°C → banda muerta entre setpoint-0.5 y setpoint+0.5
                // Por debajo de setpoint-0.5: ENCIENDE. Por encima de setpoint+0.5: APAGA.
                // Dentro de la banda: conserva el estado anterior.
                ctx->bombillo.setpoint = setpoint_temperatura;
                bomb_update(&(ctx->bombillo), temperatura);

                // --- LOGICA VENTILADOR (Etapas fijas segun SetPoint) ---
                uint32_t fan_duty;
                uint8_t fan_pct;
                if (temperatura <= setpoint_temperatura) {
                    fan_duty = 0;
                    fan_pct = 0;
                } else if (temperatura <= setpoint_temperatura + 2.0f) {
                    fan_duty = 128;
                    fan_pct = 50;
                } else if (temperatura <= setpoint_temperatura + 5.0f) {
                    fan_duty = 166;
                    fan_pct = 65;
                } else {
                    fan_duty = 217;
                    fan_pct = 85;
                }
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, fan_duty);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
                ctx->ventilador.current_speed_pct = fan_pct;

                // Formatear textos
                sprintf(buf_temp, "T:%.1f S:%.1f", temperatura, setpoint_temperatura);
                sprintf(buf_bombillo, "Luz: %s", ctx->bombillo.is_on ? "ON " : "OFF");
                sprintf(buf_ventilador, "Ven: %d%%", ctx->ventilador.current_speed_pct);

                printf("%s | %s | %s\n", buf_temp, buf_bombillo, buf_ventilador);

                // Actualizar OLED
                if (err == ESP_OK) {
                    ssd1306_clear(&(ctx->pantalla));
                    ssd1306_draw_rect(&(ctx->pantalla), 0, 0, 128, 64, 1);
                    ssd1306_draw_string(&(ctx->pantalla), 10, 12, buf_temp, 1);
                    ssd1306_draw_string(&(ctx->pantalla), 10, 32, buf_bombillo, 1);
                    ssd1306_draw_string(&(ctx->pantalla), 10, 48, buf_ventilador, 1);
                    ssd1306_update(&(ctx->pantalla));
                }

                // Enviar telemetria cada 2 segundos (temperatura + bombilla + ventilador)
                if (++telemetry_tick >= 2) {
                    telemetry_tick = 0;
                    int bomb_state = ctx->bombillo.is_on ? 1 : 0;
                    int ven_speed = ctx->ventilador.current_speed_pct;
                    bool ok = tb.sendTelemetryData(TEMPERATURE_KEY, temperatura)
                           && tb.sendTelemetryData(BOMBILLA_KEY, bomb_state)
                           && tb.sendTelemetryData(VENTILADOR_KEY, ven_speed);
                    if (ok) {
                        ESP_LOGI(TAG, "Telemetria: %.1f C | Luz: %s | Ven: %d%%",
                                 temperatura, bomb_state ? "ON" : "OFF", ven_speed);
                    } else {
                        ESP_LOGW(TAG, "Fallo al enviar telemetria");
                    }
                }
            } else {
                ESP_LOGE(TAG, "Error leyendo el termistor.");
            }
        }

        // --- Pequeño retraso de 50ms para no saturar el CPU y dejar que el MQTT respire ---
        vTaskDelay(pdMS_TO_TICKS(50));
        sensor_tick_ms += 50; 
    }

    free(ctx);
}
