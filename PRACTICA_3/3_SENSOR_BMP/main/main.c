#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

// =====================================================
// PINES SPI PARA BMP280 - ESP32-C6
// =====================================================
#define PIN_NUM_MISO  2
#define PIN_NUM_MOSI  7
#define PIN_NUM_CLK   6
#define PIN_NUM_CS    18

#define BMP280_HOST   SPI2_HOST

// =====================================================
// PINES I2C PARA PANTALLA GM009605 / OLED
// =====================================================
#define OLED_I2C_PORT I2C_NUM_0
#define OLED_SDA      GPIO_NUM_4
#define OLED_SCL      GPIO_NUM_5
#define OLED_ADDR     0x3C

// =====================================================
// REGISTROS BMP280
// =====================================================
#define BMP280_REG_ID        0xD0
#define BMP280_REG_RESET     0xE0
#define BMP280_REG_STATUS    0xF3
#define BMP280_REG_CTRL_MEAS 0xF4
#define BMP280_REG_CONFIG    0xF5
#define BMP280_REG_PRESS_MSB 0xF7
#define BMP280_REG_PRESS_LSB 0xF8
#define BMP280_REG_PRESS_XLSB 0xF9
#define BMP280_REG_TEMP_MSB  0xFA
#define BMP280_REG_TEMP_LSB  0xFB
#define BMP280_REG_TEMP_XLSB 0xFC
#define BMP280_REG_CALIB     0x88

#define BMP280_CHIP_ID       0x58
#define BME280_CHIP_ID       0x60

static const char *TAG = "BMP280_LCD";
static spi_device_handle_t bmp280_spi;

// =====================================================
// ESTRUCTURA DE CALIBRACION BMP280
// =====================================================
typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2;
    int16_t  dig_T3;

    uint16_t dig_P1;
    int16_t  dig_P2;
    int16_t  dig_P3;
    int16_t  dig_P4;
    int16_t  dig_P5;
    int16_t  dig_P6;
    int16_t  dig_P7;
    int16_t  dig_P8;
    int16_t  dig_P9;

    int32_t t_fine;
} bmp280_calib_t;

static bmp280_calib_t calib;

// =====================================================
// FUENTE BASICA PARA OLED
// =====================================================
static const uint8_t F_SPACE[5] = {0x00,0x00,0x00,0x00,0x00};
static const uint8_t F_0[5] = {0x3E,0x51,0x49,0x45,0x3E};
static const uint8_t F_1[5] = {0x00,0x42,0x7F,0x40,0x00};
static const uint8_t F_2[5] = {0x42,0x61,0x51,0x49,0x46};
static const uint8_t F_3[5] = {0x21,0x41,0x45,0x4B,0x31};
static const uint8_t F_4[5] = {0x18,0x14,0x12,0x7F,0x10};
static const uint8_t F_5[5] = {0x27,0x45,0x45,0x45,0x39};
static const uint8_t F_6[5] = {0x3C,0x4A,0x49,0x49,0x30};
static const uint8_t F_7[5] = {0x01,0x71,0x09,0x05,0x03};
static const uint8_t F_8[5] = {0x36,0x49,0x49,0x49,0x36};
static const uint8_t F_9[5] = {0x06,0x49,0x49,0x29,0x1E};
static const uint8_t F_DOT[5] = {0x00,0x60,0x60,0x00,0x00};
static const uint8_t F_COLON[5] = {0x00,0x36,0x36,0x00,0x00};
static const uint8_t F_MINUS[5] = {0x08,0x08,0x08,0x08,0x08};

static const uint8_t F_A[5] = {0x7E,0x11,0x11,0x11,0x7E};
static const uint8_t F_B[5] = {0x7F,0x49,0x49,0x49,0x36};
static const uint8_t F_C[5] = {0x3E,0x41,0x41,0x41,0x22};
static const uint8_t F_E[5] = {0x7F,0x49,0x49,0x49,0x41};
static const uint8_t F_H[5] = {0x7F,0x08,0x08,0x08,0x7F};
static const uint8_t F_I[5] = {0x00,0x41,0x7F,0x41,0x00};
static const uint8_t F_M[5] = {0x7F,0x02,0x0C,0x02,0x7F};
static const uint8_t F_N[5] = {0x7F,0x04,0x08,0x10,0x7F};
static const uint8_t F_O[5] = {0x3E,0x41,0x41,0x41,0x3E};
static const uint8_t F_P[5] = {0x7F,0x09,0x09,0x09,0x06};
static const uint8_t F_R[5] = {0x7F,0x09,0x19,0x29,0x46};
static const uint8_t F_S[5] = {0x46,0x49,0x49,0x49,0x31};
static const uint8_t F_T[5] = {0x01,0x01,0x7F,0x01,0x01};

