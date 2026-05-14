#include "potentiometer.h"

#include "esp_log.h"
#include "esp_err.h"

#include "esp_adc/adc_oneshot.h"

// ======================================================
// ADC CHANNEL 0 --> GPIO0
// ESP32-C6
// ======================================================

#define POTENTIOMETER_ADC_UNIT       ADC_UNIT_1
#define POTENTIOMETER_ADC_CHANNEL    ADC_CHANNEL_0
#define POTENTIOMETER_ADC_ATTEN      ADC_ATTEN_DB_12

static const char *TAG = "POTENTIOMETER";

static adc_oneshot_unit_handle_t adc_handle;

// ======================================================
// INIT
// ======================================================

void potentiometer_init(void)
{
    ESP_LOGI(TAG, "Inicializando ADC");

    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = POTENTIOMETER_ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE
    };

    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(
            &init_config,
            &adc_handle
        )
    );

    adc_oneshot_chan_cfg_t channel_config = {
        .atten = POTENTIOMETER_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            POTENTIOMETER_ADC_CHANNEL,
            &channel_config
        )
    );

    ESP_LOGI(TAG, "ADC inicializado correctamente");
}

// ======================================================
// READ ADC
// ======================================================

uint16_t potentiometer_read(void)
{
    int adc_raw = 0;

    ESP_ERROR_CHECK(
        adc_oneshot_read(
            adc_handle,
            POTENTIOMETER_ADC_CHANNEL,
            &adc_raw
        )
    );

    return (uint16_t)adc_raw;
}