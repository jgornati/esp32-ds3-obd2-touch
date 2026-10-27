#pragma once

#include <stddef.h>
#include <stdint.h>

// Waveshare ESP32-S3-Touch-LCD-3.49B V2 (la plaqueta dice Rev1.1), según los
// ejemplos de Waveshare para la V2 (ESP32-S3-Touch-LCD-3.49-V2,
// 10_LVGL_V9_Test/user_config.h) y su esquemático. En la V1 cambian la luz y
// el reset de la pantalla (notas de E11 y E15).

// Pantalla: AXS15231B por QSPI (SPI3_HOST), 172x640
constexpr uint8_t PIN_PANTALLA_CS = 9;
constexpr uint8_t PIN_PANTALLA_CLK = 10;
constexpr uint8_t PIN_PANTALLA_D0 = 11;
constexpr uint8_t PIN_PANTALLA_D1 = 12;
constexpr uint8_t PIN_PANTALLA_D2 = 13;
constexpr uint8_t PIN_PANTALLA_D3 = 14;
// El reset de la pantalla es el pin 5 del TCA9554. El GPIO21 es LCD_TE (una
// salida de la pantalla) y el GPIO8, la interrupción del TCA9554: no se usan.
constexpr uint8_t PIN_EXPANSOR_RESET_PANTALLA = 5;

// LVGL dibuja en horizontal (640x172) y pantalla_hw gira la imagen. true la
// gira 180° más, para montarla con el USB-C del otro lado.
constexpr bool PANTALLA_GIRADA = false;

// Retroiluminación (LCD_BL del esquemático), con PWM: en bajo, al máximo; en
// alto, apagada (como el programa de fábrica: duty = 255 - brillo). El
// regulador se prende con el pin 1 del TCA9554 (BL_EN). El brillo va en
// niveles, de 1 a CANT_NIVELES_BRILLO, guardado en Preferences; arranca en
// el más alto.
constexpr uint8_t PIN_RETROILUMINACION = 42;
constexpr uint8_t PIN_EXPANSOR_LUZ = 1;
constexpr uint32_t FRECUENCIA_RETROILUMINACION_HZ = 50000; // la del ejemplo
// De 0 a 255, por nivel. Medido en el banco (2026-10-27): con 90 o menos la
// luz se apaga (con 97 se ve), igual a 5 que a 50 kHz. Los eligió el
// usuario: 255 el máximo, 200 el anterior y parejos hasta 100.
constexpr uint8_t BRILLOS[] = {100, 133, 167, 200, 255};
constexpr uint8_t CANT_NIVELES_BRILLO = sizeof(BRILLOS) / sizeof(BRILLOS[0]);

// Touch, en el mismo AXS15231B. I2C sin INT ni RST.
constexpr uint8_t PIN_TOUCH_SDA = 17;
constexpr uint8_t PIN_TOUCH_SCL = 18;
constexpr uint8_t DIRECCION_TOUCH = 0x3B;

// I2C de la placa. Se inicia antes que la pantalla (placa.h), con el
// TCA9554; el RTC todavía no se usa.
constexpr uint8_t PIN_I2C_SDA = 47;
constexpr uint8_t PIN_I2C_SCL = 48;
constexpr uint8_t DIRECCION_RTC = 0x51;      // PCF85063
constexpr uint8_t DIRECCION_IMU = 0x6B;      // QMI8658
constexpr uint8_t DIRECCION_EXPANSOR = 0x20; // TCA9554; su pin 6 sostiene la batería
constexpr uint8_t DIRECCION_CODEC = 0x18;    // ES8311, la salida al parlante

// Audio (E11): el ES8311 por I2S, y el amplificador NS4150B, que se prende
// con el pin 7 del TCA9554 (NS_MODE). Es igual en la V1 y la V2 de la placa.
constexpr uint8_t PIN_I2S_MCLK = 7;
constexpr uint8_t PIN_I2S_BCLK = 15;
constexpr uint8_t PIN_I2S_WS = 46;
constexpr uint8_t PIN_I2S_SALIDA = 45; // DSDIN del ES8311
constexpr uint8_t PIN_EXPANSOR_AMPLIFICADOR = 7;

// IMU (E12): el acelerómetro del QMI8658, leído por una tarea cada
// PERIODO_IMU_MS. Sus interrupciones (pines 2 y 3 del TCA9554) no se usan.
// EJE_DERECHA_IMU es el eje del sensor (x, y, z) que va a lo largo de la
// pantalla, hacia la derecha, con PANTALLA_GIRADA = false (girada, el
// contrario). Sin verificar en la placa: con la placa parada sobre su lado
// izquierdo (el derecho para arriba), !imu da ~+1 g en ese eje.
constexpr float EJE_DERECHA_IMU[3] = {1, 0, 0};
constexpr uint32_t PERIODO_IMU_MS = 20;
// Las vibraciones: un filtro de primer orden con esta constante de tiempo
constexpr uint32_t FILTRO_FUERZA_G_MS = 300;
// "Abajo" se calibra con la placa quieta QUIETA_FUERZA_G_MS (cada muestra a
// menos de TOLERANCIA_QUIETA_G del promedio): al arrancar y en cada parada
// (la ECU en 0 km/h)
constexpr uint32_t QUIETA_FUERZA_G_MS = 2000;
constexpr float TOLERANCIA_QUIETA_G = 0.05f;
// Sin muestras por más que esto, "--"
constexpr uint32_t VIGENCIA_FUERZA_G_MS = 500;
// Con FUERZA G a la vista, la página se redibuja cada esto (las demás, cada
// PERIODO_PANTALLA_MS). Con 50, moviendo el punto, las consultas bajaban a 3
// o 4 por segundo (registro de E13)
constexpr uint32_t PERIODO_FUERZA_G_MS = 100;