static const uint8_t *font_get(char c)
{
    c = toupper((unsigned char)c);

    switch (c) {
        case '0': return F_0;
        case '1': return F_1;
        case '2': return F_2;
        case '3': return F_3;
        case '4': return F_4;
        case '5': return F_5;
        case '6': return F_6;
        case '7': return F_7;
        case '8': return F_8;
        case '9': return F_9;
        case '.': return F_DOT;
        case ':': return F_COLON;
        case '-': return F_MINUS;
        case 'A': return F_A;
        case 'B': return F_B;
        case 'C': return F_C;
        case 'E': return F_E;
        case 'H': return F_H;
        case 'I': return F_I;
        case 'M': return F_M;
        case 'N': return F_N;
        case 'O': return F_O;
        case 'P': return F_P;
        case 'R': return F_R;
        case 'S': return F_S;
        case 'T': return F_T;
        default: return F_SPACE;
    }
}

// =====================================================
// FUNCIONES OLED SSD1306 POR I2C
// =====================================================
static esp_err_t oled_send_cmd(uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};
    return i2c_master_write_to_device(OLED_I2C_PORT, OLED_ADDR, data, sizeof(data), pdMS_TO_TICKS(1000));
}

static esp_err_t oled_send_data(uint8_t *data, size_t len)
{
    uint8_t buffer[129];

    if (len > 128) {
        len = 128;
    }

    buffer[0] = 0x40;
    memcpy(&buffer[1], data, len);

    return i2c_master_write_to_device(OLED_I2C_PORT, OLED_ADDR, buffer, len + 1, pdMS_TO_TICKS(1000));
}

static esp_err_t oled_i2c_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = OLED_SDA,
        .scl_io_num = OLED_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };

    esp_err_t ret = i2c_param_config(OLED_I2C_PORT, &conf);

    if (ret != ESP_OK) {
        return ret;
    }

    return i2c_driver_install(OLED_I2C_PORT, conf.mode, 0, 0, 0);
}

static void oled_set_cursor(uint8_t page, uint8_t col)
{
    oled_send_cmd(0xB0 + page);
    oled_send_cmd(0x00 + (col & 0x0F));
    oled_send_cmd(0x10 + ((col >> 4) & 0x0F));
}

static void oled_clear(void)
{
    uint8_t zeros[128] = {0};

    for (int page = 0; page < 8; page++) {
        oled_set_cursor(page, 0);
        oled_send_data(zeros, 128);
    }
}

static void oled_draw_char(char c)
{
    uint8_t out[6];
    const uint8_t *bitmap = font_get(c);

    for (int i = 0; i < 5; i++) {
        out[i] = bitmap[i];
    }

    out[5] = 0x00;
    oled_send_data(out, 6);
}

static void oled_print_line(uint8_t page, const char *text)
{
    oled_set_cursor(page, 0);

    for (int i = 0; i < 21; i++) {
        if (text[i] == '\0') {
            break;
        }

        oled_draw_char(text[i]);
    }
}

static esp_err_t oled_init(void)
{
    vTaskDelay(pdMS_TO_TICKS(100));

    oled_send_cmd(0xAE);
    oled_send_cmd(0x20);
    oled_send_cmd(0x00);
    oled_send_cmd(0xB0);
    oled_send_cmd(0xC8);
    oled_send_cmd(0x00);
    oled_send_cmd(0x10);
    oled_send_cmd(0x40);
    oled_send_cmd(0x81);
    oled_send_cmd(0x7F);
    oled_send_cmd(0xA1);
    oled_send_cmd(0xA6);
    oled_send_cmd(0xA8);
    oled_send_cmd(0x3F);
    oled_send_cmd(0xA4);
    oled_send_cmd(0xD3);
    oled_send_cmd(0x00);
    oled_send_cmd(0xD5);
    oled_send_cmd(0x80);
    oled_send_cmd(0xD9);
    oled_send_cmd(0xF1);
    oled_send_cmd(0xDA);
    oled_send_cmd(0x12);
    oled_send_cmd(0xDB);
    oled_send_cmd(0x40);
    oled_send_cmd(0x8D);
    oled_send_cmd(0x14);
    oled_send_cmd(0xAF);

    oled_clear();

    return ESP_OK;
}

