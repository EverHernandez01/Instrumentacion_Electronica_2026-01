#include "term.h"
#include <math.h> // Necesario para la función log()
#include "esp_log.h"

static const char *TAG = "TERMISTOR";

// Función interna para configurar la calibración del ADC (convierte los valores raw a milivoltios reales)
static bool termistor_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) calibrated = true;
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibracion ADC inicializada con exito");
    } else {
        ESP_LOGW(TAG, "Calibracion ADC fallo, se usaran valores raw aproximados");
    }
    return calibrated;
}

esp_err_t termistor_init(termistor_t *dev, adc_unit_t unit, adc_channel_t channel) {
    dev->adc_channel = channel;
    
    // 1. Configurar la unidad ADC (OneShot)
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = unit,
    };
    esp_err_t err = adc_oneshot_new_unit(&init_config, &dev->adc_handle);
    if (err != ESP_OK) return err;

    // 2. Configurar el canal ADC
    // Atenuación de 11dB permite leer voltajes de hasta ~3.1V (Perfecto para 3.3V VCC)
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12, // Nota: En ESP-IDF v5.5 para C6 se usa DB_12 o DB_11 dependiendo de la macro
    };
    err = adc_oneshot_config_channel(dev->adc_handle, channel, &config);
    if (err != ESP_OK) return err;

    // 3. Inicializar la calibración
    dev->do_calibration = termistor_adc_calibration_init(unit, channel, ADC_ATTEN_DB_12, &dev->cali_handle);

    return ESP_OK;
}

esp_err_t termistor_read_celsius(termistor_t *dev, float *out_temp) {
    int adc_raw = 0;
    int voltage_mv = 0;

    // Leer valor crudo del ADC
    esp_err_t err = adc_oneshot_read(dev->adc_handle, dev->adc_channel, &adc_raw);
    if (err != ESP_OK) return err;

    // Convertir a voltaje real (milivoltios)
    if (dev->do_calibration) {
        adc_cali_raw_to_voltage(dev->cali_handle, adc_raw, &voltage_mv);
    } else {
        // Fallback matemático simple si la calibración por hardware falla
        voltage_mv = (adc_raw * 3300) / 4095;
    }

    // Evitar divisiones por cero si el voltaje lee 3.3V exactos o 0V (circuito abierto/corto)
    if (voltage_mv <= 0 || voltage_mv >= TERM_VCC_MV) {
        *out_temp = -999.0f; // Código de error de lectura
        return ESP_FAIL;
    }

    // Calcular la resistencia actual del termistor
    // Asume NTC a GND y R_series a VCC
    //float r_term = TERM_R_SERIES * ((float)voltage_mv / (TERM_VCC_MV - (float)voltage_mv));
    // ACTUALIZADO: NTC conectado a VCC (3.3V) y R_series a GND
    float r_term = TERM_R_SERIES * ((TERM_VCC_MV - (float)voltage_mv) / (float)voltage_mv);
    // Calcular temperatura usando la ecuación de Steinhart-Hart (método Beta)
    float steinhart;
    steinhart = r_term / TERM_NOMINAL_RES;               // (R/Ro)
    steinhart = log(steinhart);                          // ln(R/Ro)
    steinhart /= TERM_BETA_COEFF;                        // 1/B * ln(R/Ro)
    steinhart += 1.0f / (TERM_NOMINAL_TEMP + 273.15f);   // + (1/To)
    steinhart = 1.0f / steinhart;                        // Invertir
    steinhart -= 273.15f;                                // Convertir a Celsius

    *out_temp = steinhart;

    return ESP_OK;
}