// microSD (E13), en modo SD de 1 bit, como el ejemplo 04_SD_Card de la V2.
// D3 (GPIO38, el CS del modo SPI) tiene su resistencia a 3,3 V y no se toca;
// la tarjeta no tiene detección (CD va a masa): se busca al prender.
constexpr uint8_t PIN_SD_CLK = 41;
constexpr uint8_t PIN_SD_CMD = 39;
constexpr uint8_t PIN_SD_D0 = 40;
// Registro del viaje: una línea cada PERIODO_LINEA_MS, que espera en un búfer
// de TAM_BUFER_REGISTRO bytes (~50 s de líneas). La tarea de la tarjeta la
// escribe cada PERIODO_REGISTRO_MS: si se corta la luz, se pierde eso.
constexpr uint32_t PERIODO_LINEA_MS = 1000;
constexpr uint32_t PERIODO_REGISTRO_MS = 5000;
constexpr size_t TAM_BUFER_REGISTRO = 8192;

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
// Página Info: el RSSI se lee cada PERIODO_RSSI_MS mientras se ve. Olvidar
// adaptador se activa después de MANTENER_OLVIDAR_MS apretado.
constexpr uint32_t PERIODO_RSSI_MS = 2000;
constexpr uint32_t MANTENER_OLVIDAR_MS = 3000;
// Pantalla de espera: el DS3 entra a los ESPERA_ENTRADA_MS de prender la
// pantalla (si para entonces llegaron los datos, no se ve). Tocarla
// reintenta ya; mantenerla apretada MANTENER_REINICIO_MS reinicia la placa.
constexpr uint32_t ESPERA_ENTRADA_MS = 0;
constexpr uint32_t MANTENER_REINICIO_MS = 3000;
// Apretada menos que esto, la espera o la página Viaje es un toque: no
// aparece la barra de mantener apretado
constexpr uint32_t AVISO_MANTENER_MS = 500;
// Cambiar de página (E16): deslizar el dedo de costado DESLIZAR_PX o más.
// Tocar no cambia de página. Mantener apretada Viaje se cancela si el dedo se
// corre más de QUIETO_PX.
constexpr int32_t DESLIZAR_PX = 80;
constexpr int32_t QUIETO_PX = 20;
// Página Viaje: el motor está en marcha desde RPM_EN_MARCHA (ralentí
// 890-1190), y el voltaje mínimo y máximo cuentan después de
// ESPERA_VOLTAJE_VIAJE_MS en marcha, sin la caída del arranque. Mantener
// apretada la página MANTENER_VIAJE_MS reinicia el viaje.
constexpr float RPM_EN_MARCHA = 500;
constexpr uint32_t ESPERA_VOLTAJE_VIAJE_MS = 5000;
constexpr uint32_t MANTENER_VIAJE_MS = 3000;

// Alarmas (E11). Cada una se prende con el valor en el umbral o más allá
// durante el tiempo indicado, contado entre lecturas, y se apaga con el
// valor en "normal" o más acá el mismo tiempo. Sin un valor actual se
// apaga. El voltaje, solo con el motor en marcha desde hace
// ESPERA_VOLTAJE_VIAJE_MS. Con varias, se alternan cada PERIODO_ALARMAS_MS.
// ALARMAS_DE_PRUEBA pone umbrales que saltan manejando normal (motor tibio,
// 13,6-14,1 V en marcha, 2000 rpm), para probarlas en el auto.
constexpr bool ALARMAS_DE_PRUEBA = false;
constexpr float REFRIGERANTE_ALARMA = ALARMAS_DE_PRUEBA ? 40 : 105; // °C
constexpr float REFRIGERANTE_NORMAL = ALARMAS_DE_PRUEBA ? 37 : 102;
constexpr uint32_t REFRIGERANTE_ALARMA_MS = 5000;
constexpr float VOLTAJE_BAJO_ALARMA = ALARMAS_DE_PRUEBA ? 14.5f : 13.0f; // V
constexpr float VOLTAJE_BAJO_NORMAL = ALARMAS_DE_PRUEBA ? 14.7f : 13.2f;
constexpr float VOLTAJE_ALTO_ALARMA = ALARMAS_DE_PRUEBA ? 13.5f : 15.0f;
constexpr float VOLTAJE_ALTO_NORMAL = ALARMAS_DE_PRUEBA ? 13.3f : 14.8f;
constexpr uint32_t VOLTAJE_ALARMA_MS = 10000;
// La zona roja de la barra de Motor
constexpr float RPM_ALARMA = ALARMAS_DE_PRUEBA ? 2000 : 6000;
constexpr float RPM_NORMAL = ALARMAS_DE_PRUEBA ? 1800 : 5800;
constexpr uint32_t PERIODO_ALARMAS_MS = 3000;
// Al prenderse cada alarma: PITIDOS_ALARMA pitidos de PITIDO_MS, separados
// por SILENCIO_PITIDO_MS, a FRECUENCIA_PITIDO_HZ y VOLUMEN_PITIDO (de 0 a 1)
constexpr uint8_t PITIDOS_ALARMA = 3;
constexpr uint32_t PITIDO_MS = 150;
constexpr uint32_t SILENCIO_PITIDO_MS = 100;
constexpr uint32_t FRECUENCIA_PITIDO_HZ = 2500;
constexpr float VOLUMEN_PITIDO = 0.5f;
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