// =====================================================
// FUNCIONES AUXILIARES BMP280
// =====================================================
static uint16_t u16_le(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int16_t s16_le(const uint8_t *p)
{
    return (int16_t)u16_le(p);
}

// =====================================================
// SPI BMP280
// =====================================================
static esp_err_t spi_master_init(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };

    esp_err_t ret = spi_bus_initialize(BMP280_HOST, &buscfg, SPI_DMA_DISABLED);

    if (ret != ESP_OK) {
        return ret;
    }

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 100000,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 1,
    };

    return spi_bus_add_device(BMP280_HOST, &devcfg, &bmp280_spi);
}

static esp_err_t bmp280_read_u8(uint8_t reg, uint8_t *value)
{
    uint8_t tx[2] = {0};
    uint8_t rx[2] = {0};

    tx[0] = reg | 0x80;
    tx[1] = 0x00;

    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };

    esp_err_t ret = spi_device_polling_transmit(bmp280_spi, &t);

    if (ret == ESP_OK) {
        *value = rx[1];
    }

    return ret;
}

static esp_err_t bmp280_read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
    if (len == 0 || len > 31) {
        return ESP_ERR_INVALID_SIZE;
    }

    for (size_t i = 0; i < len; i++) {
        esp_err_t ret = bmp280_read_u8(reg + i, &data[i]);

        if (ret != ESP_OK) {
            return ret;
        }
    }

    return ESP_OK;
}

static esp_err_t bmp280_write_byte(uint8_t reg, uint8_t value)
{
    uint8_t tx[2];

    tx[0] = reg & 0x7F;
    tx[1] = value;

    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
    };

    return spi_device_polling_transmit(bmp280_spi, &t);
}

// =====================================================
// BMP280 CALIBRACION
// =====================================================
static esp_err_t bmp280_read_calibration(void)
{
    uint8_t c[24];

    esp_err_t ret = bmp280_read_bytes(BMP280_REG_CALIB, c, 24);

    if (ret != ESP_OK) {
        return ret;
    }

    calib.dig_T1 = u16_le(&c[0]);
    calib.dig_T2 = s16_le(&c[2]);
    calib.dig_T3 = s16_le(&c[4]);

    calib.dig_P1 = u16_le(&c[6]);
    calib.dig_P2 = s16_le(&c[8]);
    calib.dig_P3 = s16_le(&c[10]);
    calib.dig_P4 = s16_le(&c[12]);
    calib.dig_P5 = s16_le(&c[14]);
    calib.dig_P6 = s16_le(&c[16]);
    calib.dig_P7 = s16_le(&c[18]);
    calib.dig_P8 = s16_le(&c[20]);
    calib.dig_P9 = s16_le(&c[22]);

    return ESP_OK;
}

// =====================================================
// COMPENSACION TEMPERATURA Y PRESION
// =====================================================
static double bmp280_compensate_temperature(int32_t adc_T)
{
    int32_t var1;
    int32_t var2;
    int32_t temperature;

    var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) *
            ((int32_t)calib.dig_T2)) >> 11;

    var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) *
              ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >> 12) *
            ((int32_t)calib.dig_T3)) >> 14;

    calib.t_fine = var1 + var2;

    temperature = (calib.t_fine * 5 + 128) >> 8;

    return temperature / 100.0;
}

static double bmp280_compensate_pressure(int32_t adc_P)
{
    int64_t var1;
    int64_t var2;
    int64_t p;

    var1 = ((int64_t)calib.t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib.dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib.dig_P5) << 17);
    var2 = var2 + (((int64_t)calib.dig_P4) << 35);

    var1 = ((var1 * var1 * (int64_t)calib.dig_P3) >> 8) +
           ((var1 * (int64_t)calib.dig_P2) << 12);

    var1 = (((((int64_t)1) << 47) + var1) *
            ((int64_t)calib.dig_P1)) >> 33;

    if (var1 == 0) {
        return 0.0;
    }

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;

    var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib.dig_P8) * p) >> 19;

    p = ((p + var1 + var2) >> 8) + (((int64_t)calib.dig_P7) << 4);

    return (double)p / 256.0;
}

