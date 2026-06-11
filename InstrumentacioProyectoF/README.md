# Sistema de Control de Temperatura - ESP32-C6

Este proyecto utiliza un ESP32-C6 para leer un termistor NTC y ejecutar un control de temperatura con histéresis mediante un actuador ON/OFF (bombillo) y un ventilador de velocidad variable (PWM). Toda la información se visualiza en una pantalla OLED SSD1306.

## Requisitos
* **Framework:** ESP-IDF v5.x (Probado en v5.5.1)
* **Target:** ESP32-C6

## Pines Físicos
* **I2C SDA (OLED):** GPIO 22
* **I2C SCL (OLED):** GPIO 21
* **Termistor (ADC):** GPIO 2 (ADC1_CHANNEL_2)
* **Bombillo (Relé/Transistor):** GPIO 5
* **Ventilador (PWM):** GPIO 4

## Cómo clonar y compilar

1. Clona este repositorio:
   ```bash
   git clone [https://github.com/TuUsuario/TuRepositorio.git](https://github.com/TuUsuario/TuRepositorio.git)