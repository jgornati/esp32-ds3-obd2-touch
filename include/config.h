#pragma once

#include <stdint.h>

// Waveshare ESP32-S3-Touch-LCD-3.49, según los ejemplos de Waveshare
// (10_LVGL_V9_Test/user_config.h). En E0 solo se usa la retroiluminación.

// Pantalla: AXS15231B por QSPI (SPI3_HOST), 172x640
constexpr uint8_t PIN_PANTALLA_CS = 9;
constexpr uint8_t PIN_PANTALLA_CLK = 10;
constexpr uint8_t PIN_PANTALLA_D0 = 11;
constexpr uint8_t PIN_PANTALLA_D1 = 12;
constexpr uint8_t PIN_PANTALLA_D2 = 13;
constexpr uint8_t PIN_PANTALLA_D3 = 14;
constexpr uint8_t PIN_PANTALLA_RST = 21;

// LVGL dibuja en horizontal (640x172) y pantalla_hw gira la imagen. true la
// gira 180° más, para montarla con el USB-C del otro lado.
constexpr bool PANTALLA_GIRADA = false;

// Retroiluminación, con PWM. Los ejemplos de Waveshare usan duty = 255 - brillo
// (lcd_bl_pwm_bsp.h), pero en E0 con HIGH quedó prendida. Se mide en E1.
constexpr uint8_t PIN_RETROILUMINACION = 8;
constexpr bool RETROILUMINACION_INVERTIDA = true; // duty = 255 - brillo
constexpr uint8_t BRILLO_INICIAL = 128;           // 0-255
constexpr uint8_t PASO_BRILLO = 16;               // + y - en la consola

// Touch, en el mismo AXS15231B. I2C sin INT ni RST.
constexpr uint8_t PIN_TOUCH_SDA = 17;
constexpr uint8_t PIN_TOUCH_SCL = 18;
constexpr uint8_t DIRECCION_TOUCH = 0x3B;

// I2C de la placa. No se inicializa hasta que una etapa lo pida.
constexpr uint8_t PIN_I2C_SDA = 47;
constexpr uint8_t PIN_I2C_SCL = 48;
constexpr uint8_t DIRECCION_RTC = 0x51;      // PCF85063
constexpr uint8_t DIRECCION_IMU = 0x6B;      // QMI8658
constexpr uint8_t DIRECCION_EXPANSOR = 0x20; // TCA9554; su pin 6 sostiene la batería

// GPIO16 no se usa: el ejemplo 07 lo lee para saber si está prendida por batería.

// BLE
constexpr const char *BLE_NOMBRE_LOCAL = "DS3-Monitor";
// Lo que marca un dispositivo como posible adaptador OBD: una parte del
// nombre (sin distinguir mayúsculas) o un servicio anunciado.
constexpr const char *BLE_NOMBRES_OBD[] = {"OBD",    "ELM",  "VGATE",   "VLINK",
                                           "V-LINK", "ICAR", "VEEPEAK", "KONNWEI"};
constexpr const char *BLE_SERVICIOS_OBD[] = {"FFF0", "FFE0", "18F0",
                                             "E7810A71-73AE-499D-8C15-FAA9AEF0C3F2"};

// Tiempos
constexpr uint32_t BUSQUEDA_BLE_S = 10;        // !escanear (la biblioteca la toma en segundos)
constexpr uint32_t TIMEOUT_CONSOLA_MS = 10000; // comandos escritos a mano
// Al arrancar, para no perder el informe si el monitor serie todavía no abrió el USB
constexpr uint32_t ESPERA_MONITOR_MS = 2000;