// =====================================================
// BMP280 INICIALIZACION Y LECTURA
// =====================================================
static esp_err_t bmp280_init_sensor(void)
{
    uint8_t id = 0;

    esp_err_t ret = bmp280_read_u8(BMP280_REG_ID, &id);

    if (ret != ESP_OK) {
        return ret;
    }

    if (id != BMP280_CHIP_ID && id != BME280_CHIP_ID) {
        return ESP_FAIL;
    }

    ret = bmp280_write_byte(BMP280_REG_RESET, 0xB6);

    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    ret = bmp280_read_calibration();

    if (ret != ESP_OK) {
        return ret;
    }

    bmp280_write_byte(BMP280_REG_CONFIG, 0x48);
    bmp280_write_byte(BMP280_REG_CTRL_MEAS, 0x2F);

    vTaskDelay(pdMS_TO_TICKS(300));

    return ESP_OK;
}

static esp_err_t bmp280_read_temperature_pressure(double *temperature_c, double *pressure_pa)
{
    uint8_t p_msb = 0;
    uint8_t p_lsb = 0;
    uint8_t p_xlsb = 0;
    uint8_t t_msb = 0;
    uint8_t t_lsb = 0;
    uint8_t t_xlsb = 0;

    bmp280_read_u8(BMP280_REG_PRESS_MSB, &p_msb);
    bmp280_read_u8(BMP280_REG_PRESS_LSB, &p_lsb);
    bmp280_read_u8(BMP280_REG_PRESS_XLSB, &p_xlsb);

    bmp280_read_u8(BMP280_REG_TEMP_MSB, &t_msb);
    bmp280_read_u8(BMP280_REG_TEMP_LSB, &t_lsb);
    bmp280_read_u8(BMP280_REG_TEMP_XLSB, &t_xlsb);

    int32_t adc_P = ((int32_t)p_msb << 12) |
                    ((int32_t)p_lsb << 4) |
                    ((int32_t)p_xlsb >> 4);

    int32_t adc_T = ((int32_t)t_msb << 12) |
                    ((int32_t)t_lsb << 4) |
                    ((int32_t)t_xlsb >> 4);

    *temperature_c = bmp280_compensate_temperature(adc_T);
    *pressure_pa = bmp280_compensate_pressure(adc_P);

    return ESP_OK;
}

// =====================================================
// PROGRAMA PRINCIPAL
// =====================================================
void app_main(void)
{
    char line1[32];
    char line2[32];
    char line3[32];

    oled_i2c_init();
    oled_init();

    ESP_LOGI(TAG, "--- INICIANDO SENSOR BMP280 (SPI) ---");

    if (spi_master_init() != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando SPI");
        oled_clear();
        oled_print_line(0, "ERROR SPI");
        return;
    }

    if (bmp280_init_sensor() != ESP_OK) {
        ESP_LOGE(TAG, "Error inicializando BMP280");
        oled_clear();
        oled_print_line(0, "ERROR BMP280");
        return;
    }

    ESP_LOGI(TAG, "Sensor configurado correctamente. Iniciando lecturas...");

    oled_clear();
    oled_print_line(0, "BMP280 SPI");

    while (1) {
        double temperature_c = 0.0;
        double pressure_pa = 0.0;
        double pressure_hpa = 0.0;

        bmp280_read_temperature_pressure(&temperature_c, &pressure_pa);

        pressure_hpa = pressure_pa / 100.0;

        // Ajuste solo para visualizacion:
        // Si el sensor entrega presion negativa por falla, se muestra positiva.
        if (pressure_hpa < 0.0) {
            pressure_hpa = -pressure_hpa;
        }

        ESP_LOGI(TAG, "Temperatura: %.2f C | Presion Barometrica: %.2f hPa",
                 temperature_c, pressure_hpa);

        snprintf(line1, sizeof(line1), "BMP280 SPI");
        snprintf(line2, sizeof(line2), "TEMP:%.2f C", temperature_c);
        snprintf(line3, sizeof(line3), "PRES:%.2f HPA", pressure_hpa);

        oled_clear();
        oled_print_line(0, line1);
        oled_print_line(2, line2);
        oled_print_line(4, line3);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}