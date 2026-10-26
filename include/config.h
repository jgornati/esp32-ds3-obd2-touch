#pragma once

#include <stdint.h>

// Waveshare ESP32-S3-Touch-LCD-3.49, según los ejemplos de Waveshare
// (10_LVGL_V9_Test/user_config.h).

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

// Retroiluminación, con PWM. Como en los ejemplos de Waveshare
// (lcd_bl_pwm_bsp.h), duty = 255 - brillo (confirmado en E1).
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

// Botón BOOT, atrás de la placa: pasa a la página siguiente. Es pin de
// arranque; después del boot se lee como cualquier entrada (apretado = LOW).
constexpr uint8_t PIN_BOTON = 0;

// BLE
constexpr const char *BLE_NOMBRE_LOCAL = "DS3-Monitor";
// Lo que !escanear marca como posible adaptador OBD: una parte del nombre
// (sin distinguir mayúsculas) o un servicio anunciado.
constexpr const char *BLE_NOMBRES_OBD[] = {"OBD",    "ELM",  "VGATE",   "VLINK",
                                           "V-LINK", "ICAR", "VEEPEAK", "KONNWEI"};
constexpr const char *BLE_SERVICIOS_OBD[] = {"FFF0", "FFE0", "18F0",
                                             "E7810A71-73AE-499D-8C15-FAA9AEF0C3F2"};

// El adaptador del DS3 (E0). Sin dirección guardada se busca por el nombre
// o por el servicio.
constexpr const char *BLE_NOMBRE_REMOTO = "OBDBLE";
constexpr const char *BLE_SERVICIO_ELM = "FFF0";
constexpr const char *BLE_NOTIFICACION_ELM = "FFF1";
constexpr const char *BLE_ESCRITURA_ELM = "FFF2";
// Parámetros de conexión que se piden al conectar. Con los de la biblioteca
// (50 ms) un comando AT tardaba 70-390 ms (E0).
constexpr uint16_t BLE_INTERVALO_MIN = 6;  // x 1,25 ms = 7,5 ms
constexpr uint16_t BLE_INTERVALO_MAX = 12; // 15 ms
constexpr uint16_t BLE_SUPERVISION = 200;  // x 10 ms = 2 s: lo que tarda en notar una pérdida

// Tiempos en ms
constexpr uint32_t BUSQUEDA_BLE_S = 10; // !escanear (la biblioteca la toma en segundos)
constexpr uint32_t BUSQUEDA_ELM_S = 6;  // al conectar, si la dirección guardada no anduvo
// Por intento de conexión. La biblioteca esperaría 30 s.
constexpr uint32_t TIMEOUT_CONEXION_MS = 4000;
constexpr uint32_t TIMEOUT_COMANDO_MS = 1000; // AT y PIDs comunes
constexpr uint32_t TIMEOUT_ATZ_MS = 3000;
constexpr uint32_t TIMEOUT_BUSCAR_ECU_MS = 10000; // 0100 al buscar la ECU
constexpr uint32_t TIMEOUT_CONSOLA_MS = 10000;    // comandos escritos a mano
constexpr uint32_t REINTENTO_BT_MS = 5000;        // entre intentos de conexión
constexpr uint32_t REINTENTO_ECU_MS = 10000;
constexpr uint32_t REINTENTO_ELM_MS = 5000; // si el ELM no contesta la inicialización
constexpr uint32_t PERIODO_ATRV_MS = 2000;
// Datos que no están en pantalla. Menos que VIGENCIA_MS para no vencerse entre consultas.
constexpr uint32_t PERIODO_LENTO_MS = 2500;
constexpr uint32_t PERIODO_PANTALLA_MS = 500;
constexpr uint32_t PERIODO_TABLA_MS = 1000; // tabla de datos en el monitor serie
constexpr uint32_t ANTIRREBOTE_MS = 30;     // el botón tiene que quedar estable este tiempo
// Página Fallas: se lee al entrar y después cada PERIODO_FALLAS_MS, o antes
// si 03 o 07 no contestaron bien. Si hay más códigos de los que entran, se
// alternan cada PERIODO_GRUPO_FALLAS_MS.
constexpr uint32_t PERIODO_FALLAS_MS = 30000;
constexpr uint32_t REINTENTO_FALLAS_MS = 5000;
constexpr uint32_t PERIODO_GRUPO_FALLAS_MS = 3000;
// Al arrancar, para no perder el informe si el monitor serie todavía no abrió el USB
constexpr uint32_t ESPERA_MONITOR_MS = 2000;
// Desde el arranque hasta prender el BLE, para que la alimentación se
// estabilice. Si el arranque anterior terminó en brownout, se espera más.
constexpr uint32_t ESPERA_ANTES_DE_BT_MS = 1000;
constexpr uint32_t ESPERA_TRAS_BROWNOUT_MS = 3000;

// Fallas seguidas de consultas a la ECU en LEYENDO para pasar a SIN_ECU,
// con SILENCIO_PARA_SIN_ECU_MS sin ninguna respuesta buena
constexpr uint8_t FALLAS_PARA_SIN_ECU = 5;
// ATRV sin respuesta seguidos para volver a inicializar el ELM
constexpr uint8_t FALLAS_PARA_REINICIAR_ELM = 3;
// Intentos de cada consulta al encontrar la ECU (los mapas de PIDs y la
// prueba de varios PIDs): con el adaptador nuevo hay NO DATA sueltos
constexpr uint8_t INTENTOS_AL_ENCONTRAR = 3;
// PIDs por consulta cuando la ECU acepta pedidos de varios (hasta 6). El
// adaptador nuevo tarda lo mismo por consulta pida uno o varios (E3).
constexpr uint8_t PIDS_POR_CONSULTA = 6;
// Respuestas seguidas a las que les falta algún PID para volver a pedir de a uno
constexpr uint8_t INCOMPLETAS_PARA_DE_A_UNO = 3;

// Un dato más viejo que esto se muestra como "--"
constexpr uint32_t VIGENCIA_MS = 3000;
// Con el adaptador nuevo hubo 5 NO DATA seguidos con el motor en marcha
// (E3b). Igual a VIGENCIA_MS: al pasar a SIN_ECU ya no queda ningún dato vigente.
constexpr uint32_t SILENCIO_PARA_SIN_ECU_MS = VIGENCIA_MS;
