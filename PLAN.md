# Plan de trabajo — Monitor OBDII DS3 (pantalla táctil)

Es el mismo monitor de `../esp32-ds3-obd2`, llevado a la placa Waveshare
ESP32-S3-Touch-LCD-3.49 y a un adaptador OBD por BLE. El README dice
**qué** hace el proyecto. Este archivo dice **cómo** y **en qué orden**.
Cada etapa está pensada para que la haga un agente distinto, empezando
sin contexto previo.

Para arrancar una etapa alcanza con pedirle al agente:

> Hacé la Etapa EN de PLAN.md.

Al terminar, el agente actualiza la tabla de estado, la sección
**Datos descubiertos** (si aprendió algo del hardware) y su sección en
**Notas de traspaso**. Una etapa pasa a *Verificada* solo cuando el
usuario confirma la prueba en el hardware.

El proyecto anterior está en `../esp32-ds3-obd2`. Su `PLAN.md` explica
por qué se hizo cada cosa (notas de traspaso de E0 a E7): leer la parte
que corresponda antes de portar un módulo.

## Estado

| Etapa | Tema                                   | Depende de | Estado    |
| ----- | -------------------------------------- | ---------- | --------- |
| E0    | Base del proyecto y exploración BLE    | —          | Verificada |
| E1    | Pantalla y touch                       | E0         | Verificada |
| E2    | Transporte BLE con el ELM              | E0         | Verificada |
| E3    | El monitor completo en el auto         | E1, E2     | Verificada |
| E4    | Diseño de páginas y navegación táctil  | E3         | Verificada |
| E5    | Códigos de falla                       | E4         | Verificada |
| E6    | Brillo y ajustes                       | E4         | Verificada (sin brillo: la placa no lo regula) |
| E7    | Cierre e instalación                   | E5, E6     | Verificada (salvo el cuelgue ocasional, sin investigar) |
| E8    | Conexión al prender y apagar el auto   | E7         | Descartada |
| E9    | Pantalla de espera con el DS3          | E7         | Verificada |
| E10   | Viaje: máximos y recorrido             | E9         | Hecha (falta verificar en hardware) |
| E11   | Alarmas                                | E9         | Hecha (el parlante pita; faltan las alarmas en el auto) |
| E12   | Fuerza G                               | E10        | Hecha (falta verificar en hardware) |
| E13   | Registro del viaje en la microSD       | E10        | Verificada |
| E14   | Reloj                                  | —          | Verificada |
| E15   | La placa V2: pines de la pantalla y brillo | E11    | Verificada (salvo que el brillo se mantenga al apagar y prender) |
| E16   | Cambiar de página deslizando el dedo   | E4         | Verificada |
| E17   | Pantalla fluida: que dibujar no frene  | E16        | Verificada (salvo FUERZA G con el punto moviéndose y el `LCD_TE`; en DS3, ~1 consulta menos por segundo) |
| E19   | Micrófono y parlante: grabar y hablar  | E11        | Descartada (se hizo y anduvo, pero el parlante se oye bajo; se sacó) |
| E20   | Asistente de voz por OpenRouter        | E19        | Descartada |
| E21   | WhatsApp por el celular                | E20        | Descartada |
| E22   | Viajes anteriores, también sin conexión | E10       | Hecha (falta verificar en hardware) |
| E23   | Que no se cuelgue al prender y apagar el auto | —    | Hecha (falta verificar en hardware) |
| E24   | Pantalla principal: RPM, marcha, refrigerante, velocidad y voltaje | E22 | Hecha (falta verificar en hardware) |
| E25   | Aviso de cambio y umbrales del refrigerante | E24   | Pendiente |

Estados posibles: Pendiente, En curso, Hecha (falta verificar en
hardware), Verificada, Descartada.

E1 y E2 tocan archivos distintos y se pueden hacer en paralelo. E5 y E6
también. De E8 en adelante son pedidos del usuario del 2026-10-26, después
de usarlo en el auto. E8 se descartó (ver la etapa): se sigue por E9. E10
y E11 se pueden hacer en cualquier orden. E14 necesita la batería puesta
(ver la etapa). E15 la pidió el usuario el 2026-10-26, al saberse en E11 que
la placa es la V2; se hizo antes que E12. E16 la pidió el usuario el
2026-10-27, después de usarlo en el auto. E17 es la optimización que pidió
el mismo día. No hay E18: era leer la ECU por CAN directo, sin el ELM, y el
usuario la sacó del plan el 2026-10-27 (la va a hacer en otro proyecto). E19
a E21 son el asistente de voz que pidió también ese día: preguntarle cosas
en voz alta y, más adelante, que lea y mande mensajes de WhatsApp.

**El plan está terminado** (decisión del usuario, 2026-10-27). E19 se hizo y
anduvo en la placa, pero el parlante se oye bajo: el usuario la descartó y se
sacó el código (no quedó en ningún commit; ver notas de E19). Con eso se
descartan E20 y E21. No hay más etapas (después se sumaron E22 a E25, abajo).
Las que dicen *Hecha* o *Verificada
(salvo ...)* quedan así: lo que les falta probar está en **Pendiente de
probar**, abajo. Lo que sigue, según el usuario: probarlo y optimizarlo (las
ideas están en **Ideas para después**) y, después, mejorar la interfaz para que
se vea más lindo.

E22 la pidió el usuario el mismo 2026-10-27, después del cierre: que sin
conexión se pueda ir a la página DS3 y ver los datos de viajes pasados.

E23 a E25 las pidió el usuario el 2026-10-28, después de ir de Funes a
Rosario: que no se cuelgue más al prender y apagar el auto (hoy hay que
desenchufarlo), una pantalla principal con la marcha y un reloj del
refrigerante, sacar AIRE/VOLT, un aviso de cambio configurable y la alarma
del refrigerante más alta. Van en ese orden: E23 es lo que más molesta, y E25
usa la marcha de E24. Lo que se vio en los registros de la microSD que pasó el
usuario (`VIAJE0003` a `VIAJE0008`, del 2026-10-27) está en **Datos
descubiertos**.

## Pendiente de probar

Lo que las etapas dejaron sin probar en el hardware, según sus notas (las
pruebas detalladas están en las **Pruebas del usuario** de cada una). Al
probar algo, anotarlo en la etapa y sacarlo de acá.

**Para la próxima prueba** (la lista para el 2026-10-29, con E23, el arreglo de
la fuerza G y E24 en `main`; en orden):

En el banco, con la Mac y sin el adaptador:

1. Subir el firmware de `main` y desenchufar la placa.
2. **E23:** enchufarla a la Mac **sin abrir el monitor**: el DS3 entra entero
   y tocar o deslizar responde enseguida (antes se trababa ~1 min con la
   trompa asomando). Probarlo 2 o 3 veces.
3. **E23:** abrir el monitor y `!info`: `Ultimo reinicio`, `Volcado del ultimo
   panico: ninguno` y la RAM interna libre. Después `!trabar`: a los ~10 s la
   placa se reinicia sola, y `!info` dice `Ultimo reinicio: guardian (...)`.
4. **E23:** `!escanear`: durante los 10 s la espera sigue respondiendo y la
   placa no se reinicia.
5. **E22:** desde la espera, deslizar a ANTERIORES y a DS3, sin demora.

En el auto:

6. **E23:** al abrir la puerta, el DS3 entra entero; con la llave, llega a
   MOTOR. Unos días sin desenchufar la placa.
7. **E24, MOTOR:** la marcha coincide con la palanca en las 5 y da `-` con el
   embrague apretado o en punto muerto; las luces suben con las RPM; el reloj
   del refrigerante, en blanco hasta 110 °C; el voltaje.
8. **E24, MEZCLA:** con el motor frío dice `sin regular` en gris, y al cerrarse
   el lazo los valores se mueven.
9. **Fuerza G:** al acelerar la bolita va para abajo, al frenar para arriba y
   en una curva a la derecha, a la izquierda.
10. Con la notebook en el auto, si se puede: `!info` con el motor andando (RAM
    interna, `LCD_TE N barridos/s`) y las consultas por segundo en MOTOR (6–7).
11. Al volver: pasar los CSV de la microSD a la Mac (la columna `Marcha`
    contra lo que se vio, y el `Inicio de la placa` de cada uno).

La alarma del refrigerante sigue en 105 °C hasta E25 (va a seguir saltando
con el motor regulando en 103–108).

Lo demás, por etapa:

En el auto, con el motor andando y la microSD:

- **Alarmas (E11):** con `ALARMAS_DE_PRUEBA = true`, que cada una salte y se
  vaya cuando corresponde, con sus tres pitidos y la barra roja; con `false`,
  que no salte ninguna en un viaje normal, y que los pitidos se oigan con el
  motor andando. `!pitar` ya suena bien (con `VOLUMEN_PITIDO` en 1). En los
  registros del 2026-10-27, con `false`, `REFRIGERANTE` saltó y se apagó bien,
  pero en cada viaje caliente (el motor regula en 103–108 °C): E25 la sube a
  110 °C. `ZONA ROJA` saltó una vez, a 6004 rpm, y se apagó al segundo.
- **Viaje (E10):** la distancia contra el parcial del tablero en un viaje
  andando, y los máximos contra lo que se vio. Mantener apretada VIAJE 3 s la
  reinicia; menos, no. Sacar la llave y volver a ponerla: empieza de cero. (En
  los registros de E13 el tiempo en marcha ya contaba; la distancia daba 0,
  con el auto quieto.)
- **Fuerza G (E12):** el punto hacia el lado que corresponde al frenar y en las
  curvas, y que recalibre en las paradas (`!info`, las calibraciones suben).
  Que el IMU contesta y calibra quieto ya se vio en E13. **El signo estaba al
  revés:** el usuario vio que al acelerar la bolita iba para adelante, y en los
  registros del 2026-10-27 la `G long.` sigue a la derivada de la velocidad con
  el signo cambiado (correlación −0,97, pendiente −1,07, en 1700 s). El eje es
  el correcto y la derecha es −x: desde el 2026-10-28,
  `EJE_DERECHA_IMU = {-1, 0, 0}` (era `{1, 0, 0}`, una suposición), que da
  vuelta la lateral y la longitudinal juntas (adelante sale de arriba por la
  derecha). Falta verlo en el auto: al acelerar la bolita va para abajo, y en
  una curva a la derecha, a la izquierda.
- **FUERZA G a 50 ms (E17):** con el punto moviéndose (andando, o moviendo la
  placa con la mano), ~20 cuadros/s y las consultas en 6–7. Si bajan, volver
  a 100 (`git revert 7176bbd`).
- **Reloj (E14):** después de unas semanas, cuánto se corrió (la columna `Hora`
  y la fecha de los archivos ya se vieron en los registros del 2026-10-27).
- **Brillo (E15):** que el nivel se mantenga al apagar y prender, y verlo de
  día en el auto (el nivel 1, 100, queda cerca del corte de 90–97).
- **Viajes anteriores (E22):** ver sus **Pruebas del usuario**, en las notas
  de E22. Lo principal: que un viaje aparezca en ANTERIORES al volver a poner
  el contacto, y que sin el adaptador (llave en accesorios, o en el banco) se
  pueda pasar deslizando de la espera a ANTERIORES y a DS3 sin demora.
- **`!info` con el motor andando:** la RAM interna libre (la última medida fue
  74 KB en el banco, sin el adaptador; en E5, 128 KB en el auto) y `LCD_TE N
  barridos/s` (E17: si el TE llega).

- **Pantalla principal (E24):** ver sus **Pruebas del usuario**, en las notas
  de E24. Lo principal: que la marcha coincida con la palanca en las 5 y dé
  `-` con el embrague apretado, y las consultas por segundo en MOTOR (6–7).
- **Sin cuelgues (E23):** ver sus **Pruebas del usuario**, en las notas de
  E23. Lo principal: enchufada a la Mac sin el monitor, que el DS3 entre sin
  trabarse; `!trabar` reinicia la placa a los ~10 s; y unos días en el auto
  sin desenchufarla.

El cuelgue ocasional al prender y apagar el auto (E7; E8, que lo iba a
investigar, se descartó) se encontró y se arregló en E23.

## Qué se reutiliza del proyecto anterior

| Del proyecto anterior                         | Acá                                                        |
| --------------------------------------------- | ---------------------------------------------------------- |
| `lib/obd/` y `test/` (45 tests)               | Se copian sin cambios (E0). Es la lógica probada con el DS3. |
| `.clang-format`                               | Igual (E0).                                                |
| `consola`, `datos`, `sondeo`, `descubrimiento` | Se portan casi sin cambios (E2, E3).                       |
| `main.cpp` (máquina de estados)               | Se porta (E3). Salen los LEDs, la cámara, el botón y el detector de brownout desactivado. |
| `elm327`                                      | Misma interfaz; por dentro pasa de Bluetooth clásico a BLE (E2). |
| `pantalla`, `paginas`, `boton`                | Se reescriben para la pantalla color y el touch (E1, E3, E4). |
| **Datos descubiertos** del DS3                | Valen: es el mismo auto. Se reconfirman con el adaptador nuevo en E3. |

## Decisiones tomadas

Prevalecen sobre el README si hay diferencias. Las marcadas
*propuesta* las confirma el usuario antes de la etapa que las usa.

| Tema                   | Decisión                                                                                       |
| ---------------------- | ---------------------------------------------------------------------------------------------- |
| Alcance                | Solo lectura, como el anterior. Todo comando pasa por `comandoPermitido()`; la lista blanca no se amplía sin pedido del usuario. El 2026-10-25 el usuario pidió permitir el servicio 01 con hasta 6 PIDs por pedido (`010C0D050F`), que sigue siendo lectura (ver notas de E3). |
| Bluetooth              | **Solo BLE.** El ESP32-S3 no tiene Bluetooth clásico, así que el ELM anterior (`OBDII`, SPP) no sirve con esta placa. Se usa el adaptador BLE nuevo. |
| Plataforma             | `espressif32 @ 8.0.0` (Arduino-ESP32 3.3.x), la misma del anterior. Waveshare pide Arduino-ESP32 3.1.0 o más nuevo. |
| Placa en PlatformIO    | No hay definición para la 3.49. Se usa `esp32-s3-devkitc-1` con lo que Waveshare indica para el Arduino IDE: flash 16 MB QIO 80 MHz, PSRAM OPI (`qio_opi`), USB CDC al arrancar, particiones `app3M_fat9M_16MB.csv`. Un programa de prueba con BLE, `esp_lcd` e `i2c_master` compiló así (2026-10-24); falta subirlo a la placa. |
| Subir el firmware      | Por el USB-C, sin adaptador USB–TTL ni puente GPIO0–GND. Si no entra en modo de descarga: mantener BOOT, apretar RESET, soltar BOOT. |
| Biblioteca BLE         | *Propuesta:* la `BLE` del core (viene con la plataforma, sin dependencia nueva). Si da problemas, NimBLE-Arduino con versión fijada. E0 ya la usa: en el S3 corre sobre NimBLE, no Bluedroid (ver notas de E0). |
| Gráficos               | *Propuesta:* LVGL 9.3.0 (la versión de los ejemplos de Waveshare) y el driver `esp_lcd_axs15231b` del ejemplo `10_LVGL_V9_Test`. |
| Orientación            | Horizontal, 640×172, con rotación por software como en el ejemplo de Waveshare. Montada como en el banco: `PANTALLA_GIRADA = false` en `config.h` (confirmado en E7). |
| Concurrencia           | La lógica sigue en un solo `loop()`. Las notificaciones BLE llegan desde la tarea del stack y se pasan a un buffer. LVGL es de `loop()`: `lv_timer_handler()` se llama desde `loop()` y mientras se espera una respuesta del ELM. Desde E9, mientras `loop()` bloquea conectando el BLE (hasta 30 s), una tarea de `PantallaHw` atiende LVGL, con un mutex: así las animaciones no se traban y mantener apretado para reiniciar anda aunque la conexión se cuelgue (ver notas de E9). Desde E12, una tarea en el núcleo 0 lee el IMU cada 20 ms, con un mutex para la cuenta (ver notas de E12). Desde E13, otra en el núcleo 0 escribe en la microSD cada 5 s lo que `loop()` deja en un búfer (ver notas de E13). Desde E17, otra en el núcleo 0 gira y manda cada imagen al panel, sin tocar LVGL, mientras LVGL dibuja la siguiente en otra imagen (ver notas de E17). Desde E23, un guardián (prioridad 5, en cualquier núcleo) reinicia la placa si LVGL pasa 10 s sin atenderse o una imagen no se termina de mandar; `setup()` suelta LVGL mientras inicia el sonido, el IMU, la tarjeta y el BLE, y la consola mientras hace `!escanear` y `!explorar` (ver notas de E23). |
| Optimización           | Arduino-ESP32 3.3 compila el proyecto y las bibliotecas sin `-O` (GCC en `-O0`; el ESP-IDF viene ya compilado a `-Os`). Desde E17, `optimizar.py` sube a `-O2` solo LVGL y `src/giro.cpp`, que son lo que cuesta dibujar: lo eligió el usuario. Lo demás queda como se probó |
| Protocolo              | `ATSP6` (CAN 11 bit 500 kbaud), confirmado en el DS3. Heredada.                                |
| Sufijo de cantidad     | `01PP1`. Heredada; se reconfirma con el adaptador nuevo en E3.                                 |
| Fallas                 | Solo OBD genérico (motor y emisiones). Heredada.                                               |
| Alimentación           | 5 V desde un USB del auto que se corta con la llave. Sin deep sleep (confirmado en E7). El 2026-10-26 el usuario decidió ponerle una batería de 3,7 V, para que el RTC no pierda la hora (ver **Hardware de la placa**). La placa se sigue apagando con la llave: el firmware no activa `SYS_EN` |
| Alimentación del adaptador | Con el contacto, desde el pin 1 del conector OBD (en el DS3 se corta con la llave), no desde el 16 (permanente). Lo modificó el usuario el 2026-10-26: le sacó la pata 16 y puenteó la 1 al pad. Con la llave sacada el adaptador se apaga y la placa queda en `SIN ADAPTADOR` (hasta E5, `SIN CONEXION` en rojo), no en `SIN ECU`. |
| Brillo                 | En 5 niveles, con - y + en Info, guardado en `Preferences` (E15). Hasta E15 quedaba siempre al máximo: el firmware usaba el pin de la luz de la V1 (GPIO8) y la placa es la V2 (GPIO42). |
| Imágenes del DS3       | Las animaciones que trajo el usuario (2026-10-26, hechas aparte): un DS3 rojo con techo negro que entra a la pantalla de espera, y otro andando con nubes y camino para una página al final. Las fuentes, en `herramientas/imagenes/fuentes/` |
| Viajes anteriores      | Desde E22, un resumen de cada viaje (lo de VIAJE y cuándo empezó) en la flash, con `Preferences`, no en la microSD: los últimos 10 que llegan a 1 min en marcha. Se ven en ANTERIORES, después de VIAJE, también sin conexión. Lo eligió el usuario (2026-10-27) |
| Hardware sin usar      | No inicializar microSD, audio, IMU, RTC ni el expansor TCA9554 hasta que una etapa lo pida. Los piden E11 (audio y TCA9554), E12 (IMU), E13 (microSD) y E14 (RTC). Desde E11 se inician el I2C de la placa y el audio, y desde E15 se usan **solo los pines 1 (luz), 5 (reset de la pantalla) y 7 (amplificador)** del TCA9554; los demás no se tocan. Desde E12 se inicia el IMU, solo el acelerómetro y sin sus interrupciones (EXIO2 y EXIO3 no se tocan). Desde E13 se monta la microSD, en modo SD de 1 bit. Desde E14 se lee el RTC al arrancar, sin su interrupción (EXIO4 no se toca). |

## Hardware de la placa

Según los ejemplos de Waveshare (`waveshareteam/ESP32-S3-Touch-LCD-3.49`,
`Examples/Arduino/10_LVGL_V9_Test/user_config.h`). Se verifica en E0 y E1.
Son los de la **V1**. La placa del usuario es la **3.49B V2** (`Rev1.1`, con
caja, batería y parlante): lo que cambia en la V2 está en cada fila, según
los ejemplos y el esquemático de `waveshareteam/ESP32-S3-Touch-LCD-3.49-V2`
(visto en E11).

| Parte                                  | Detalle                                                    |
| -------------------------------------- | ---------------------------------------------------------- |
| Chip                                   | ESP32-S3R8: 2 núcleos, 240 MHz, 8 MB de PSRAM (OPI), 16 MB de flash, Wi-Fi y BLE 5 |
| Pantalla                               | IPS 3,49", 172×640, controlador AXS15231B por QSPI (`SPI3_HOST`) |
| Pines de la pantalla                   | CS 9, CLK 10, D0–D3 11–14, RST 21. **V2:** el GPIO21 es `LCD_TE` (una salida de la pantalla) y el reset es EXIO5 del TCA9554. El firmware usa los de la V2 desde E15, y lee el `LCD_TE` desde E17 (los ejemplos de Waveshare no lo usan) |
| Retroiluminación                       | GPIO8 (`LCD_BL`) al regulador AP3032; activa en bajo, como en los ejemplos (`lcd_bl_pwm_bsp.h`). En esta placa no cambia la luz (E6): queda en bajo, al máximo. El encendido del regulador es EXIO1 del TCA9554 (`BL_EN`). **V2:** `LCD_BL` es el GPIO42, y el GPIO8 es `EXIO_INT` (la interrupción del TCA9554): por eso no cambiaba. Igual que en la V1, en bajo queda al máximo (el programa de fábrica pone duty = 255 − brillo). El firmware usa el GPIO42 desde E15 |
| Touch                                  | Capacitivo, en el mismo AXS15231B. I2C en SDA 17 / SCL 18, dirección `0x3B`, sin INT ni RST |
| I2C de la placa                        | SDA 47 / SCL 48: RTC PCF85063 (`0x51`), IMU QMI8658 (`0x6B`), expansor TCA9554 (`0x20`) y el códec de audio. Un solo bus para todos |
| TCA9554 (expansor)                     | Según el esquemático (E6): EXIO0 `TP_INT`, EXIO1 `BL_EN` (encendido de la luz, con resistencia a `VSYS`), EXIO2 y EXIO3 interrupciones del IMU, EXIO4 `RTC_INT`, EXIO5 `LCD_TE` (**V2:** `LCD_RST`), EXIO6 `SYS_EN` (mantiene la alimentación desde la batería, ejemplo `07_BATT_PWR_Test`) y EXIO7 `NS_MODE` (el amplificador del parlante, ejemplo `08_Audio_Test`) |
| RTC                                    | PCF85063 alimentado por dos diodos B5819WS: D3 desde `3V3` y otro desde `VBAT` (la batería), con C32 de 1 µF en su VDD (pata 10). **Con la batería puesta, sigue andando con la placa apagada**; sin batería, pierde la hora en cada apagado. (Corregido el 2026-10-26: las notas de E6 decían que solo tenía la entrada de 3,3 V) |
| IMU                                    | QMI8658A en `0x6B` del I2C de la placa: acelerómetro y giróscopo. Sus dos salidas de interrupción van a EXIO2 y EXIO3 del TCA9554. El datasheet está en el repositorio de Waveshare de la V2 (`Arduino/libraries/SensorLib/datasheet/`). Cómo está puesto en la placa (qué eje va a lo largo de la pantalla) no lo dicen ni los ejemplos ni el esquemático: se mide con `!imu` (E12) |
| microSD                                | SPI: CS 38, MOSI 39, MISO 40, SCLK 41. Son las patas de la tarjeta en modo SD: D3, CMD, D0 y CLK. Las cuatro tienen 10 kΩ a 3,3 V; D1 y D2 no van a ningún GPIO, y `CD` (detección) va a masa. El ejemplo `04_SD_Card` de la V2 la usa en modo SD de 1 bit (CLK 41, CMD 39, D0 40), y el firmware también (E13) |
| Audio                                  | Códec ES8311 (salida al parlante, `0x18`) y ES7210 (micrófonos) por I2S: MCLK 7, SCLK 15, LRCK 46; la placa manda el audio por el 45 (`DSDIN` del ES8311) y lo recibe por el 6 (corregido en E11: decía al revés); control por el I2C de la placa. Amplificador NS4150B, prendido con EXIO7 (`NS_MODE`) en alto. Igual en la V1 y la V2 |
| Batería                                | Una celda de 3,7 V en el conector J1 (MX1.25 de 2 pines; mirar la polaridad antes de enchufar). Protección DW01 + 8205 y cargador ETA6098, que la carga desde el USB. Sin el USB, alimenta a la placa solo con PWR apretado o con `SYS_EN` (EXIO6) en alto, por Q3; al RTC, siempre. Su tensión se lee en GPIO4 (`BAT_ADC`), por un divisor de 200 kΩ y 100 kΩ que gasta ~14 µA todo el tiempo. Ningún GPIO lee `VBUS` |
| GPIO16                                 | `SYS_OUT`: el botón PWR (baja al apretarlo). El ejemplo 07 lo lee. No usar. |
| Botones                                | BOOT (GPIO0), PWR y RESET, atrás                           |
| Otros                                  | Conector de batería 3,7 V y 22 pines de expansión |

## Arquitectura

```
include/
  config.h          pines, BLE, tiempos, rotación
  lv_conf.h         configuración de LVGL (E1)
src/
  main.cpp          exploración (E0); máquina de estados (E3)
  explorar.h/.cpp   !escanear, !explorar (E0; !usar salió en E2)
  elm327.h/.cpp     transporte BLE con la interfaz del anterior (E2)
  consola.h/.cpp    consola por el USB (E0, E2)
  pantalla_hw.h/.cpp  QSPI, rotación, touch y puerto de LVGL (E1); la tarea que lo atiende mientras se conecta (E9); la tarea que manda la imagen y el LCD_TE (E17); el guardián (E23)
  giro.h/.cpp       pasa la imagen a vertical para el panel, sin Arduino ni LVGL, a -O2 (E17)
  pantalla.h/.cpp   pantalla de espera (E9), pantallas de estado (E3) y el marco de las páginas (E4)
  paginas.h/.cpp    páginas y navegación táctil (E4)
  dibujo.h/.cpp     colores y funciones chicas de LVGL de pantalla y paginas (E4)
  animaciones.h/.cpp  el DS3 que entra a la espera y el de la página DS3 (E9)
  llanta.h/.cpp     gira las llantas, sin LVGL (E9)
  imagenes/         las imágenes de las animaciones, generadas (E9)
  boton.h/.cpp      botón BOOT: página siguiente (E4)
  sonido.h/.cpp     el parlante: I2C de la placa, ES8311, I2S y el amplificador (E11)
  placa.h/.cpp      el I2C de la placa y el TCA9554, antes que la pantalla (E15)
  brillo.h/.cpp     el nivel de brillo, guardado en Preferences (E15)
  imu.h/.cpp        el acelerómetro del QMI8658, leído por una tarea (E12)
  tarjeta.h/.cpp    la microSD y la tarea que escribe el registro (E13)
  rtc.h/.cpp        el reloj PCF85063, que pone la hora del sistema (E14)
  guardados.h/.cpp  los viajes anteriores en la flash, con Preferences (E22)
  datos, sondeo, descubrimiento   portados del anterior (E3)
lib/obd/src/        igual que el anterior: lógica pura, SIN Arduino.h; más viaje (E10), alarmas (E11), fuerza G (E12), registro (E13), reloj (E14), viajes anteriores (E22) y la marcha (E24)
lib/axs15231b/      driver del panel, del ejemplo de Waveshare (E1)
src/fuentes/         números grandes generados con lv_font_conv (E1)
test/               igual que el anterior, más lo nuevo de lib/obd
herramientas/render/  las pantallas en la PC, a PNG (E4)
herramientas/imagenes/  las animaciones a arreglos de C, y sus fuentes (E9)
docs/descubrimiento/  logs crudos de las sesiones en el auto
```

Reglas de límites entre módulos (las mismas del anterior):

- `elm327` **no interpreta** el contenido OBD. Devuelve el texto crudo
  recibido antes del `>` y un estado de transporte.
- `lib/obd` **no depende de Arduino** y se testea con
  `pio test -e native`.
- Todo comando pasa por `comandoPermitido()` dentro de
  `Elm327::enviar()`. No existe otro camino para escribirle al ELM.
- Lo de LVGL queda en `pantalla_hw`, `pantalla`, `paginas`, `dibujo` y
  `animaciones`. La máquina de estados no llama a LVGL directamente.

### Estados del programa

Los mismos del anterior:

```
INICIO → CONECTANDO_BT → INICIANDO_ELM → BUSCANDO_ECU → LEYENDO
                ↑                              ↓           ↓
                └──────── BT_PERDIDO ←─────  SIN_ECU ←─────┘
```

### Convenciones de pantalla

Las mismas del anterior; cambia solo cómo se ven.

| Situación                                 | Se muestra   |
| ----------------------------------------- | ------------ |
| Dato válido y actual                      | valor + unidad |
| La ECU no soporta el dato                 | `N/D`        |
| Todavía no llegó la primera respuesta     | `...`        |
| Dato vencido (más de `VIGENCIA_MS`)       | `--`         |
| Bluetooth perdido                         | barra: `SIN ADAPTADOR` en gris, sin datos (E6) |

Nunca mostrar un número vencido. Con color, además, un dato que no es
válido y actual se muestra en gris.

## Etapas

### E0 — Base del proyecto y exploración BLE

**Objetivo:** confirmar que PlatformIO sube y corre en esta placa, traer
lo que se reutiliza, y averiguar cómo habla el adaptador BLE antes de
escribir el transporte. No toca la pantalla.

**Tareas:**

1. `git init` (el repositorio en GitHub lo crea el usuario o lo pide).
2. `platformio.ini`:
   - `[env:esp32s3]` con la configuración de la tabla de decisiones
     (placa, flash, PSRAM, particiones, `-DBOARD_HAS_PSRAM`,
     `-DARDUINO_USB_CDC_ON_BOOT=1`, `-DARDUINO_USB_MODE=1`),
     `monitor_speed = 115200` y `monitor_echo = yes`.
   - `[env:native]` igual que en el anterior, y
     `default_envs = esp32s3`.
3. Copiar del proyecto anterior, sin cambios: `lib/obd/`, `test/` y
   `.clang-format`. `pio test -e native` tiene que pasar como allá.
4. `include/config.h`: los pines de **Hardware de la placa**, el nombre
   local BLE `DS3-Monitor` y los tiempos de esta etapa.
5. `main.cpp` de exploración:
   - Al arrancar, en el monitor serie: chip, PSRAM, flash y motivo del
     último reinicio. GPIO8 en `LOW` (pantalla apagada: todavía no se
     inicializa).
   - `!escanear`: búsqueda BLE activa de 10 s. Lista número, nombre,
     dirección, tipo de dirección, RSSI y servicios anunciados. Marca
     los que parecen OBD: nombre con `OBD`, `ELM`, `Vgate`, `Vlink`,
     `Veepeak`, `iCar`, `OBDLink`..., o servicio `FFF0`, `FFE0`, `18F0`
     o `E7810A71-73AE-499D-8C15-FAA9AEF0C3F2`.
   - `!explorar N`: conecta al número `N` de la lista y muestra cada
     servicio con sus características y propiedades (`READ`, `WRITE`,
     `WRITE_NR`, `NOTIFY`, `INDICATE`), y el MTU negociado. Avisa si
     pide emparejamiento.
   - `!usar N`: se suscribe a la característica de notificación y anota
     la de escritura. Desde ahí, las líneas sin `!` van al adaptador:
     pasan por `comandoPermitido()`, se escriben con `\r`, y se juntan
     las notificaciones hasta el `>` o un timeout. Log como el anterior
     (`>> CMD`, `<< "texto\r\r" OK 123 ms`), más cuántas notificaciones
     llegaron y de qué tamaño.
   - Todavía no guarda nada en la flash.
6. Si el adaptador resulta ser Wi-Fi y no BLE, frenar y avisar: la placa
   también tiene Wi-Fi, pero E0 y E2 cambian.

**Verificación del agente:** `pio run` compila y `pio test -e native`
pasa.

**Verificación del usuario** (adaptador alimentado en banco, con 12 V en
el pin 16 y GND en los pines 4/5, o en el auto con contacto):

- Sube por el USB-C y el monitor muestra 8 MB de PSRAM y 16 MB de flash.
- `!escanear` encuentra el adaptador.
- `!explorar` y `!usar`: `ATZ`, `ATI` y `ATRV` responden.
- `04` responde `RECHAZADO`.

**Traspaso:** completar en **Datos descubiertos** nombre, dirección,
servicio, características, MTU, tamaño de las notificaciones, si pidió
emparejamiento y tiempos por comando.
Decidir con el usuario si `!escanear` y `!explorar` quedan en el
firmware (como `!descubrir` en el anterior).

### E1 — Pantalla y touch

**Objetivo:** pantalla y touch andando en horizontal, con la base
gráfica sobre la que se arman las páginas. Se puede hacer en paralelo
con E2.

**Tareas:**

1. Traer del ejemplo `10_LVGL_V9_Test` de Waveshare el driver
   `esp_lcd_axs15231b` a `lib/axs15231b/`, con su licencia.
2. `src/pantalla_hw.h/.cpp`, a partir del `lvgl_port.c` del ejemplo:
   - Bus QSPI y panel, búfers en PSRAM y el de transferencia en RAM con
     DMA, rotación por software a 640×172.
   - Touch: lectura por I2C (`0x3B`, SDA 17 / SCL 18) como en el
     ejemplo, con las coordenadas rotadas igual que la imagen.
   - Brillo con PWM en GPIO8.
   - Sin tarea de LVGL: `lv_timer_handler()` desde `loop()` y el tick
     con `millis()`.
3. LVGL 9.3.0 fijada en `lib_deps` y un `include/lv_conf.h` propio, con
   solo lo que se usa (sin demos ni widgets de más). Fuentes Montserrat
   en los tamaños que hagan falta, con el `°`.
4. `main.cpp` de prueba: barra de estado arriba, un número grande, un
   texto con `°`, los colores que se piensan usar, una marca donde se
   toca y las coordenadas del touch en el monitor serie. Brillo con `+`
   y `-` en la consola, como el contraste del anterior.

**Verificación del agente:** compila; los tests nativos siguen pasando.

**Verificación del usuario:**

- Se ve derecha, sin parpadeo ni franjas.
- El touch coincide con donde se toca, en las cuatro esquinas.
- El brillo se regula. Anotar el cómodo de día y de noche.
- El número grande se lee desde la posición de manejo.

**Traspaso:** API de `pantalla_hw`, memoria usada (RAM interna y PSRAM)
y tiempo de un refresco completo.

### E2 — Transporte BLE con el ELM

**Objetivo:** la clase `Elm327` con la misma interfaz del anterior,
sobre BLE, para que el resto del programa no note el cambio. Usa lo que
E0 descubrió del adaptador.

**Tareas:**

1. `src/elm327.h/.cpp` con la interfaz final del anterior (ver sus notas
   de E1, E3, E5 y E6): `conectar(aviso)`, `conectado()`,
   `desconectar()`, `olvidar()`, `enviar(comando, timeout, registrar)`,
   `mientrasEspera(funcion)` y `nombreEstado()`.
   - `conectar()`: primero la dirección guardada en `Preferences`
     (namespace `obd`, con el tipo de dirección); si falla, busca por
     nombre o servicio. Avisa la fase para la pantalla. Todo con timeout
     acotado (anotar los tiempos).
   - Las notificaciones se copian a un buffer de FreeRTOS desde el
     callback. `enviar()` es lo único que lo lee.
   - `enviar()`: valida con `comandoPermitido()`, descarta lo que quedó
     en el buffer, escribe el comando con `\r` y espera el `>` o el
     timeout. Mientras espera, llama a la función de
     `mientrasEspera()`.
   - La desconexión se detecta con el callback del stack.
   - Pedir un intervalo de conexión corto y medir cuánto mejora.
   - `olvidar()`: borra la dirección guardada y el vínculo, si hubo.
2. `src/consola.h/.cpp` portada del anterior: `!reconectar`, `!olvidar`,
   `!log` y los comandos de E0 que queden.
3. `main.cpp` de banco, como E1 del anterior: conecta, `ATZ`, `ATE0`,
   `ATI`, `ATRV`, y reconecta solo si se pierde.

**No hacer:** máquina de estados completa ni pantalla (es de E3).

**Verificación del agente:** compila; los tests nativos siguen pasando.

**Verificación del usuario:**

- Conecta y muestra versión y voltaje en el monitor serie.
- La segunda vez conecta por la dirección guardada, más rápido (anotar
  los dos tiempos).
- Al apagar el adaptador se informa la pérdida; al prenderlo reconecta
  solo.
- `04` responde `RECHAZADO`.

**Traspaso:** tiempos de conexión, tiempo por comando AT (en el anterior
era ~40 ms por SPP) y API final.

### E3 — El monitor completo en el auto

**Objetivo:** juntar E1 y E2 y portar lo que ya funciona en el DS3, con
una pantalla provisoria. Al terminar, el monitor hace lo mismo que el
anterior en E5.

**Tareas:**

1. Portar del anterior `main.cpp` (máquina de estados), `datos`,
   `sondeo` y `descubrimiento`.
   - Sacar lo propio de la ESP32-CAM: LEDs, cámara, botón en GPIO16 y
     PSRAM desactivada.
   - El detector de brownout queda **activo**. Se decide en E7 con la
     placa instalada.
   - Se inicializa con `ATSP6`.
2. `mientrasEspera()` llama a `lv_timer_handler()`, para que la pantalla
   no se congele durante una consulta.
3. `src/pantalla.h/.cpp`: las pantallas de estado del anterior
   (`CONECTANDO`, `INICIANDO ELM`, `BUSCANDO ECU`, `SIN ECU`,
   `SIN CONEXION`, `ALIMENTACION`) y una pantalla provisoria con RPM,
   refrigerante, velocidad, admisión y voltaje. El diseño final es de
   E4.
4. **E3b (usuario, en el auto):** `!descubrir` con contacto y en ralentí
   con el adaptador nuevo, como E4b del anterior. El log va a
   `docs/descubrimiento/AAAA-MM-DD.log`.
5. **E3c (agente):** comparar con `../esp32-ds3-obd2/docs/descubrimiento/2026-10-23.log`
   (PIDs soportados, `010C1`, tiempos). Si alguna respuesta del
   adaptador nuevo es distinta, agregarla como test en `lib/obd` y
   corregir el parser si hace falta.

**Verificación del usuario (auto):**

- Los valores coinciden con el tablero.
- Al apagar el motor pasa a `SIN ECU` sin dejar números viejos.
- Al dar arranque, si se corta, se recupera solo.
- Consultas por segundo (en el anterior: ~11).

### E4 — Diseño de páginas y navegación táctil

**Objetivo:** las páginas finales, aprovechando la pantalla grande y el
color.

**E4a (agente + usuario):** maqueta de las páginas a tamaño real
(640×172), para elegir antes de programar:

- Navegación: deslizar, tocar a izquierda o derecha, pestañas, o el
  botón BOOT como alternativa física. En el auto: zonas grandes y nada
  que obligue a mirar mucho.
- Páginas, las mismas del anterior: Motor, Aire/Volt, Mezcla, Fallas e
  Info. En Mezcla entran los 5 datos que el DS3 soporta (en la Nokia
  quedaba afuera la sonda post).
- Colores pensados para la noche (fondo oscuro), tamaños de letra,
  barra de estado e indicador de página.
- Lambda como barra rica/pobre, como en el anterior, ahora con color.

**E4b (agente):** `src/paginas.h/.cpp` con lo elegido. El sondeo
prioriza la página visible con `Sondeo::visibles()`, como en el
anterior.

**Verificación del usuario:** la navegación funciona con el auto en
marcha y todo se lee bien de día y de noche.

### E5 — Códigos de falla

**Objetivo:** la página Fallas. En el anterior quedó hecha (E7) pero sin
verificar en el auto; acá se verifica por primera vez.

**Tareas:**

1. Portar `leerFallas()` y sus reglas: `01011`, `03`, `07` y `0A` al
   entrar en la página y cada 30 s mientras está visible; reintento a
   los 5 s si falla; `Sin fallas` solo si `03` y `07` respondieron bien
   con cero códigos; nunca `Sin fallas` con un error.
2. `pantallaFallas()` de `lib/obd` arma renglones de 14 caracteres para
   la Nokia. Hace falta una versión para esta pantalla (más códigos con
   su descripción por página), en `lib/obd` y con tests.
3. **No existe ninguna función para borrar fallas.** `04` sigue
   rechazado.

**Verificación del usuario:** en el auto se ve `Sin fallas` como en el
relevamiento.

### E6 — Brillo y ajustes

**Objetivo:** que se pueda usar sin la notebook: brillo y las acciones
que hoy solo están en la consola.

**Tareas:**

1. Brillo en niveles, cambiado desde la pantalla y guardado en
   `Preferences`.
2. Página Info: adaptador (nombre y dirección), versión del ELM,
   protocolo, PIDs soportados, consultas por segundo y RSSI.
3. Acciones en Info: `Reconectar` y `Olvidar adaptador`. Las que borran
   algo piden mantener apretado unos segundos, para no tocarlas sin
   querer.

**Verificación del usuario:** brillo cómodo de noche; las acciones
andan y no se disparan sin querer.

### E7 — Cierre e instalación

**Tareas:**

1. Prueba larga: RAM interna y PSRAM libres impresas cada minuto durante
   30 min (banco y auto), sin reinicios ni pérdida de memoria.
2. Alimentación desde el USB del auto: ¿se reinicia al dar arranque?
   ¿Hace falta el capacitor del anterior? Decidir si el detector de
   brownout queda activo. Batería: no se recomienda dentro del auto por
   la temperatura; lo decide el usuario.
3. Montaje: orientación final (`config.h`) y lejos del sol directo.
4. README final: hardware, uso y páginas.
5. Limpiar warnings propios.

### E8 — Conexión al prender y apagar el auto

**Descartada** por el usuario (2026-10-26): no es tan importante. No se
hacen el registro de eventos en la flash ni el watchdog. El cuelgue
ocasional al prender y apagar el auto ("a veces como que se cuelga entre
que lo prendo y apago el auto, no se conecta") queda sin investigar; lo
que se sabía está en las notas de E8.

### E9 — Pantalla de espera con el DS3

**Objetivo:** cambiar las pantallas de estado por una imagen de un
Citroën DS3 rojo, con muy poca información, y que tocar la pantalla
reintente.

Lo que pidió el usuario (2026-10-26): hay mucha información que no le
importa. "Que sea más user friendly: en vez de mostrar eso cuando está en
contacto sin ECU o apenas prende el USB, que aparezca la imagen de un
Citroën DS3 rojo. Que si aprieto la pantalla reintente conectar o resetee
la conexión."

**Antes de programar, decidir con el usuario:**

- **La imagen.** *Propuesta:* una foto del DS3 del usuario, de costado,
  recortada a 640×172 (o a 640×144 debajo de la barra). No usar fotos de
  internet ni el logo de Citroën: son de otros. Otra opción: una
  ilustración hecha para esto.
- **Qué más se ve.** *Propuesta:* solo una línea chica en gris abajo
  (`Conectando...`, `Esperando la ECU...`, `Toca para reintentar`). El
  detalle sigue en el monitor serie.
- **El toque.** *Propuesta:* tocar = reintentar ya, sin esperar los 5 o
  10 s; mantener 3 s = reiniciar la placa, por si quedó trabada.
- **Qué pantallas reemplaza.** *Propuesta:* `CONECTANDO`,
  `BUSCANDO ADAPTADOR`, `SIN ADAPTADOR`, `INICIANDO ELM`, `BUSCANDO ECU` y
  `SIN ECU`. `ALIMENTACION` (rojo) queda, porque es una falla de verdad.
  Si se pierde la ECU con el motor andando, ¿imagen o aviso?

**Tareas:**

1. La imagen como arreglo de C para LVGL (`lv_image_dsc_t`). A 640×172 en
   RGB565 son 220 KB de flash (quedan ~1,5 MB libres); con paleta, menos.
   Prender `LV_USE_IMAGE` en `lv_conf.h`. La conversión, con un script en
   `herramientas/` para poder cambiar la foto.
2. `pantalla`: una función de espera que reemplaza a las de estado, con
   sus casos en el render.
3. El toque en la pantalla de espera deja un pedido, como los botones de
   Info: reintentar (adelanta el próximo intento) o reiniciar. No usa el
   ELM de otra forma.

**Verificación del usuario:** al poner el contacto se ve el DS3 hasta que
aparecen los datos, y tocar la pantalla reintenta.

### E10 — Viaje: máximos y recorrido

**Objetivo:** una página nueva, `VIAJE`, con lo del viaje en curso.

**Antes de programar, decidir con el usuario:**

- **Qué datos.** *Propuesta:* duración, distancia, velocidad máxima y
  promedio, RPM máxima, refrigerante máximo y voltaje mínimo y máximo con
  el motor en marcha. El consumo no: el DS3 no da MAF (`10`) ni MAP
  (`0B`).
- **Cuándo empieza un viaje.** *Propuesta:* al poner el contacto. La placa
  arranca con la llave, así que no hace falta guardar nada. ¿Guardar el
  último viaje para verlo después?
- **Reiniciar a mano.** *Propuesta:* un botón que hay que mantener
  apretado 3 s, como Olvidar adaptador.
- El diseño, con una maqueta antes, como en E4.

**Tareas:**

1. Los acumuladores en `lib/obd`, sin Arduino y con tests: la distancia
   integrando la velocidad entre lecturas, sin sumar huecos de más de
   `VIGENCIA_MS`.
2. La velocidad entra en cada consulta aunque no se vea, para que la
   distancia salga bien (hoy, fuera de Motor, se lee cada 2,5 s). Entra en
   los 6 PIDs por consulta.
3. La página, con sus casos en el render. Con DS3 (E9), VIAJE y FUERZA G
   (E12) son 9 páginas: hay que subir `MAX_PAGINAS` (8), los puntos de la
   barra. Que VIAJE vaya antes de DS3, que es la última.

**Verificación del usuario:** en un viaje, la distancia contra el
parcial del tablero y los máximos contra lo que se vio.

### E11 — Alarmas

**Objetivo:** avisar con color, y si el usuario quiere con sonido, cuando
algo está fuera de lo normal, en cualquier página.

**Antes de programar, decidir con el usuario:**

- **Cuáles y con qué umbral.** *Propuesta:* refrigerante de 105 °C o más;
  voltaje con el motor en marcha por debajo de 13,0 V o por encima de
  15,0 V durante más de 10 s (falla de carga); luz de falla encendida
  (leer `01011` cada tanto aunque no se vea Fallas). ¿RPM altas?
- **Cómo se ve.** *Propuesta:* la barra de arriba en rojo con el texto de
  la alarma, sobre cualquier página, y el valor en rojo en su página.
  Tocar la barra la silencia por unos minutos.
- **Sonido.** El parlante de la placa necesita el códec ES8311, el I2S y
  el amplificador (`NS_MODE`, EXIO7 del TCA9554): se levanta para esta
  etapa la regla de no inicializar el audio ni el expansor. *Propuesta:*
  un pitido generado por el programa, sin archivos de sonido.

**Tareas:**

1. La lógica en `lib/obd`, con tests: umbral, histéresis y tiempo mínimo
   para no saltar por un dato suelto ni por un `NO DATA`.
2. El dibujo, sobre las páginas, con sus casos en el render.
3. El sonido, si se elige.
4. Para probar: umbrales bajos en `config.h` (por ejemplo, refrigerante de
   40 °C) y después los de verdad.

**Verificación del usuario:** con los umbrales bajos, cada alarma sale y
se apaga cuando corresponde; con los de verdad, no salta en un viaje
normal.

### E12 — Fuerza G

**Objetivo:** una página nueva, `FUERZA G`, con la aceleración lateral y
la longitudinal, medidas con el IMU de la placa (QMI8658).

**Antes de programar, decidir con el usuario:**

- **Cómo se ve.** *Propuesta:* un círculo con un punto que se mueve, como
  los medidores de G, y los máximos del viaje para cada lado. Maqueta
  antes.
- **La calibración del montaje.** *Propuesta:* al arrancar, con el auto
  quieto, la gravedad marca "abajo"; "adelante" sale de cómo está montada
  la placa (en `config.h`) o de la primera frenada en línea recta.

**Tareas:**

1. El I2C de la placa (SDA 47 / SCL 48, compartido con el RTC, el audio y
   el expansor) y el QMI8658 en `0x6B`, leído a ~50 Hz sin frenar el
   sondeo (su FIFO o lecturas cortas también mientras se espera al ELM).
2. Las cuentas en `lib/obd` con tests: llevar la medición a los ejes del
   auto y filtrar las vibraciones.
3. La página, con sus casos en el render. Los máximos se suman a VIAJE
   (E10). (Quedaron en FUERZA G: VIAJE estaba llena; ver notas de E12.)

**Verificación del usuario:** con el auto quieto marca 0; al frenar y en
las curvas, el punto va para el lado que corresponde.

### E13 — Registro del viaje en la microSD

**Objetivo:** guardar cada viaje en la microSD, para verlo después en la
PC.

**Antes de programar, decidir con el usuario:**

- **Qué se guarda.** *Propuesta:* un CSV con una línea por segundo, como
  la tabla del monitor (tiempo desde el arranque, todas las magnitudes y
  las consultas por segundo), y los eventos (fallas leídas, alarmas,
  pérdidas de conexión). Un archivo por viaje (`VIAJE001.CSV`), con el
  número en `Preferences`. Sin hora real, salvo que se haga E14.
- **El corte de luz.** El USB se corta con la llave, a veces mientras se
  escribe, y la tarjeta puede quedar dañada. *Propuesta:* escribir de a
  bloques cada pocos segundos, con `flush` después de cada uno, y aceptar
  que se pierdan los últimos segundos del viaje. La batería no se usa para
  apagar con cuidado (decisión del usuario, 2026-10-26: complica el código;
  si se corta, se corta).

**Tareas:**

1. La microSD por SPI (CS 38, MOSI 39, MISO 40, SCLK 41). La escritura no
   puede frenar el sondeo (una tarjeta puede tardar decenas de ms): con un
   búfer o una tarea aparte. (Quedó en modo SD de 1 bit, como el ejemplo de
   la V2, con los mismos pines: ver notas de E13.)
2. `!sd` en la consola: si hay tarjeta, el espacio libre y el archivo
   actual.
3. El formato del CSV, en el README.

**Verificación del usuario:** después de un viaje, el CSV se abre en la
PC. Sacar la llave varias veces con el auto andando y que la tarjeta se
siga leyendo.

### E14 — Reloj

**Objetivo:** la hora en la barra de arriba (y en el registro de E13).

**Hace falta la batería.** El RTC (PCF85063, `0x51`) toma corriente de los
3,3 V de la placa y de la batería, cada una por un diodo (ver **Hardware de
la placa**). Sin batería, como el USB se corta con la llave, pierde la hora
cada vez que se apaga el auto. Con la batería puesta (decisión del usuario,
2026-10-26), sigue andando con la placa apagada: no hace falta soldar nada.

Antes de empezar: con la batería puesta y la llave sacada, medir con un
tester la tensión en C32, al lado del RTC (debería dar ~3,4–3,9 V). Si no
da, la placa no es como el esquemático, y las opciones eran un
supercapacitor o una pila en la pata 10 del RTC, o la hora por Wi-Fi con el
celular. (El 2026-10-27 el usuario tenía la batería puesta, sin medir C32: se
hizo igual, y lo confirma la prueba de una noche. Ver notas de E14.)

**Tareas:**

1. El PCF85063 en el I2C de la placa.
2. Poner la hora con `!hora AAAA-MM-DD HH:MM` en la consola o desde la
   pantalla.
3. Mostrarla en la barra. Si el RTC perdió la hora (el bit `OS` del
   PCF85063 avisa que el oscilador se detuvo, por ejemplo si se desconectó
   la batería), no mostrar una hora falsa: `--:--`.

**Verificación del usuario:** después de una noche con el auto apagado,
la hora sigue bien.

### E15 — La placa V2: pines de la pantalla y brillo

**Objetivo:** que el firmware use los pines de la V2, que es la placa del
usuario (ver notas de E11), y que el brillo se pueda regular.

Lo decidió el usuario (2026-10-26): hacerlo aparte de E11, después de
mergearla, y con el brillo en 5 niveles en Info, como se había hecho en E6.

**Tareas:**

1. El I2C de la placa y el TCA9554 antes que la pantalla, compartidos con el
   sonido.
2. El reset del panel por EXIO5; el GPIO21 (`LCD_TE`) y el GPIO8
   (`EXIO_INT`) sin tocar.
3. La luz por PWM en el GPIO42, con `BL_EN` (EXIO1). Apagada hasta mandar
   el primer cuadro negro.
4. El brillo en 5 niveles: - y + en Info, `!brillo N` en la consola (para
   probarlo en el banco, sin la ECU), guardado en `Preferences`.

**Verificación del usuario:** la pantalla arranca bien, sin basura; el
brillo cambia con cada nivel y se mantiene al apagar y prender.

### E16 — Cambiar de página deslizando el dedo

**Objetivo:** que las páginas se cambien deslizando el dedo (swipe), no
tocando las mitades de la pantalla. Lo pidió el usuario (2026-10-27): en el
auto, sin un teclado, tocar los botones o la pantalla para cambiar de página
es incómodo.

Lo decidió el usuario: **solo deslizar** (tocar no cambia de página, así un
bache o un roce no la cambian) y **de golpe**, sin animación (cada cuadro
cuesta ~33 ms de envío: se vería a saltos y frenaría las consultas).

**Tareas:**

1. Deslizar hacia la izquierda: la siguiente; hacia la derecha: la anterior.
   En toda la pantalla, también la barra, DS3 y los botones de Info, que no
   se tienen que apretar al deslizar.
2. Tocar no cambia de página. Mantener apretada Viaje sigue reiniciando el
   viaje, con el dedo quieto. El botón BOOT, igual.
3. El render, con deslizamientos.

**Verificación del usuario:** en el auto, cambiar de página deslizando, en
todas, sin que se aprieten los botones de Info ni se cambie la página sin
querer.

### E17 — Pantalla fluida: que dibujar no frene

**Objetivo:** que las animaciones y los cambios de página se vean fluidos, y
que dibujar no le saque tiempo a las consultas a la ECU.

Lo que se sabe hoy:

- Cada cambio en pantalla manda **la imagen entera** (en QSPI el panel no
  acepta una fila de inicio, ver notas de E1): ~32 ms de envío, se cambie lo
  que se cambie. De esos, **~20 ms son girar la imagen** (LVGL dibuja en
  horizontal y el panel es vertical), leyendo de la PSRAM columna por
  columna.
- Mientras se manda, `loop()` espera. Con FUERZA G redibujando cada 50 ms las
  consultas bajaron de 6–7 a 3–4 por segundo (registro de E13); por eso hoy
  va cada 100 ms.
- Refresco completo (dibujar todo y mandarlo): 64 ms en el banco (E13), 93 ms
  en Motor (E5). LVGL está compilado para tamaño (`-Os`). (Corregido en E17:
  todo iba a `-O0`, sin optimizar; ver notas de E17.)

**Tareas** (medir antes y después de cada una):

1. **Medir:** el tiempo de dibujar, de girar y de mandar cada cuadro, con su
   promedio y su máximo por segundo, en la tabla del monitor y en `!info`.
2. **Girar por bloques** (por ejemplo de 8×8 píxeles, con palabras de 32
   bits), para leer la PSRAM en orden y aprovechar su caché. Objetivo: menos
   de 8 ms.
3. **LVGL con `-O2`**, solo esa biblioteca; mirar cuánto crece el programa
   (hoy 65 % de 3 MB).
4. **Mandar desde otra tarea, con dos imágenes en la PSRAM** (sobran 7,9 MB):
   LVGL dibuja la siguiente mientras se manda la anterior, y `loop()` deja de
   esperar el envío. Es el cambio que más ayuda a las consultas.
5. **Probar el QSPI más rápido** (hoy 40 MHz; 60 u 80 si el panel aguanta, sin
   basura en la imagen).
6. **`LCD_TE`** (GPIO21 en la V2): empezar cada envío con el barrido del
   panel, para que la animación no se vea cortada.
7. Con todo eso, volver FUERZA G a 50 ms y revisar la animación de DS3.

**Verificación del usuario:** en el auto, las consultas por segundo iguales
en todas las páginas (6–7), también en FUERZA G y DS3; los cambios de página
y las animaciones, más suaves y sin cortes.

### E19 — Micrófono y parlante: grabar y hablar

**Descartada** por el usuario (2026-10-27), después de probarla: anduvo, pero
el parlante se oye bajo. Se sacó el código. Lo que se hizo y lo que se supo,
en las notas de E19.

**Objetivo:** la base del asistente, sin internet: grabar la voz con los
micrófonos de la placa y reproducir audio por el parlante.

La placa tiene el códec **ES7210** (micrófonos) y el **ES8311** (parlante)
por I2S, con el amplificador NS4150B (ver **Hardware de la placa**). El
parlante ya anda desde E11 (los pitidos); los micrófonos todavía no se usan.
El ejemplo `08_Audio_Test` de la V2 graba y reproduce: partir de ahí.

**Antes de programar, decidir con el usuario cómo se activa** (en el auto,
sin teclado):

- Un gesto en cualquier página (por ejemplo, mantener apretado 1 s; ojo con
  Viaje, donde mantener reinicia el viaje).
- Una página ASISTENTE con un botón grande (hoy hay 9 puntos y la barra está
  justa con la hora y la marca SD).
- El botón BOOT, atrás de la placa.
- Una palabra para despertarlo: ESP-SR (viene con el core) trae WakeNet, pero
  con palabras fijas y en inglés o chino. En español haría falta entrenar una
  propia. *Propuesta:* empezar apretando, sin palabra.

**Tareas:**

1. El ES7210 en el I2C de la placa (`busPlaca()`) y la entrada I2S (GPIO6),
   a 16 kHz, mono, 16 bits, en un búfer de la PSRAM (unos 32 KB por segundo).
2. Reproducir PCM por el ES8311 desde la PSRAM, sin cortar los pitidos de las
   alarmas (que tienen prioridad).
3. `!grabar N` en la consola: graba N segundos, los guarda en la microSD como
   WAV y los reproduce. Sirve para oír cómo suena en el auto con el motor
   andando.
4. Que grabar no frene las consultas: en una tarea aparte, como el IMU.

**Verificación del usuario:** en el auto, con el motor andando, una
grabación se entiende al escucharla en la PC.

### E20 — Asistente de voz por OpenRouter

**Descartada** (2026-10-27): el usuario dio el plan por terminado al
descartar E19. Queda como estaba planeada, por si se retoma.

**Objetivo:** preguntarle cosas en voz alta y que conteste por el parlante.
Por ejemplo, "¿cómo está el motor?", "¿qué significa la falla P0420?" o
"¿cuánto llevo de viaje?", además de preguntas generales.

Lo decidió el usuario (2026-10-27): **por OpenRouter**, no atado a un
proveedor, para poder cambiar de modelo. OpenRouter tiene, con la misma
clave (según su documentación, mayo de 2026):

- `/api/v1/audio/transcriptions`: voz a texto (Whisper).
- `/api/v1/chat/completions`: el modelo que se elija. Algunos aceptan el
  audio directo (`input_audio`, en base64), y se saltea el paso anterior.
- `/api/v1/audio/speech`: texto a voz, en MP3 o **PCM** (PCM se reproduce sin
  decodificar).

Camino de una pregunta: apretar → grabar hasta que se suelte (o hasta un
silencio) → voz a texto → modelo → texto a voz → parlante. Unos 2 a 5 s
entre que se termina de hablar y empieza la respuesta.

**Antes de programar, decidir con el usuario:**

- **Dónde corre:**
  - *Propuesta:* directo desde la placa, por el hotspot del celular, con una
    clave de OpenRouter **con límite de gasto**.
  - La alternativa es un puente propio en la nube, que guarde la clave y
    permita cambiar el modelo o las instrucciones sin volver a subir el
    firmware.
- **Qué modelo,** y cuánto puede gastar la clave por mes (OpenRouter muestra
  el precio de cada modelo).
- **Qué datos del auto ve el modelo.** *Propuesta:* un resumen en texto con
  cada pregunta: los datos actuales, las fallas leídas, el viaje y la fuerza
  G.

**Reglas que no se negocian:**

- **Solo lectura con el auto, también para el asistente.** El modelo recibe un
  resumen de los datos; no hay ningún camino desde la respuesta hacia el ELM.
  Si se usan herramientas, son solo de lectura.
- **La clave y la contraseña del Wi-Fi no van en el código ni en el
  repositorio:** se cargan por la consola y quedan en `Preferences`.
- **Para manejar:** respuestas cortas y habladas, sin textos largos en la
  pantalla.
- El audio y las preguntas salen a OpenRouter y al proveedor del modelo:
  avisarlo en el README.

**Riesgos que hay que medir primero** (una prueba chica antes de armar todo):

- **RAM interna:** hoy quedan 74 KB libres; el Wi-Fi y TLS necesitan una parte
  grande. Puede hacer falta pasar búferes a la PSRAM. (E17 no pasó nada a la
  PSRAM: las dos tandas del envío, 44 KB, siguen en la RAM interna, y su
  tarea suma 4 KB.)
- **Wi-Fi y BLE comparten la radio:** mientras se habla con OpenRouter, el ELM
  por BLE puede ir más lento.
- **Flash:** hoy 59 % de 3 MB (con todo a `-Os`, ~50 %: ver notas de E17);
  Wi-Fi y HTTPS suman bastante. Puede hacer falta otra tabla de particiones
  (la flash es de 16 MB y la partición FAT de 9 MB no se usa), cuidando lo
  guardado en `Preferences`.

**Tareas:**

1. Wi-Fi al hotspot del celular, con `!wifi RED CLAVE` y `!openrouter CLAVE`
   en la consola, guardados en `Preferences`. Reconexión sola; sin Wi-Fi, el
   asistente dice que no hay conexión y lo demás sigue igual.
2. La prueba chica: un pedido HTTPS a OpenRouter, midiendo la RAM interna y
   las consultas por segundo mientras tanto.
3. Voz a texto, modelo y texto a voz, cada paso con un tiempo máximo. Si algo
   falla, un aviso corto hablado y nada más.
4. El resumen de los datos del auto, armado en `lib/obd` con tests (sin
   Arduino).
5. En pantalla, solo el estado: escuchando, pensando, hablando.

**Verificación del usuario:** en el auto, preguntas sobre el auto y
generales; las consultas por segundo no bajan mientras no se le habla.

### E21 — WhatsApp por el celular

**Descartada** (2026-10-27), como E20.

**Objetivo:** que lea en voz alta los mensajes de WhatsApp que llegan y que se
puedan contestar dictando.

WhatsApp no tiene una API para cuentas personales. Las bibliotecas que
automatizan WhatsApp Web van contra sus condiciones y pueden hacer que
bloqueen el número: **no usarlas**. El camino que usan Android Auto y los
relojes son **las notificaciones de Android**: una app con permiso de
notificaciones lee los mensajes que llegan y contesta con la acción
"Responder" de la notificación. El celular del usuario es un Samsung con
Android (se vio por el USB en E15).

**Antes de programar, decidir con el usuario:**

- La app del celular: Tasker con AutoNotification, o una app propia chica
  (Kotlin).
- Cómo se hablan el celular y la placa: por el mismo hotspot (la placa le
  habla a la IP del celular) o por BLE.
- **Siempre confirmar antes de mandar:** "¿Le mando a Juan: llego en 10
  minutos?", y se manda solo con un "sí".

**Tareas:**

1. Del celular a la placa: los mensajes nuevos (quién y qué).
2. Leerlos por el parlante, con el texto a voz de E20, cuando el auto está
   parado o en un momento tranquilo (a decidir).
3. Contestar dictando: voz a texto, confirmación hablada y respuesta por la
   notificación.
4. Mensajes nuevos a cualquier contacto: más difícil sin automatizar la
   pantalla del celular. Dejarlo para después de ver cómo anda lo anterior.

**Verificación del usuario:** en el auto, un mensaje que llega se lee, se
contesta dictando y la respuesta llega bien. Nunca se manda algo sin el "sí".

### E22 — Viajes anteriores, también sin conexión

**Objetivo:** que sin conexión con el adaptador se pueda ir a la página DS3 y
ver los datos de los viajes pasados.

Lo que pidió el usuario (2026-10-27): "si no está conectado al bluetooth, que
te deje ir a la pantalla esa del autito girando y ver los datos de viajes
pasados". Hasta E22 el viaje vivía solo en la RAM, y sin datos se veía solo
la espera.

**Decidido con el usuario, antes de programar** (entre las opciones que
propuso el agente):

- **De dónde salen:** un resumen de cada viaje en la flash de la placa
  (`Preferences`), actualizado cada minuto mientras se maneja; los últimos 10.
  No de los CSV de la microSD (solo con tarjeta, y había que recorrer cada
  archivo).
- **Cómo se ven:** un viaje por pantalla, con las 8 casillas de VIAJE y la
  fecha en la barra; tocar pasa al anterior. No una tabla de los últimos 4.
- **Cuándo:** también con la ECU conectada, como una página después de VIAJE.
  Sin conexión: espera → viajes anteriores → DS3, deslizando.

**Tareas:**

1. En `lib/obd`, con tests: el resumen de un viaje, sus bytes (con versión) y
   la lista de los anteriores.
2. Guardarlos en la flash sin gastarla ni frenar el sondeo, y que un corte de
   luz en el medio no deje un viaje mal leído.
3. La página ANTERIORES, y las páginas sin datos, con sus casos en el render.
4. `!viajes` en la consola, para verlos en el banco.

**Verificación del usuario:** después de un viaje, al volver a poner el
contacto aparece en ANTERIORES con sus datos y la fecha; sin el adaptador se
pasa deslizando de la espera a ANTERIORES y a DS3.

### E23 — Que no se cuelgue al prender y apagar el auto

**Objetivo:** que la placa no quede trabada (hoy hay que desenchufarla) y, si
se traba igual, que se reinicie sola y deje dicho dónde estaba.

Lo que contó el usuario (2026-10-28): "se tilda cuando se apaga, y después me
voy del auto, vuelvo, abro la puerta, se energiza el USB, se enciende la
pantalla, aparece la animación del auto y se cuelga cuando está saliendo la
trompa. Intento reiniciar manteniendo apretada la pantalla y no pasa nada:
tengo que conectar y desconectar el USB". Es el cuelgue de E7, que E8 no llegó
a investigar.

**Lo que se sabe:**

- La placa va al USB de la consola, el mismo de la radio (lo dijo el usuario):
  del otro lado hay un host USB, no un cargador. Se prende al abrir la puerta,
  antes de la llave, y sigue prendido de 31 s a 4 min después de sacarla
  (**Datos descubiertos**). Mientras tanto la placa reintenta cada 15 s, 10 de
  ellos bloqueada en `conectarBt()`.
- Los 6 registros del 2026-10-27 empiezan con `Inicio de la placa: encendido`:
  ningún reinicio raro. Pero un arranque sin la ECU no deja archivo (se abre
  con la primera lectura): el cuelgue al abrir la puerta no queda en la
  tarjeta.
- La trompa asoma a ~0,3 s de empezar la entrada, que empieza al prender la
  pantalla. Si `Serial` cree que hay un monitor (con un host USB puede pasar),
  `setup()` no espera `ESPERA_MONITOR_MS` y sigue con el sonido, el IMU, la
  microSD, el BLE e `imprimirInfo()` **con LVGL tomado**: la entrada y el touch
  quedan quietos hasta que `conectarBt()` lo suelta. Con un cargador espera los
  2 s y el auto termina de entrar antes.
- **Hipótesis 1: escribir por el USB bloquea.** En Arduino-ESP32 3.3,
  `HWCDC::write()` con un host que no lee espera hasta 20 veces
  `tx_timeout_ms` (100 ms) por llamada antes de rendirse (`HWCDC.cpp`, "host
  backpressure"). Lo que se imprime con LVGL tomado (en `setup()` y en
  `loop()`) trabaría la pantalla y mantener para reiniciar. Manejando no pasó
  (6–7 consultas/s en todos los viajes): la radio estaría en otro estado
  recién despertada.
- **Hipótesis 2: la radio reinicia la placa.** Abrir el puerto la reinicia
  (README, **Consola**); según cómo la radio maneje DTR y RTS, puede quedar en
  modo de descarga, quieta hasta cortarle la luz. Eso no lo arregla el
  firmware: solo un cable sin datos.
- Menos probables: un corte corto del USB mientras la radio arranca (sería un
  `brownout` y se vería `ALIMENTACION`) y las esperas largas del BLE (notas de
  E8), que desde E9 no traban la pantalla.
- **En el banco se trabó igual** (2026-10-28): enchufada a la Mac sin el
  monitor y sin el adaptador, se trabó en el mismo lugar que en el auto (con
  la trompa asomando), el touch no respondía y revivió sola al minuto (el
  usuario la había desenchufado y enchufado 3 veces). **Confirma la hipótesis
  1;** la 2 no revive sola. La cuenta da lo mismo: el búfer de salida del
  `HWCDC` es de 256 bytes (se llena con las primeras 4 o 5 líneas) y, antes de
  soltar LVGL, `setup()` hace unas 25–35 escrituras (`imprimirInfo()`, el RTC,
  los guardados, la tarjeta, el sonido, el IMU y el BLE): a ~2 s cada una, la
  pantalla queda quieta ~1 min. Después, cada `cambiarA()` imprime con LVGL
  tomado: con el adaptador apagado (llave sacada), ~2 s quieta cada 15 s.

**Antes de programar, pruebas del usuario** (con el firmware de hoy):

1. Enchufar la placa a la Mac **sin abrir el monitor** y mirar la entrada: si
   se traba con la trompa, es la hipótesis 1. (Hecha el 2026-10-28: se trabó y
   revivió sola, ver arriba.)
2. Unos días en el auto con un cable solo de carga (o un bloqueador de datos
   USB) entre la placa y la radio: si deja de trabarse, el problema es el USB
   con datos (1 o 2). Con la 1 confirmada, es opcional: solo descarta la 2.

**Tareas:**

1. Que escribir por el USB nunca bloquee: `Serial.setTxTimeoutMs(0)` y un
   búfer de salida más grande, para no perder líneas con el monitor abierto.
2. Que `setup()` no deje la entrada quieta: atender LVGL entre cada inicio
   (sonido, IMU, tarjeta, BLE) o soltarlo, como `conectarBt()`.
3. **Un guardián:** si LVGL pasa `GUARDIAN_MS` (10 s) sin atenderse, ni por
   `loop()` ni por la tarea de E9, un pánico a propósito: el core guarda el
   volcado en la partición `coredump` (queda en la flash aunque se desenchufe)
   y reinicia. Lo que bloquea a propósito con LVGL tomado (`!escanear`,
   `!explorar`) no lo tiene que disparar.
4. `!info`: el último reinicio y, si hay un volcado, la tarea y la función
   donde estaba (`esp_core_dump_get_summary()`), con las direcciones para
   `addr2line`. En el registro, `Inicio de la placa: ...` con lo mismo.
5. README: el USB es el de la radio, se prende al abrir la puerta y sigue unos
   minutos después de sacar la llave (hoy dice que se corta con la llave).

**Verificación del usuario:** unos días de uso sin desenchufar la placa. Si se
traba y en ~10 s no se reinicia sola, no es el programa (la hipótesis 2, o la
alimentación): el cable solo de carga.

### E24 — Pantalla principal: RPM, marcha, refrigerante, velocidad y voltaje

**Objetivo:** que MOTOR muestre de un vistazo lo que importa manejando, con la
marcha y un reloj del refrigerante, y sacar AIRE/VOLT.

Lo que pidió el usuario (2026-10-28): "la pantalla principal me gustaría que
tenga el indicador de revoluciones, indicador de marcha, reloj de temperatura
del refrigerante", y además la velocidad y el voltaje. Y "la segunda pantalla
[AIRE/VOLT] la volaría, no sirve para nada".

**Decidido con el usuario:**

- MOTOR: RPM, marcha, refrigerante (como reloj), velocidad y voltaje.
- La caja es **manual de 5**.
- El reloj del refrigerante: normal hasta 110 °C, ámbar de 110 a 115 y rojo
  desde 115 (E25 pone la alarma en 110).
- AIRE/VOLT sale: quedan 9 páginas.

**Antes de programar:** una maqueta con el render, 2 o 3 formas de poner los 5
datos en 640×144 (debajo de la barra), para que elija el usuario. La marcha y
las RPM, grandes; el reloj, un arco con aguja y las zonas.

**La marcha** (en `lib/obd`, con tests hechos con filas de los registros):

- Sale de la velocidad sobre las RPM, en km/h por 1000 rpm. Las 5 están
  medidas (**Datos descubiertos**): 8,1, 14,6, 21,0, 27,2 y 33,7. Las dos
  vienen en la misma consulta, ~6 por segundo en MOTOR.
- Es la más cercana, si cae dentro de un margen que sale de los registros: de
  2ª a 5ª, el 90 % cae a menos del 4 % de la suya; en 1ª, a menos del 15 %
  (la velocidad llega en km/h enteros).
- Cambia recién con dos lecturas seguidas en la marcha nueva, para no
  parpadear en el medio de un cambio.
- Sin marcha (embrague apretado, punto muerto, cambio en curso, menos de
  ~5 km/h): `–`. Con la RPM o la velocidad vencidas: `--`, como cualquier dato.
  Es un cálculo con datos reales, no un dato de la ECU: el README lo dice.
- *Propuesta:* una columna `Marcha` en el CSV, después de `Vel km/h`.

**Tareas:**

1. La marcha, en `lib/obd`, con tests.
2. MOTOR nueva, con sus casos en el render (con y sin marcha, vencida, el
   reloj en cada zona). El reloj necesita `LV_USE_ARC` o `LV_USE_SCALE` en
   `lv_conf.h` (hoy en 0): mirar cuánta flash suma.
3. Sacar AIRE/VOLT. El voltaje (`ATRV`, cada `PERIODO_ATRV_MS`) se sigue
   leyendo para MOTOR, la alarma, VIAJE y el CSV; la admisión sigue en el CSV,
   cada `PERIODO_LENTO_MS`.
4. README: las páginas, la marcha y cómo se ve cada cosa.

**MEZCLA "se cuelga y después anda"** (lo contó el usuario el mismo día). En
los registros no hay nada trabado: en MEZCLA las consultas siguen en 6–7 por
segundo (6,5 en 800 s de `VIAJE0007`), el dibujo no pasa de 24 ms y no faltan
segundos. Lo que sí se ve: con `Abierto frio` (los primeros 30–90 s) la ECU da
lambda y sonda post fijas y los ajustes en 0, y con el corte de inyección
(soltando el acelerador) lambda queda en 2,000 (**Datos descubiertos**). La
página se queda quieta varios segundos, y al cerrarse el lazo los números se
empiezan a mover. *Propuesta*, si el usuario confirma que era eso: con el lazo
abierto, lambda y sonda en gris con `sin regular`. Si era otra cosa (por
ejemplo, que deslizar no respondía), se mira aparte.

**Verificación del usuario:** en el auto, la marcha coincide con la palanca en
las 5 y da `–` con el embrague apretado; el reloj y los datos se leen de un
vistazo, de día y de noche.

### E25 — Aviso de cambio y umbrales del refrigerante

**Objetivo:** un aviso de "pasá el cambio" a las RPM que elija el usuario, y
que la alarma del refrigerante no salte con el motor regulando normal.

Lo que pidió el usuario (2026-10-28): "que la alarma de revoluciones sea
configurable, que la pueda poner como aviso de mete el cambio a tantas
revoluciones", "que aparezca como un parpadeo en toda la pantalla, onda auto de
carrera", "con pitido distinto al de alerta". Y la del refrigerante, "que salte
a 110 grados (5 s seguidos) y que se apague a 108".

**Decidido con el usuario:**

- `REFRIGERANTE`: 110 °C o más durante 5 s; se apaga con 108 °C o menos (hoy
  105 y 102). El EP6C regula en 103–108 °C (**Datos descubiertos**). En los
  registros del 2026-10-27 el máximo fue 108: con esto no habría saltado.
- El aviso de cambio hace parpadear **toda la pantalla**, en cualquier página,
  con un pitido distinto del de las alarmas.

**Propuestas, a confirmar antes de programar:**

- **Dónde se configura:** en INFO, con `-` y `+` como el brillo, de 250 en 250
  rpm (de 2000 a 6500, o apagado), guardado en `Preferences`; y `!cambio N` en
  la consola. INFO está llena: maqueta antes.
- **Cómo parpadea:** la pantalla alterna con un color fuerte unas 4 veces por
  segundo mientras las RPM están en el umbral o más, hasta 2 s. Vuelve a avisar
  cuando las RPM bajan 300 del umbral (al pasar el cambio). Cada parpadeo es un
  cuadro entero: le saca tiempo a las consultas solo mientras dura.
- **El pitido:** uno corto y más agudo que los de las alarmas (por ejemplo,
  80 ms a 4000 Hz); se elige probándolo con `!pitar cambio`.
- **Cuándo no avisa:** en 5ª, ni sin marcha (punto muerto, embrague), así no
  suena acelerando parado. Por eso va después de E24.
- **ZONA ROJA:** el aviso la reemplaza; la barra de las RPM sigue con la zona
  roja pintada desde 6000.
- En MEZCLA la RPM se lee cada ~2,5 s: ahí el aviso puede llegar tarde, como
  hoy la zona roja.

**Tareas:**

1. En `lib/obd`, con tests: el aviso (umbral, rearme, en 5ª, sin marcha, sin un
   valor actual) y los umbrales nuevos del refrigerante.
2. El parpadeo, el pitido nuevo y el ajuste en INFO, con sus casos en el
   render.
3. README: alarmas, INFO y consola.

**Verificación del usuario:** en el auto, con el aviso en un valor bajo (por
ejemplo, 2500): parpadea y suena al llegar, una vez por marcha, y no en 5ª ni
en punto muerto. La alarma del refrigerante no salta en un viaje normal.

## Ideas para después

Fuera del plan; solo si el usuario las pide:

- (La pantalla más fluida y `LCD_TE` pasaron a E17.)
- Compilar todo el proyecto optimizado (E17): con `-Os`, el programa baja de
  65 % a 50,5 % de 3 MB; con `-O2`, a 53,5 %, y todo anda más rápido. Hoy solo
  LVGL y el giro van a `-O2`. Hay que volver a probar todo en el auto (cambia
  cómo se compila lo ya verificado: BLE, sondeo, tareas).
- QSPI a 80 MHz (E17: el usuario decidió dejar 40). Con el envío en otra tarea
  no frena las consultas; ayudaría a que el envío entre en un barrido del
  panel. Hay que mirar que no aparezca basura.
- Mandar solo hasta la última fila del panel que cambió, siempre desde la 0
  (idea de E1): menos QSPI cuando lo que cambia queda a la izquierda.
- La animación de DS3 (E17): 20 cuadros/s de ~14 ms le sacan ~1 consulta por
  segundo (5,6 contra 6,4). Se puede bajar a ~10 cuadros/s o abaratar cada
  cuadro: hoy cada uno redibuja las nubes de todo el ancho, las rayas y las
  dos llantas giradas.
- Fuerza G: separar la pendiente de la aceleración con la velocidad de la ECU
  (la longitudinal es la derivada de la velocidad), y sacar "adelante" de las
  frenadas en línea recta, para una placa girada hacia el conductor.
- Los pitidos más fuertes, si con el motor andando se oyen poco.
  `VOLUMEN_PITIDO` ya está en 1 (2026-10-27), lo más que entra sin recortar, y
  así `!pitar` se oye bien. Lo que queda es recortar la onda a propósito (un
  `VOLUMEN_PITIDO` de más de 1 con un límite en `pitido()`, o subir el DAC del
  ES8311 de 0 dB, registro `0x32` en `0xBF`, que según el driver de Espressif
  llega a +32 dB en `0xFF`): más fuerte pero más áspero. O probar otra
  `FRECUENCIA_PITIDO_HZ`, por si el parlante rinde más en otra.
- Sincronizar los viajes por Wi-Fi al llegar a casa, para verlos en una app o
  un dashboard web (pedido del usuario, 2026-10-28, para más adelante). Lo que
  ya se sabe: con la llave sacada la placa sigue prendida unos minutos por el
  USB de la radio (**Datos descubiertos**), justo al llegar; Wi-Fi y BLE
  comparten la radio y hay poca RAM interna libre (ver E20); la red y su clave
  van en `Preferences`, no en el código. Qué se sube (los CSV de la microSD o
  los resúmenes de ANTERIORES) y adónde (un servidor propio, una planilla) se
  decide con el usuario.
- Un dinamómetro: la curva de potencia y torque en las ruedas contra las RPM,
  en una pasada a fondo en una marcha (pedido del usuario, 2026-10-28, para más
  adelante). La potencia es la fuerza por la velocidad; la fuerza, la masa por
  la aceleración (del IMU, o la derivada de la velocidad), más el aire
  (½ · densidad · Cx · área frontal · v²) y la rodadura (~1,2 % del peso).
  - Probado a mano con `VIAJE0007`: una pasada en 3ª de 84 a 130 km/h, con el
    acelerador en 86 % (el máximo de este auto). Con ~1200 kg, Cx 0,32,
    2,1 m² y 6 % más de masa por lo que gira (valores aproximados, sin
    verificar), da ~100–115 CV en las ruedas entre 5000 y 6000 rpm. Fábrica:
    120 CV a 6000 rpm, en el motor.
  - Lo que falta: datos más seguidos que una línea por segundo (las consultas,
    ~6 por segundo, y el IMU a 50); el peso del auto, cargado una vez;
    detectar la pasada (acelerador a fondo, una sola marcha, con la marcha de
    E24); la masa de lo que gira en cada marcha; y una página con la curva.
  - Necesita la fuerza G con el signo bien: en los registros del 2026-10-27 la
    longitudinal daba al revés; se arregló el 2026-10-28 y falta verlo en el
    auto (ver **Pendiente de probar**, Fuerza G).
  - La masa es lo que más pesa en el resultado: un 10 % de error en el peso es
    casi un 10 % en los CV. Solo en plano y donde sea legal.

El consumo no se puede estimar bien: el DS3 no da MAF (`10`) ni MAP
(`0B`).

## Datos descubiertos

Del proyecto anterior (mismo auto, adaptador Bluetooth clásico). Valen
para la ECU; los de tiempos se vuelven a medir con el adaptador nuevo.

| Dato                          | Valor | Etapa |
| ----------------------------- | ----- | ----- |
| Protocolo (`ATDPN`)           | `A6`: CAN 11 bit, 500 kbaud | E4 anterior |
| ECUs que responden            | Solo `7E8` (motor) | E4 anterior |
| PIDs soportados               | 17 en el servicio 01: `01 03 04 05 06 07 0C 0D 0E 0F 11 13 15 1C 21 34 47`. No están `0B`, `10`, `14`, `24`, `42` ni `44` | E4 anterior |
| Sondas O2                     | B1S1 de banda ancha (PID `34`) y B1S2 de banda angosta (PID `15`) | E4 anterior |
| `0A` (fallas permanentes)     | No soportado (`NO DATA`) | E4 anterior |
| Servicio 09                   | Solo `0904` y `0908`; el VIN no se puede leer | E4 anterior |
| Voltaje (`ATRV`)              | 12,4 V con contacto; 14,0–14,1 V en marcha | E4 anterior |
| Tiempos con el ELM SPP        | AT ~40 ms; `01PP1` ~75 ms; ~11 consultas por segundo | E1/E5 anterior |

De esta placa y el adaptador nuevo:

| Dato                          | Valor | Etapa |
| ----------------------------- | ----- | ----- |
| PSRAM y flash detectadas      | 8 MB de PSRAM (8189 KB libres) y 16 MB de flash física. ESP32-S3 rev 2. RAM interna libre con el BLE iniciado: 218–225 KB | E0    |
| Adaptador: nombre y modelo    | Anuncia `OBDBLE`, relleno con 23 bytes nulos, y el servicio `FFF0`. El nombre de GAP (`2A00`) también es `OBDBLE`, así que el `OBDII` que se ve en el celular no sale del BLE (quizás tiene también Bluetooth clásico). No tiene Device Information (`180A`): el modelo no se puede leer | E0    |
| Dirección y tipo              | `81:23:45:67:89:BA`, pública. No parece una dirección asignada de verdad (el primer byte tiene el bit de multicast); otros adaptadores iguales pueden tener la misma. -52 dBm en el banco | E0    |
| Servicio y características    | `1800` (`2A00` READ), `1801` (`2A05` INDICATE) y `FFF0`: `FFF1` WRITE, WRITE_NR, NOTIFY y `FFF2` WRITE, WRITE_NR. Notificaciones en `FFF1`; los comandos andan escritos sin respuesta en `FFF1` y en `FFF2` | E0    |
| MTU y tamaño de notificaciones | MTU 248. Las respuestas cortas llegan en una sola notificación con el `>` adentro (8–18 bytes). `0100` sin ECU: dos (14 y 40 bytes) | E0    |
| ¿Pide emparejamiento?         | No: conecta, se suscribe y escribe sin cifrar | E0    |
| Respuestas del ELM            | `ELM327 v2.1` (clon). Tras `ATZ` el eco queda activo. Las líneas terminan en `\r\n` (el anterior, `\r\r`); el parser ya lo acepta. `ATZ` sin el `\r\r` inicial. `ATRV` 11,9 V en el banco. `0100` sin ECU: `SEARCHING...` y `UNABLE TO CONNECT` en 4,4 s, y en la misma notificación vino `UNABLE TO CONNECT\r\n>` repetido: **puede llegar texto después del `>`** | E0    |
| Tiempos BLE (E0)              | Conexión 456–753 ms y servicios 799 ms. Con intervalo de 50 ms, escribiendo en `FFF1`: `ATZ` 99, `ATI` 110, `ATRV` 70–238 ms; en `FFF2`: `ATZ` 261, `ATE0` 311, `ATI` 386, `ATRV` 119–173 ms (pocas muestras) | E0    |
| Brillo cómodo de día / noche  | No se pudo medir: con esta placa la luz no se regula (fila siguiente). Queda siempre al máximo | E1, E6 |
| La luz según el valor (V2)    | Medido en el banco el 2026-10-27, el agente con `!luz` por el USB y el usuario mirando, de noche: con 90 o menos la luz se apaga del todo; con 97 ya se ve, muy oscura; 160 se lee oscura. Igual a 5 kHz que a 50 kHz. No es proporcional: hay un corte entre 90 y 97. Por eso los niveles de E15 (12, 40, 90, 160, 255) dejaban 3 de 5 apagados | E15 |
| ¿El brillo 0 apaga la pantalla? | Según el esquemático, sí (IO8 en alto apaga el AP3032). Pero en esta placa, hoy, ningún valor cambia la luz. Probado el 2026-10-26 (el agente por el USB, el usuario mirando la pantalla): PWM en GPIO8 de 0 a 255 (con el LEDC del core y con el del ESP-IDF, señal 74 en el pin), GPIO8 fijo en alto y en bajo sin PWM (la pata los lee bien), `0x53`/`0x51` al AXS15231B y un firmware mínimo con el código de la luz de Waveshare. La luz queda igual. La demo de fábrica sí regulaba: parece hardware (ver notas de E6) | E0, E6 |
| Refresco de pantalla          | Completo (dibujar todo y mandarlo): 96–131 ms. Solo el envío al panel: 32–33 ms, se cambie lo que se cambie | E1    |
| Memoria con pantalla y BLE    | RAM interna libre 164 KB (bloque mayor 123 KB); PSRAM libre 7972 KB | E1    |
| Conexión guardada / buscando  | Por dirección guardada: conecta en 389–1348 ms y queda listo en 974–1928 ms. Buscando: lo encuentra en 23–1526 ms y queda listo en 1536–1923 ms. Casi no hay diferencia, porque la búsqueda lo encuentra enseguida. De lo que tarda en quedar listo, ~530 ms son servicios, características y suscripción | E2    |
| Parámetros de conexión        | Con 7,5–15 ms pedidos queda en 15 ms (latencia 0, supervisión 2 s). El adaptador pide 25–35 ms dos veces por conexión; se rechaza (ver notas de E2) | E2    |
| Tiempo por comando AT         | Con 15 ms: `ATZ` 29–89, `ATE0` 59–89, `ATI` 35–90, `ATRV` 64–100 ms. Con 50 ms (E0): 70–390 ms. Con SPP era ~40 ms | E2    |
| Con la inicialización de E3   | El clon respeta `ATL0`: después, las líneas terminan en `\r` solo. Sin auto y con `ATSP6`, `0100` contesta `UNABLE TO CONNECT` en 131–146 ms (con `ATSP0`, 4,4 s). AT 26–95 ms, `ATRV` 45–133 ms | E3    |
| Memoria en `SIN ECU`          | RAM interna libre 157–158 KB (bloque mayor 111–117 KB), PSRAM libre 7972 KB. Sin pérdidas con 6 reconexiones seguidas | E3    |
| PIDs con el adaptador nuevo   | Iguales al anterior: 17 PIDs (`0100` `BE1EA811`, `0120` `80001001`, `0140` `02000000`, `0160` `NO DATA`); `0114`, `0124`, `0142`, `0144` y `0A` dan `NO DATA`; `0900` `11000000`; `03`/`07` sin códigos, luz apagada. Con `ATH1` se ve el relleno del frame: `7E8064100BE1EA811AA` | E3    |
| ¿Acepta `010C1`? ms con y sin | Sí, pero no acelera: `010C` y `010C1` tardan ~148 ms los dos (el anterior, 126 y 75 ms). `NO DATA` también tarda 133–148 ms | E3    |
| Consultas por segundo         | 6–7 (promedio 6,4), ~143 ms cada una: las consultas se llevan ~910 ms de cada segundo. El anterior, ~11. `NO DATA` sueltos en el 1–5 % de las consultas; una vez, 5 seguidos con el motor en marcha | E3    |
| Varios PIDs por pedido        | Sí: `010C0D050F` en 149 ms (igual que uno solo), en orden y en varios frames sin relleno. RPM 5–6 por segundo con contacto | E3    |
| Timeout del ELM               | Lo ignora: con `ATST05`–`ATST19` y `ATAT0`/`1`/`2` contesta `OK`, pero cada consulta sigue tardando ~148 ms | E3    |
| Voltaje y arranque            | 11,9 V con contacto, 9,9–11,7 V al arrancar, 13,6–13,7 V en marcha (motor frío). En los dos arranques no se cortó el BLE ni se reinició la placa (alimentada desde la notebook) | E3    |
| RAM interna y consultas por segundo con las páginas | Con el motor andando: RAM interna libre 128 KB (bloque mayor 87 KB; en E3, 157–158 KB) y 133 KB al arrancar, antes del BLE. Consultas por segundo: Motor 5–7 (RPM 5–6 por segundo), Aire/Volt y Carga 6–7, Mezcla 5–6 (140–168 ms). Refresco completo en Motor: 93 ms | E5    |
| Página Fallas en el DS3 | `01011` 102–150 ms, `03` 135–164, `07` 135–165, `0A` 120–150 (`NO DATA`): ~0,6 s la lectura entera. Luz apagada, sin códigos guardados ni pendientes: `Sin fallas` | E5    |
| Adaptador alimentado por el pin 1 | Al sacar la llave con el motor andando: dos consultas sin respuesta (timeout de 1 s), el BLE se corta a los ~3 s (`SIN CONEXION`) y el adaptador deja de anunciarse. Sin medir: si el pin 1 se corta al dar arranque (con el 16, en E3, el BLE no se cortaba) y cuánto tarda en conectar al poner el contacto | E5    |
| Versión de la placa           | 3.49B **V2** (`Rev1.1`, lo confirmó el usuario el 2026-10-26), con parlante. Cambian la luz (GPIO42), `EXIO_INT` (GPIO8), `LCD_TE` (GPIO21) y el reset de la pantalla (EXIO5); desde E15 el firmware usa los de la V2. Según los ejemplos y el esquemático de la V2, sin medir | E11, E15 |
| microSD                       | Según el esquemático y el ejemplo `04_SD_Card` de la V2: modo SD de 1 bit en CLK 41, CMD 39 y D0 40 (el ejemplo, a 40 MHz; el firmware, a 20); D3 (GPIO38) con 10 kΩ a 3,3 V, que deja la tarjeta en modo SD. Sin detección de tarjeta (`CD` a masa). Probado en el auto el 2026-10-27: monta, graba, y dos cortes de luz con el motor andando dejaron los archivos enteros. Sin reloj, la FAT les pone 1980-01-01. Los 6 registros del mismo día, con el reloj (E14), también enteros con el corte del final, y con la creación y la modificación en hora (la primera y la última línea) | E13, E14 |
| IMU (QMI8658)                 | Según el datasheet del QMI8658A y el ejemplo `03_I2C_QMI8658` de la V2 (2026-10-26, sin probar en la placa): `0x6B`, `WHO_AM_I` `0x05`; se reinicia con `0xB0` en `0x60` y queda `0x80` en `0x4D` (~15 ms); `CTRL1` arranca en big-endian (el firmware pone little-endian). Solo el acelerómetro, a 125 Hz, ±4 g y con su filtro de 4,5 Hz. Qué eje va a lo largo de la pantalla: E12 supuso x; los registros del 2026-10-27 dicen que la derecha es −x (la longitudinal daba al revés, ver **Pendiente de probar**) | E12   |
| RTC (PCF85063)                | Según el datasheet del PCF85063A y el ejemplo `02_I2C_PCF85063` de la V2 (SensorLib), sin probar en la placa: `0x51`. La hora en BCD, de `0x04` (segundos) a `0x0A` (año, de 2000 a 2099), con el día de la semana en `0x08`; el bit 7 de los segundos (`OS`) dice que el oscilador se detuvo y se borra al escribirlos. En `CONTROL_1` (`0x00`), el bit 5 lo detiene y el 1 lo pasa a 12 h: de fábrica, los dos en 0 | E14   |
| RTC y batería (esquemático)   | Según el esquemático de la wiki (2026-10-26, sin medir en la placa): el VDD del RTC llega por un diodo desde `3V3` y por otro desde `VBAT`, así que con la batería puesta no pierde la hora con la placa apagada. Con la placa apagada la batería alimenta solo al RTC, la protección, el cargador y el divisor de `BAT_ADC` (~20 µA en total). **Confirmado el 2026-10-27:** la hora se mantuvo una noche con el auto apagado (C32 no se midió) | E14   |
| Optimización del compilador   | Todo lo que compila PlatformIO (`src`, `lib/obd`, LVGL, el driver del panel, el core de Arduino y su BLE) va **sin ningún `-O`**, o sea a `-O0`: el `platformio-build.py` de Arduino-ESP32 3.3 toma los flags de `flags/c_flags` y `flags/cpp_flags`, que no lo traen (el `-Os` está en `pioarduino-build.py`, que no se usa). El ESP-IDF viene ya compilado a `-Os`. Programa: 65,2 % de 3 MB así; LVGL a `-Os`, 56,6 %; LVGL a `-O2`, 58,8 %; todo a `-Os`, 50,5 %; todo a `-O2`, 53,5 % | E17   |
| Reloj del QSPI                | El SPI del S3 sale de los 80 MHz del APB (o de los 40 del cristal) dividido por un entero: 80, 40 o 26,7 MHz. 60 no se puede. Queda en 40 (decisión del usuario); 80 sin probar | E17   |
| Volumen del parlante          | Probado por el usuario con E19 (2026-10-27): grabó y reprodujo, pero se oye bajo. La grabación se reproducía tal cual (micrófonos a 30 dB, el DAC del ES8311 a 0 dB y sin subirle el nivel), así que no se sabe cuánto es el parlante y cuánto la grabación. Los pitidos de E11 pasaron de 0,5 a 1 (el DAC sigue a 0 dB), y con eso `!pitar` "pita perfecto" (2026-10-27); con el motor andando, sin probar | E11, E19 |
| Micrófonos (esquemático)      | Según el esquemático y el ejemplo `08_Audio_Test` de la V2: ES7210 en `0x40`; los dos micrófonos de la placa (MEMS analógicos) en MIC1 y MIC2, y su `SDOUT1` al GPIO6 por 51 Ω; `SDOUT2` y el `ASDOUT` del ES8311 no van a ningún pin. Con el I2S en full duplex (16 kHz, 16 bits, MCLK de 256 veces) y el ES7210 como esclavo sin TDM, grabó (E19, descartada) | E19   |
| Partición NVS (`Preferences`) | 20 KB (`nvs` en `0x9000`, tabla `app3M_fat9M_16MB.csv`): 5 páginas de 4 KB, una siempre libre. La usan la dirección del adaptador (`obd`, E2), el brillo (`pantalla`, E15) y los viajes (`viajes`, E22: `ultimo` y `v0` a `v9`, de 40 bytes). Sin medir lo que tarda una escritura en la placa: `Viajes: guardado ... en N ms` lo dice | E22   |
| `LCD_TE` (V2)                 | Los ejemplos de Waveshare no lo usan ni mandan `TEON` (`0x35`). El firmware lo prende con `0x35 0x00` y lo lee en el GPIO21. Sin medir en la placa: si llega y a qué frecuencia barre el panel (`!info`, `LCD_TE N barridos/s`; en la prueba en el auto no se pasó) | E17   |
| Pantalla en el auto (E17)     | Con la rama de E17, estacionado en ralentí (`VIAJE0003.CSV`, 2026-10-27). Por cuadro: giro ~7,2 ms (máximo 11; a `-O0`, ~20), envío ~13,8 (máximo 17,4), espera 0. Dibujo: DS3 ~14 ms, 20 cuadros/s; Motor ~9 ms, 4–6 cuadros/s; Info ~3 ms; al cambiar de página, 11–29 ms | E17   |
| Consultas por segundo y pantalla (E17) | Mismo registro: Motor 6,4 (6–7 en 30 s, como en E3), Info 6,6, Aire/Volt, Carga, Mezcla y Viaje 6–7. DS3 5,6 (4–7 en 38 s): 20 cuadros/s de ~14 ms, ~280 ms de cada segundo dibujando. FUERZA G 6–7, pero sin redibujar (auto y placa quietos: 0 cuadros/s). Fallas, 3 los dos segundos de la entrada (la lectura de las fallas, E5) | E17   |
| Escribir por el USB sin que lean | Arduino-ESP32 3.3 (`HWCDC.cpp`): el búfer de salida es de 256 bytes y, lleno, con un host enchufado que no lee (la Mac sin el monitor, la radio del auto), cada escritura espera hasta 20 veces `tx_timeout_ms` (100 ms). Probado el 2026-10-28: con la Mac sin el monitor, la placa quedó ~1 min trabada al arrancar, con la trompa del DS3 asomando y sin touch, igual que en el auto. Desde E23, 4 KB y sin espera | E23   |
| USB del auto                  | La placa va al de la consola, el mismo de la radio: con datos y un host del otro lado. Se prende al abrir la puerta, antes de poner la llave (lo contó el usuario el 2026-10-28), y sigue después de sacarla: en los registros del 2026-10-27, de 31 s a 4 min 15 s, con la placa intentando conectar cada 15 s. Hasta E23 el README decía que se cortaba con la llave | E23   |
| Marchas (km/h por 1000 rpm)   | Caja manual de 5. Medianas de los registros del 2026-10-27 (2416 lecturas con 6 km/h o más, 1000 rpm o más y fuera de MEZCLA, donde la RPM llega cada 2,5 s): 1ª 8,07 (el 90 % entre 7,0 y 9,3), 2ª 14,57 (14,0–15,0), 3ª 20,95 (20,5–21,3), 4ª 27,24 (26,8–27,5), 5ª 33,74 (33,3–33,9). El 98 % cae en una marcha; el resto, con el embrague o cambiando | E24   |
| Refrigerante en marcha        | Mismos registros: con el motor caliente regula en 103–105 °C y llega a 108 parado o en tráfico (promedio 103 parado, 97 a más de 90 km/h con máximo 104). Nunca 110 en 6 viajes. Según catálogos de repuestos y foros (sin una fuente oficial), el termostato del EP6C abre a 105 °C y es pilotado (la ECU lo abre antes con carga), y el electroventilador entra cerca de 108 | E25   |
| MEZCLA con el motor frío      | Mismos registros: con `Abierto frio` la ECU da lambda 0,995–0,996, la sonda post 0,45–0,46 V y los ajustes en 0, fijos; el lazo se cierra a los 29–85 s del arranque. Con el corte de inyección (`Abierto carga`, soltando el acelerador), lambda 2,000 y la sonda ~0 V. En MEZCLA, 6,5 consultas/s (800 s de `VIAJE0007`) | E24   |

## Notas de traspaso

Cada agente agrega acá qué hizo, qué cambió respecto del plan, la API
final si corresponde y lo que quedó pendiente.

### E0

Hecho:

- `git init` ya estaba (lo hizo el usuario); `main` sigue sin commits.
- `platformio.ini`: `[env:esp32s3]` con `board_build.arduino.memory_type = qio_opi`,
  `flash_mode = qio`, `f_flash = 80000000L`, `board_upload.flash_size = 16MB`,
  particiones `app3M_fat9M_16MB.csv` (el tamaño máximo, 3 MB, lo saca la
  plataforma de la tabla) y los tres `-D` del plan. `[env:native]` igual
  que el anterior.
  - La imagen dice `Flash mode: DIO` en el encabezado aunque se pida QIO.
    Es lo normal en el S3: el Arduino IDE hace lo mismo
    (`FlashMode.qio.build.flash_mode=dio`, `boot=qio`) y el bootloader
    pasa a QIO. La variante `qio_opi` tiene `CONFIG_SPIRAM_MODE_OCT`.
- Copiados sin cambios: `lib/obd/`, `test/` y `.clang-format`. También
  `.gitignore`, `fix_intellisense.py` (mismo problema de IntelliSense con
  el core 3.3) y `.vscode/extensions.json`. `pio test -e native`: 45/45,
  como allá.
- `include/config.h`: pines de **Hardware de la placa** (`PIN_PANTALLA_*`,
  `PIN_RETROILUMINACION`, `PIN_TOUCH_*`, `PIN_I2C_*`, `DIRECCION_*`),
  `BLE_NOMBRE_LOCAL`, `BLE_NOMBRES_OBD[]` y `BLE_SERVICIOS_OBD[]` (lo que
  marca un posible OBD; E2 los puede usar para buscar), `BUSQUEDA_BLE_S`
  (10), `TIMEOUT_CONSOLA_MS` (10000) y `ESPERA_MONITOR_MS` (2000: al
  arrancar espera el USB para no perder el informe).
- `src/main.cpp`: retroiluminación apagada, informe de arranque (chip,
  PSRAM física con `esp_psram_get_size()`, flash física con
  `esp_flash_get_physical_size()` y la configurada, RAM interna libre,
  motivo del último reinicio) y el loop de consola. `!info` lo repite,
  por si el monitor abrió tarde.
- `src/consola.h/.cpp`: igual que la del anterior pero sobre
  `Explorador` y con `infoPedida()`. Comandos: `!escanear`,
  `!explorar N`, `!usar N [S]`, `!desconectar`, `!info`.
- `src/explorar.h/.cpp` (`class Explorador`):
  - `!escanear`: búsqueda activa de 10 s. Lista número, RSSI, dirección,
    tipo, nombre y servicios anunciados; con `*` los que parecen OBD, que
    van primero (después, por RSSI). Hasta 30; si se llena, un posible OBD
    reemplaza a otro. Si había conexión, la corta antes. Saca los bytes
    nulos del final del nombre: el adaptador lo rellena (`OBDBLE` + 23
    `\0`). E2 tiene que hacer lo mismo antes de comparar por nombre.
  - `!explorar N`: conecta (si no estaba), muestra el tiempo de conexión,
    MTU, intervalo, latencia y supervisión, y cada servicio (`S1`, `S2`...)
    con sus características y propiedades. De `1800` y `180A` lee los
    valores (nombre, fabricante, modelo, versiones). Al final dice si el
    enlace quedó cifrado. Queda conectado.
  - `!usar N [S]`: se suscribe a la primera característica con `NOTIFY`
    (o `INDICATE`) y, para escribir, anota la primera que no sea esa (si
    no hay, la misma). Es del servicio `S` o, sin `S`, del primero de
    `BLE_SERVICIOS_OBD` que tenga las dos (si no, de cualquiera que las
    tenga). En el adaptador del DS3 queda `FFF1` para notificaciones y
    `FFF2` para escribir. Escribe sin respuesta si la característica
    acepta `WRITE_NR`. La `S` opcional no estaba en el plan: es por si
    la elección automática no acierta.
  - Líneas sin `!`: `Explorador::enviar()`, el único camino hacia el
    adaptador. Valida con `comandoPermitido()` antes de tocar el BLE,
    descarta y muestra lo que quedó (`-- descartado:`), escribe el
    comando con `\r` y junta hasta el `>` o 10 s. Log
    `<< "texto\r\r" OK 123 ms, 2 notificaciones de 20+5 bytes`; lo que
    llegue después del `>` en la misma notificación sale como
    `-- despues del '>':`.
  - Las notificaciones llegan en la tarea de NimBLE y se copian a un
    `MessageBuffer` de FreeRTOS de 4 KB: un mensaje por notificación, así
    se sabe el tamaño de cada una. Si no entran se cuentan y se avisa.
  - Emparejamiento: no se empareja por iniciativa propia. Si el adaptador
    lo pide (o rechaza una operación por falta de cifrado), se hace "Just
    Works" y **sin vínculo**, así no se guarda nada en la flash. Se avisa
    con `BLE: el adaptador pidio emparejamiento...`.
  - La pérdida de conexión se informa en `atender()` (callback
    `onDisconnect`).

**Cambio respecto del plan: GPIO8 va en `HIGH`, y la pantalla no queda
apagada.** Los ejemplos de Waveshare (también el de fábrica) calculan el
duty como `255 - brillo`, así que `LOW` sería el máximo y se puso `HIGH`.
Pero con `HIGH` el usuario la vio prendida, con píxeles de colores (el
panel sin inicializar). Puede que ningún nivel la apague del todo. E1 lo
mide con el PWM.

Para E2, de la biblioteca `BLE` del core 3.3.12 sobre NimBLE:

- `BLEClient::connect()` ignora el timeout que se le pasa: espera hasta
  30 s (`m_connectTimeout`, privado y sin setter) y vuelve recién
  después del intercambio de MTU. Para un timeout más corto hay que
  cancelar o ir a `ble_gap_connect()` directo.
- `getServices()` borra y vuelve a descubrir los servicios en cada
  llamada: deja colgadas las características de antes y sus callbacks.
  Hay que llamarla una vez por conexión.
- `BLESecurity::m_forceSecurity` viene en `true`: si se activa la
  seguridad sin `setForceAuthentication(false)`, empareja al conectar.
- El MTU preferido es 256 (`CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU`).
  Parámetros iniciales de conexión (`ble_gap.h`): intervalo 30–50 ms,
  supervisión 2,56 s; `updateConnParams()` los cambia ya conectado.
  `!explorar` muestra los que quedaron.
- `CONFIG_BT_NIMBLE_NVS_PERSIST=y`: si algún día se activa el vínculo,
  queda en la NVS de NimBLE, aparte de `Preferences`. `olvidar()` lo
  tendría que borrar.

Verificado por el agente: `pio run` compila (RAM 9,2 %, flash 24,1 % de
3 MB), sin warnings propios también con `-Wall -Wextra`.
`pio test -e native` pasa (45/45).

Probado en el banco (2026-10-24), primero por el usuario y después por
el agente desde el USB (el usuario lo autorizó): `!info` muestra 8 MB de
PSRAM y 16 MB de flash. `!escanear` encuentra el adaptador (`OBDBLE`,
`FFF0`, -51 dBm). `!explorar 1` y `!usar 1` conectan; `ATZ`, `ATE0`,
`ATI` y `ATRV` responden escribiendo en `FFF1` y en `FFF2`. `04` da
`RECHAZADO`. Datos en **Datos descubiertos**.

- Un comando AT tarda 70–110 ms, casi el doble que con SPP (~40 ms). Con
  intervalo de 50 ms, el comando sale en un evento de conexión y la
  respuesta vuelve en otro. Para E2: pedir un intervalo corto (7,5–15 ms)
  con `updateConnParams()` debería acercarlo a SPP. Medirlo.
- Las respuestas cortas llegan en una sola notificación con el `>`
  adentro. `0100` sin ECU llegó en dos, y la segunda traía
  `UNABLE TO CONNECT\r\n>` repetido. Para E2: juntar notificaciones
  hasta el `>` y descartar lo que venga después.
- Escribir en `FFF2` pareció más lento que en `FFF1`, pero son pocas
  muestras. Para E2: medir las dos con el intervalo corto.

Decidido con el usuario: `!escanear` y `!explorar` quedan en el firmware
(E2 los suma a la consola, como `!descubrir` en el anterior).

### E1

Hecho:

- `lib/axs15231b/`: el driver `esp_lcd` del AXS15231B (Espressif,
  Apache-2.0) copiado sin cambios del ejemplo `10_LVGL_V9_Test`, con
  `esp_lcd_touch`, que el driver incluye. `LICENSE` y `README.md` dicen
  de dónde sale.
- `platformio.ini`: `lvgl/lvgl @ 9.3.0`, `-DLV_CONF_INCLUDE_SIMPLE`,
  `-Iinclude` y `-DLV_KCONFIG_IGNORE`. Sin este último no compila: los
  `.S` de LVGL incluyen `lv_conf_kconfig.h`, que con `ESP_PLATFORM` busca
  `esp_attr.h`, y al ensamblador no le llegan las rutas del framework.
- `include/lv_conf.h`: solo lo que cambia (lo demás queda por defecto).
  RGB565, `malloc` del sistema, sin sistema operativo, avisos de LVGL al
  monitor serie, un assert reinicia la placa con `abort()` en vez de
  colgarla, tema oscuro, Montserrat 14 y 20 (traen el `°`) y solo la
  etiqueta como widget. E4 prende los que necesite.
- `src/fuentes/fuente_numeros_110.c`: Montserrat Medium (la de LVGL,
  licencia OFL) a 110 px, 4 bpp, solo ` %+,-./0-9:°`. 31 KB de flash. Las
  de LVGL llegan a 48 px (unos 5 mm acá); esta da dígitos de unos 10 mm.
  Alto de línea 109 px: entra debajo de una barra de 28. Se generó con:

  ```text
  npx lv_font_conv@1.5.3 --font .pio/libdeps/esp32s3/lvgl/scripts/built_in_font/Montserrat-Medium.ttf \
    --size 110 --bpp 4 --format lvgl --no-compress -r 0x20,0x25,0x2B-0x3A,0xB0 \
    --lv-include lvgl.h --lv-font-name fuente_numeros_110 -o src/fuentes/fuente_numeros_110.c
  ```

- `include/config.h`: `PANTALLA_GIRADA` (gira la imagen 180°),
  `RETROILUMINACION_INVERTIDA` (duty = 255 − brillo, como Waveshare),
  `BRILLO_INICIAL` (128) y `PASO_BRILLO` (16).
- `src/pantalla_hw.h/.cpp`, a partir del `lvgl_port.c` del ejemplo:

  ```cpp
  class PantallaHw {
  public:
    bool iniciar();          // panel, touch y LVGL; queda en negro con BRILLO_INICIAL
    bool iniciada() const;
    void atender();          // lv_timer_handler(); desde loop() y mientras se espera al ELM
    void ponerBrillo(uint8_t nivel);  // 0-255
    uint8_t brillo() const;
    uint32_t medirRefresco(uint32_t &envio);  // ms del refresco completo y del envío
    bool toqueCrudo(uint16_t &x, uint16_t &y) const;  // última lectura sin girar
  };
  ```

  - Panel por QSPI (`SPI3_HOST`, 40 MHz) con el reset y los comandos del
    ejemplo. En QSPI el driver solo manda `CASET`: no se puede elegir la
    fila de inicio, así que la imagen va siempre entera, de arriba abajo
    (`RAMWR` y después `RAMWRC`), en 10 tandas de 64 filas.
  - **Cambio respecto del ejemplo:** LVGL en modo `DIRECT` con un solo
    búfer de 640×172 en la PSRAM. Dibuja solo lo que cambió y, al
    terminar la última zona, se manda la imagen entera. El ejemplo usa
    `FULL` (redibuja todo cada vez) y gira el cuadro entero a un tercer
    búfer.
  - El giro se hace por tanda: cada columna de la imagen es una fila del
    panel. De paso se dan vuelta los bytes del píxel (el panel espera
    primero el alto). Hay dos búferes de tanda con DMA en la RAM interna
    (2 × 22 KB): se prepara uno mientras se manda el otro. Un semáforo
    contador, que da la interrupción de fin de envío, evita pisar uno que
    todavía se está mandando.
  - Touch: bus I2C propio (`I2C_NUM_1`, SDA 17 / SCL 18, 300 kHz,
    `i2c_master`), con la lectura del ejemplo: 11 bytes de pedido y 14 de
    respuesta (`[1]` dedos, `[2..5]` x e y de 12 bits). El crudo va a lo
    largo del panel; se pasa a vertical como en el ejemplo
    (`x = crudoY`, `y = 640 − crudoX`) y de ahí a horizontal con la misma
    cuenta que la imagen, así que el touch gira junto con `PANTALLA_GIRADA`.
  - Retroiluminación con `ledcAttach` a 50 kHz y 8 bits, como el ejemplo.
    Queda en 0 hasta mandar el primer cuadro negro, para no mostrar la
    basura del panel.
  - Sin tarea de LVGL: tick con `millis()` (`lv_tick_set_cb`) y
    `lv_timer_handler()` en `atender()`.
- `src/consola`: `+` y `-` cambian el brillo (`ajusteBrillo()`).
- `src/main.cpp` de prueba: barra de estado con el toque y el brillo, un
  contador de prueba con los números grandes que cambia cada 100 ms,
  `92°` en la fuente grande y `21,5 °C` en la chica, muestras de los
  colores propuestos para la noche (blanco, gris, verde, ámbar, rojo y
  cian sobre negro) y una marca que sigue al dedo. El monitor serie
  muestra cada toque con las coordenadas giradas y crudas. `!info` agrega
  el refresco y el brillo. La consola y la exploración BLE de E0 siguen.
  La pantalla se inicia antes que el BLE, por la RAM interna con DMA.

Medido por el agente en la placa (2026-10-24), sin poder ver la
pantalla: arranca sin errores con pantalla, touch y BLE. Refresco
completo 96–131 ms, envío 32–33 ms. RAM interna libre 164 KB (bloque
mayor 123 KB), PSRAM libre 7972 KB. Flash 42 % de 3 MB. `+` y `-`
cambian el duty. Al arrancar apareció un toque suelto que no se repitió
en 15 s: puede ser una lectura falsa al iniciar.

Para E3: cada cambio en pantalla cuesta unos 33 ms de envío más lo que
tarde LVGL en dibujarlo, y bloquea `loop()`. Si molesta, se puede:

- Mandar solo hasta la última fila del panel que cambió (siempre desde la
  0). Ayuda solo si lo que cambia queda del lado del inicio del panel.
- Acelerar el giro, que se lleva unos 20 de los 33 ms (el QSPI solo
  serían ~11 ms), por ejemplo escribiendo dos píxeles por vez.

Verificado por el usuario en la placa (2026-10-24, "funciona todo"):
imagen derecha sin franjas ni parpadeo con `PANTALLA_GIRADA = false`,
colores como dicen las muestras, touch en las cuatro esquinas, brillo con
`+` y `-` en el sentido correcto (`RETROILUMINACION_INVERTIDA = true`) y
el número grande legible.

No quedaron anotados: de qué lado queda el USB-C, el brillo cómodo de día
y de noche, ni si el brillo 0 apaga del todo. Se completan en E6 (brillo)
y E7 (montaje).

### E2

Hecho (salió de la rama de E1, antes de que E0 y E1 se mergearan a `main`):

- `include/config.h`: el adaptador (`BLE_NOMBRE_REMOTO` `OBDBLE`,
  `BLE_SERVICIO_ELM` `FFF0`, `BLE_NOTIFICACION_ELM` `FFF1`,
  `BLE_ESCRITURA_ELM` `FFF2`), los parámetros de conexión que se piden
  (`BLE_INTERVALO_MIN`/`MAX` 7,5–15 ms, `BLE_SUPERVISION` 2 s) y los
  tiempos: `BUSQUEDA_ELM_S` 6, `TIMEOUT_CONEXION_MS` 4000,
  `TIMEOUT_COMANDO_MS` 1000, `TIMEOUT_ATZ_MS` 3000, `REINTENTO_BT_MS`
  5000, `REINTENTO_ELM_MS` 5000 y `PERIODO_ATRV_MS` 2000 (los nombres de
  los que ya estaban en el anterior, para portar E3 más fácil).
- `src/elm327.h/.cpp` con la interfaz del anterior:

  ```cpp
  enum class EstadoTransporte { OK, TIMEOUT, DESCONECTADO, RECHAZADO };
  struct ResultadoComando { EstadoTransporte estado; String cruda; uint32_t ms; };
  enum class FaseConexion { POR_DIRECCION, BUSCANDO };
  class Elm327 {
  public:
    bool iniciar();  // nuevo: inicia el BLE; conectar() lo llama si hace falta
    bool conectar(void (*aviso)(FaseConexion) = nullptr);
    void olvidar();
    bool conectado();
    void desconectar();
    ResultadoComando enviar(const char *comando, uint32_t timeoutMs, bool registrar = true);
    void mientrasEspera(void (*funcion)());
  };
  const char *nombreEstado(EstadoTransporte estado);
  ```

  - `iniciar()` es nuevo y público: el `Explorador` usa el mismo BLE, así
    que `main.cpp` lo llama en `setup()`. Configura el emparejamiento como
    en E0: sin clave, sin vínculo y solo si el adaptador lo pide.
  - `conectar()`: primero la dirección guardada en `Preferences`
    (`obd`/`dir` como texto y `obd`/`tipo`). Va como texto porque, con
    NimBLE, `BLEAddress` da vuelta los bytes al construirla desde un
    arreglo y `getNative()` no. Si falla, busca durante hasta 6 s el
    nombre `OBDBLE` (sin los bytes nulos del final) o el servicio `FFF0`,
    corta la búsqueda apenas lo encuentra, conecta y guarda la dirección
    si cambió. `aviso` se llama antes de cada fase. Peor caso con el
    adaptador apagado: 4 s + 6 s.
  - **Timeout de conexión:** `BLEClient::connect()` con NimBLE ignora el
    timeout que se le pasa y espera 30 s. Un `esp_timer` de una vez
    llama a `ble_gap_conn_cancel()` a los `TIMEOUT_CONEXION_MS`.
    *Probado:* con una dirección falsa (versión temporal), "no conecto
    (4001 ms)" y después encontró el adaptador por búsqueda.
  - Al conectar: pide el intervalo corto (antes de leer los servicios),
    lee los servicios una sola vez, se suscribe a `FFF1` y escribe en
    `FFF2` sin respuesta.
  - **El adaptador pide 25–35 ms dos veces por conexión y se rechaza.** Si
    se aceptara, la biblioteca le contestaría con los parámetros con los
    que conectó (30–50 ms, `m_pConnParams`, privado y sin setter) y se
    perdería el intervalo corto. Con el rechazo no pasó nada malo.
  - Las notificaciones se copian a un `StreamBuffer` de 4 KB desde la
    tarea de NimBLE; `enviar()` es lo único que lo lee.
  - `enviar()`: igual que el anterior. Valida con `comandoPermitido()`
    antes de tocar el BLE, descarta y muestra lo que quedó (incluido lo
    que llegue después del `>`, como el `UNABLE TO CONNECT` repetido de
    E0), escribe el comando con `\r` y junta hasta el `>` o el timeout.
    Mientras espera, llama a `mientrasEspera()` cada ~2 ms (también
    mientras busca el adaptador, no mientras conecta). Antes de cada
    comando informa lo que pasó en el BLE: emparejamientos, pedidos de
    parámetros, cambios de intervalo y bytes perdidos.
  - `conectado()` se entera de la desconexión por el callback del stack.
  - `olvidar()`: desconecta, borra `obd`/`dir` y `obd`/`tipo`, y llama a
    `ble_store_clear()` por si alguna vez quedó un vínculo.
  - No está la recuperación automática del anterior
    (`FALLAS_PARA_OLVIDAR`): era por el PIN de SPP, y acá no hay vínculo.
- `src/explorar`: quedan `!escanear` y `!explorar N`. **Salió `!usar`**:
  con su `enviar()` era un segundo camino hacia el ELM. `!explorar` usa
  su propia conexión y la corta al terminar (con el timeout de 30 s de la
  biblioteca).
- `src/consola`: `Consola(Elm327&, Explorador&)`, portada del anterior:
  `!reconectar`, `!olvidar`, `!log` y `detalle()`, más `!info`,
  `!escanear`, `!explorar N`, `+` y `-`. Las líneas sin `!` van a
  `Elm327::enviar()`. Antes de explorar suelta el ELM (el adaptador
  acepta una sola conexión y, conectado, no se anuncia) y pide reconectar.
- `src/main.cpp` de banco, como E1 del anterior: conecta, `ATZ`, `ATE0`,
  `ATI`, `ATRV`; después `ATRV` cada 2 s, callado salvo que falle o con
  `!log`. Si el ELM no contesta, reintenta la inicialización a los 5 s;
  si se pierde la conexión, reconecta cada 5 s. La pantalla queda con una
  línea gris con el estado de la conexión (`aviso` y `mientrasEspera`
  la mantienen al día); las pantallas de estado son de E3.

Probado por el agente en la placa (2026-10-24), con el adaptador en el
banco: primera conexión buscando (1,5 s hasta quedar listo) y dirección
guardada; `!reconectar` por dirección (~1–1,9 s); `ATZ`, `ATE0`, `ATI` y
`ATRV` responden; `04` da `RECHAZADO`; `!escanear` y `!explorar 1`
sueltan el ELM y se reconecta solo; `!olvidar` vuelve a buscar de cero.
RAM interna libre 159 KB. Tiempos en **Datos descubiertos**.

Para E3:

- Mientras conecta (hasta 4 s por intento) la pantalla no se actualiza:
  `connect()` bloquea. Mientras busca, sí.
- Los comandos AT tardan 30–100 ms. `ATRV` es de los más lentos
  (64–100 ms): parece que el clon tarda en medir.
- `mientrasEspera()` llama a `lv_timer_handler()`, que puede tardar un
  refresco (~33 ms) y sumarse al tiempo del comando.

Verificado por el usuario en el banco (2026-10-24, "funciona todo"): al
apagar el adaptador se informa la pérdida y al prenderlo reconecta solo.
En su log, después de prenderlo, `ATZ` contesta con eco
(`"ATZ\rELM327 v2.1\r\n"`, 104 ms) porque el ELM arranca de cero; los
demás tiempos coinciden con los de arriba.

Sin anotar: cuánto tarda cada intento con el adaptador apagado (tendría
que ser ~10 s: 4 s por dirección + 6 s buscando).

### E3

Hecho (rama `e3-monitor-completo`, desde `main` con E0–E2):

- **Portados del anterior:**
  - `src/datos.h/.cpp` y `src/sondeo.h/.cpp`, sin cambios salvo uno: con
    `pantalla = true`, `textoDato()` usa `°C` en UTF-8 (las fuentes de
    LVGL lo traen) en vez del `0xF8` de la Nokia. El lazo abreviado sigue.
  - `src/descubrimiento.h/.cpp` sin cambios: la misma serie que E4 del
    anterior, para poder comparar (incluye 20 × `010C` y 20 × `010C1`).
  - `main.cpp`: la máquina de estados del anterior sin LEDs, cámara,
    botón, páginas ni fallas (son de E4 y E5). Inicializa con `ATZ`,
    `ATE0`, `ATL0`, `ATS0`, `ATH0`, `ATSP6`, `ATAT1` y después `ATI`
    (de `ELM327 v2.1` queda `v2.1`). Al encontrar la ECU carga los mapas
    y lee `ATDPN`. En `LEYENDO`, una consulta por vuelta con el sufijo
    `1`, la tabla de datos y consultas por segundo cada 1 s, y 5 fallas
    seguidas llevan a `SIN_ECU` (después del auto, además con 3 s sin
    respuesta: ver E3c). Lo que hacía el anterior en E5. Los mapas se
    reintentan (ver E3b).
  - `config.h`: los tiempos que faltaban, con los valores del anterior
    (`TIMEOUT_BUSCAR_ECU_MS`, `REINTENTO_ECU_MS`, `PERIODO_LENTO_MS`,
    `PERIODO_PANTALLA_MS`, `PERIODO_TABLA_MS`, `ESPERA_ANTES_DE_BT_MS`,
    `ESPERA_TRAS_BROWNOUT_MS`, `FALLAS_PARA_SIN_ECU`,
    `FALLAS_PARA_REINICIAR_ELM` y `VIGENCIA_MS`).
- **Detector de brownout activo** (no se toca el registro). Si el último
  reinicio fue por brownout, se ve `ALIMENTACION` durante
  `ESPERA_TRAS_BROWNOUT_MS` antes de prender el BLE.
- **`src/pantalla.h/.cpp`**, con LVGL:

  ```cpp
  struct LineaDato { const char *etiqueta; const char *valor; bool actual; };
  class Pantalla {
  public:
    explicit Pantalla(PantallaHw &hw);
    void iniciar();  // crea los objetos; sin pantalla, nada hace nada
    void inicio();
    void alimentacionBaja();
    void conectando(bool buscando);  // la dibuja en el acto
    void sinConexion(const char *motivo, uint32_t segundos);
    void iniciandoElm(const char *comando);
    void elmNoResponde(uint32_t segundos);
    void buscandoEcu(const char *version);
    void sinEcu(const char *version, const LineaDato &voltaje, const char *motivo, uint32_t segundos);
    void descubriendo(uint16_t hechos, uint16_t total);
    void datos(const char *version, const LineaDato &principal, const LineaDato *lineas,
               uint8_t cantidad);  // provisoria
  };
  ```

  - Barra de 28 px (título con color según el estado y un texto a la
    derecha) y debajo 4 renglones de 36 px en Montserrat 20, con etiqueta
    a la izquierda y valor a la derecha, como las líneas de la Nokia.
  - Colores de la prueba de E1: título blanco al iniciar, cian mientras
    conecta o busca, ámbar en `SIN ECU` y `El ELM no responde`, rojo en
    `SIN CONEXION` y `ALIMENTACION`, verde en `ECU OK`.
  - Pantalla provisoria (`datos()`): barra `ECU OK` con `ELM v2.1` a la
    derecha; RPM con los números grandes de E1, alineada a la derecha en
    310 px para que no salte; a la derecha refrigerante, velocidad,
    admisión y `Volt ELM`. Lo que no es válido y actual va en gris. La
    fuente grande no tiene letras: `N/D` sale en Montserrat 20.
  - Una etiqueta se toca solo si cambia su texto, color o fuente: cada
    cambio le cuesta a LVGL redibujar y mandar la imagen entera (~33 ms).
- **`PantallaHw::dibujarYa()`** (nuevo): `lv_refr_now()`. Lo usa
  `conectando()`, porque `connect()` bloquea hasta 4 s sin llamar a
  `mientrasEspera()`.
- **Consola:** `!descubrir` (solo en `LEYENDO`, como en el anterior) y
  `antesDeExplorar(funcion)`. `!info` agrega estado, versión del ELM,
  protocolo y PIDs soportados.
- **`test/test_obdble/`**: 5 tests con las respuestas del adaptador nuevo.
  Del banco: eco tras `ATZ`, `\r\n` antes de `ATL0` y `\r` después,
  `12.0V` y `0100` sin ECU con `ATSP6` y con `ATSP0`. Del auto: `0100`
  (termina en `\r\r`), `0120` con `NO DATA`, `ATDPN` = `6` y lo nuevo de
  `!descubrir` (`AT@1`, `0100` con `ATH1`, los mapas, valores en ralentí
  y un `NO DATA` suelto). Y datos del DS3 con `\r\n` (armados: es lo que
  llegaría si el ELM se reinicia solo). Salvo los armados, están copiados
  de los logs. Para entender al adaptador nuevo, el parser no necesitó
  cambios (los pedidos de varios PIDs son aparte, más abajo).

Cambios respecto del plan y del anterior:

- **La pantalla se actualiza también mientras se espera al ELM.**
  `mientrasEspera()` llama a `dibujar()` cada `PERIODO_PANTALLA_MS`,
  además de `lv_timer_handler()`. Así un dato que se vence durante una
  espera larga (un comando escrito a mano, hasta 10 s) pasa a `--`. En el
  anterior era un límite conocido. Durante `!descubrir` no se dibujan los
  datos: queda `DESCUBRIENDO`.
- **`!escanear` y `!explorar` sacan los datos antes de bloquear.** Sueltan
  el ELM y no atienden la pantalla por 10 s o más; con
  `antesDeExplorar()`, `main` pasa antes a `SIN CONEXION` /
  `Explorando BLE`. Después reconecta solo, como en E2.
- **Arranque:** la pantalla se inicia primero y muestra `DS3 OBDII`
  mientras espera el monitor serie (`ESPERA_MONITOR_MS`).
  `ESPERA_ANTES_DE_BT_MS` (1 s) se cuenta desde el arranque, así que en
  el auto, sin monitor, no suma a esos 2 s.
- **Motivos sin abreviar** (`Conexion perdida`, `Respuesta invalida`...):
  ya no está el límite de 14 caracteres.
- Sin cambios en `lib/obd` ni en la lista blanca. Los comandos nuevos
  respecto de E2 (`ATL0`, `ATS0`, `ATH0`, `ATSP6`, `ATAT1`, `ATDPN`,
  `0100`–`0160`, `01PP1` y los de `!descubrir`) ya estaban permitidos.

Verificado por el agente: `pio run` compila (RAM 9,7 %, flash 43,2 % de
3 MB), sin warnings propios también con `-Wall -Wextra -Wshadow`;
`pio test -e native` pasa (53/53).

Probado por el agente en la placa (2026-10-24), con el adaptador en el
banco y sin auto, por el monitor serie (sin ver la pantalla). El usuario
lo autorizó:

- Arranca y sigue la secuencia: `CONECTANDO_BT` (por la dirección
  guardada, listo en 1,3–1,9 s), `INICIANDO_ELM` con los 7 comandos y
  `ATI`, `BUSCANDO_ECU` y `SIN_ECU (No responde)`. Vuelve a buscar la ECU
  cada 10 s.
- **Con `ATSP6` y sin auto, `0100` contesta `UNABLE TO CONNECT` en
  131–146 ms** (con `ATSP0` eran 4,4 s). El ciclo de `SIN ECU` casi no
  pasa por `BUSCANDO ECU`.
- **El clon respeta `ATL0`:** desde ahí las líneas terminan en `\r` solo.
- Comandos AT en la inicialización: 26–95 ms. `ATRV` (12,0 V): 45–133 ms.
- `04` da `RECHAZADO`; un `ATRV` escrito a mano contesta; `!descubrir`
  sin ECU se rechaza y lo avisa; `!log` muestra el `ATRV` cada 2 s de
  `SIN_ECU`.
- `!reconectar` (7 veces), `!olvidar` (encuentra el adaptador en 425 ms
  y guarda la dirección) y `!escanear` (pasa a `BT_PERDIDO (Explorando
  BLE)`, lista 14 dispositivos con `OBDBLE` primero y reconecta solo).
- Memoria: RAM interna libre 163 KB antes de conectar y 157–158 KB
  conectado (bloque mayor 111–117 KB); PSRAM libre 7972 KB. Con 6
  reconexiones seguidas no se pierde memoria. Una búsqueda (`!escanear`,
  `!olvidar`) achica un poco el bloque mayor, sin acumularse.
- Refresco completo con las pantallas de estado: 51–59 ms (envío 32 ms).
- Ojo: abrir el puerto desde Python (pyserial) reinicia la placa, aunque
  DTR y RTS estén en falso antes de abrir (pasó el 2026-10-25, con el
  motor en marcha; reconectó sola en ~4 s).

Sin probar: todo lo que se ve en la pantalla, apagar y prender el
adaptador, y el auto.

**Pruebas del usuario, en el banco** (adaptador alimentado, sin auto; el
firmware ya está subido):

1. Al arrancar (desenchufar y enchufar el USB-C) se ve, en orden:
   `DS3 OBDII` / `Iniciando...` (blanco), `CONECTANDO` (cian, 1–2 s),
   `INICIANDO ELM` con `> ATZ`... (cian, menos de 1 s) y `SIN ECU`
   (ámbar). En `SIN ECU`: `ELM v2.1` con el voltaje (`12.0V`) en blanco a
   la derecha, `No responde` y `Reintento en N s` contando de 10 a 1.
   `BUSCANDO ECU` dura ~150 ms: puede que apenas se vea.
2. El texto se lee entero, sin cortes ni franjas, y nada parpadea.
3. Apagar el adaptador: en 2 s o menos pasa a `SIN CONEXION` (rojo),
   `Conexion perdida`, y el voltaje desaparece. Después alterna entre
   `CONECTANDO` (hasta 4 s), `BUSCANDO ADAPTADOR` (hasta 6 s) y
   `SIN CONEXION` / `No conecta con el adaptador` / `Reintento en 5 s`.
4. Prender el adaptador: vuelve solo a `SIN ECU` en el próximo intento.
5. Con la placa alimentada desde un cargador (sin la notebook) también
   llega a `SIN ECU`.

**E3b, primera sesión (usuario, 2026-10-24):** contacto, arranque, unos
50 s en ralentí con el motor frío y apagado. Log:
`docs/descubrimiento/2026-10-24.log`, de la extensión de VS Code que
genera `ArduinoSerialLog_*.txt`. **No tiene `!descubrir`:** el comando
no se ejecutó (no hay ninguna respuesta de la consola). Lo más probable
es que ese monitor no mande el fin de línea.

Lo que muestra el log (análisis del agente):

- Recién conectado no había ECU (`UNABLE TO CONNECT`); a los 10 s la
  encontró. `0100` igual que en el anterior (`BE1EA811`), `ATDPN` = `6`.
- **`0120` contestó `NO DATA`:** quedaron 14 PIDs y Lambda en `N/D` toda
  la sesión. En el anterior, `0120` siempre contestó; parece una falla
  suelta como las de abajo. **Arreglado:** cada mapa se intenta
  `INTENTOS_MAPA` (3) veces antes de darlo por no soportado.
- **6–7 consultas por segundo** (promedio 6,4), con contacto y en
  ralentí. El anterior hacía ~11. No hay tiempos por consulta (con `!log`
  apagado no salen). Para la próxima, la tabla agrega el promedio de cada
  consulta (`6 consultas/s, 140 ms cada una`) y cada falla dice cuánto
  tardó. La segunda sesión confirmó que es el adaptador (ver abajo).
- **8 `NO DATA` sueltos en ~890 consultas** (~1 %), 6 de ellos en 1,2 s
  en ralentí. Nunca 5 seguidos: no llegaron a `SIN ECU`.
- Valores, con el motor frío: refrigerante 21–23 °C, admisión 18 °C,
  ralentí 1100–1190 rpm y después 930–990; lazo `Abierto frio` y a los
  ~29 s `Cerrado`; ajuste corto 0–1,6 %; sonda post 0,46 V; 11,9 V con
  contacto, 11,7 V al arrancar y 13,6–13,7 V en marcha. Que coincidan con
  el tablero lo confirma el usuario.
- **Al dar arranque no se cortó el BLE** ni se reinició la placa.
- Al sacar la llave: 5 `NO DATA` seguidos en 0,6 s y `SIN_ECU (Sin
  datos)`. Con el motor apagado y el contacto puesto, los datos siguieron
  válidos (RPM 0).
- En la tabla del monitor, `Lazo`, `O2 post` y `Aj largo` pasan a `--`
  cada tanto. Es el sondeo: con 6–7 consultas por segundo, un dato que
  no está en pantalla se vuelve a leer recién a los ~3,1 s (2,5 s de
  `PERIODO_LENTO_MS` más lo que tarda en ganarles a los visibles), y
  `VIGENCIA_MS` es 3 s. En pantalla no pasa: lo visible se lee en cada
  vuelta y `Volt ELM` va cada 2 s. Queda para E4, que rehace las
  prioridades del sondeo con las páginas.

**E3b, segunda sesión (usuario, 2026-10-25):** con el firmware que
reintenta los mapas. `!descubrir` con contacto y en ralentí, con
`pio device monitor -f log2file`. Log: `docs/descubrimiento/2026-10-25.log`
(empieza en `ATZ`: el monitor abrió después del arranque).

**E3c (agente), comparación con `../esp32-ds3-obd2/docs/descubrimiento/2026-10-23.log`:**

- **La ECU contesta igual:** los mismos mapas y 17 PIDs, los mismos
  `NO DATA` (`0160`, `0114`, `0124`, `0142`, `0144`, `0A`), `0900` igual,
  `03`/`07` sin códigos y luz apagada. `0120` contestó en todas las
  búsquedas (4 veces) al primer intento.
- **Con `ATH1`** este adaptador muestra el byte de relleno del frame
  (`7E8064100BE1EA811AA`); el anterior no. No afecta: se usa `ATH0`.
- **El sufijo `1` no acelera nada:** `010C` 147 ms y `010C1` 148 ms de
  promedio (el anterior: 126 y 75 ms). Los tiempos van de a ~15 ms (132,
  148, 163), el intervalo de conexión. Y **`NO DATA` tarda lo mismo
  (133–148 ms)**: parece que el clon espera siempre una ventana fija,
  aunque la ECU conteste antes y aunque se pida una sola respuesta.
- **Las consultas por segundo las limita el adaptador:** 6,4 por segundo
  a ~143 ms cada una suman ~910 ms de cada segundo. Lo que hace el
  programa fuera de las consultas (pantalla, tabla) se lleva menos de
  100 ms.
- **`NO DATA` sueltos:** 1 de 20 en cada medición de `!descubrir` (con el
  motor en marcha) y **una vez 5 seguidos en 0,6 s con el motor en
  ralentí (119–177 ms cada uno), que llevó a un `SIN_ECU` falso** de
  ~10 s. **Arreglado:** ahora hacen falta las 5 fallas seguidas y además
  `SILENCIO_PARA_SIN_ECU_MS` (3 s, igual que `VIGENCIA_MS`) sin ninguna
  respuesta buena de la ECU. Al pasar a `SIN_ECU` ya no queda ningún dato
  vigente. Al sacar la llave tarda ~3 s en vez de ~0,6 s. Cada falla
  dice cuántas van seguidas y cuánto hace que no hay respuesta.
- Valores, motor frío: ralentí ~1150 rpm al arrancar y 890–1000 después;
  refrigerante 21–24 °C; lambda 0,99–1,09 con el lazo cerrado; 9,9 V al
  arrancar y 13,6–13,7 V en marcha.

**Prueba del timeout (agente, 2026-10-25, en el auto con contacto y el
motor apagado, con permiso del usuario):** para subir las consultas por
segundo se probó desde la consola, con el firmware que ya tiene la regla
nueva de `SIN_ECU`. **El adaptador ignora el timeout:** contesta `OK` a
`ATST19`, `ATST10`, `ATST0A` y `ATST05` (100 a 20 ms), con `ATAT1`,
`ATAT0` y `ATAT2`, pero cada consulta sigue tardando 133–163 ms (casi
siempre 148). Al final, `!reconectar` (`ATZ`) lo dejó como estaba. En las
dos corridas hubo rachas de `NO DATA` (5 seguidos y más), también con los
valores de fábrica, y la regla nueva evitó el `SIN_ECU` falso en todas.

Consecuencia: con este adaptador, cada consulta cuesta ~148 ms, se pida
lo que se pida, así que hay como mucho ~6,7 por segundo, y las RPM se
renovaban cada ~0,9 s. Se le plantearon al usuario dos caminos: prioridad
a las RPM (~3–4 por segundo) o varios PIDs por consulta. Eligió el
segundo, que se aprovecha de que la ventana fija se cobra por consulta.

**Varios PIDs por consulta (pedido del usuario, 2026-10-25):**

- **Lista blanca** (`comandos`): el servicio 01 acepta de 1 a
  `MAX_PIDS_POR_PEDIDO` (6) PIDs, con o sin el dígito de cantidad
  (`010C0D050F`, `010C0D1`). `09` sigue con un solo PID, y los demás
  servicios no cambian. El comando más largo pasa de 8 a 15 caracteres.
  Los tests que rechazaban `010C0D` y parecidos ahora los aceptan; con 7
  PIDs, o con varios en otro servicio, se rechaza.
- **Parser** (`respuesta`): con varios PIDs, `datos` trae cada PID seguido
  de sus bytes (solo se saca el `41`), y el primero puede ser cualquiera
  de los pedidos: la ECU puede contestar en otro orden. Con más de 7 bytes
  la respuesta viene en varios frames, que el parser ya armaba.
- **`separarPids()`** (`pids`): separa esa respuesta según el largo de
  cada PID en la tabla. Se detiene en un PID que no conoce.
- **`Sondeo::tocan()`**: las magnitudes que tocan, de la más atrasada a la
  menos, sin `VOLT_ELM`.
- **`main.cpp`:**
  - Al encontrar la ECU, `probarVariosPids()` pide juntos los visibles
    que soporta (`010C0D050F` en el DS3; hasta `INTENTOS_AL_ENCONTRAR`
    veces si da `NO DATA`). Si vuelven todos, en `LEYENDO` cada consulta
    pide hasta `PIDS_POR_CONSULTA` (6) de las que tocan: los 4 visibles y
    hasta 2 de los demás. Si no, sigue de a uno como el anterior. El monitor
    serie dice cuál de los dos usa.
  - Si `INCOMPLETAS_PARA_DE_A_UNO` (3) respuestas seguidas traen menos
    PIDs que los pedidos, vuelve a pedir de a uno (si no, el dato que
    falta no se renovaría nunca).
  - El pedido va sin el sufijo `1`: con una respuesta de varios frames,
    un ELM podría cortarla en el primero.
  - `ATRV` sigue aparte, cada 2 s.
  - **La pantalla se actualiza en cada consulta:** `leer()` llama a
    `dibujar()` antes de consultar, así lo que llegó se dibuja mientras se
    espera al adaptador. Antes era cada 0,5 s y no se habría notado la
    mejora.
  - La tabla del monitor agrega cuántas lecturas de RPM hubo por segundo.
- **`!descubrir`** agrega 20 × `010C0D050F` con su promedio, después de
  las dos mediciones de siempre.
- Una respuesta `INVALIDA` muestra lo crudo en el monitor (`enviar()`
  solo lo muestra si falló el transporte).
- Tests: `pio test -e native` 54/54 (con las respuestas reales de varios
  frames en `test/test_obdble/`). Compila sin warnings propios.

Probado por el agente en el auto (2026-10-25, contacto puesto y motor
apagado, con permiso del usuario). Log:
`docs/descubrimiento/2026-10-25-varios-pids.log`.

- **El adaptador y la ECU aceptan pedidos de varios PIDs:** `ECU: pedidos
  de varios PIDs: SI`. Contestan en el orden pedido, en varios frames
  (`00A\r0:410C00000543\r1:0D000F41\r\r`), sin el relleno del último.
- **Tarda lo mismo que uno solo:** `010C0D050F` 149 ms de promedio en
  `!descubrir` (`010C` y `010C1`: 148 ms). Con 6 PIDs y 3 frames, 131–162 ms.
- **RPM 5–6 lecturas por segundo** (antes ~1), con 6–7 consultas por
  segundo. Los datos que no están en pantalla ya no pasan a `--` en la
  tabla: entran de a 2 en cada consulta.
- Una respuesta `INVALIDA` (`01150C050D0F`) en ~250 consultas; el mismo
  pedido en otro orden vino bien. Sin lo crudo no se sabe qué fue; por eso
  ahora se muestra.

**Verificado por el usuario en el auto** (2026-10-25, con este firmware y
el motor en marcha: "todo probado funciona bien, se ve más fluido"). La
lista que tenía era la de **Pruebas del usuario que faltan** (abajo): la
pantalla, los valores contra el tablero, que no aparezca `SIN ECU` con el
motor andando, `SIN ECU` al sacar la llave y las RPM siguiendo a la aguja.
No detalló cada punto.

No quedaron anotados:

- Cuántas consultas por segundo quedan con el motor en marcha, cuando la
  pantalla se redibuja en cada una. Se ve en la tabla del monitor.
- El firmware de la placa es el de la prueba de varios PIDs, sin el
  cambio posterior que muestra lo crudo de una respuesta `INVALIDA`. No
  cambia el funcionamiento.
- Las pruebas de pantalla en el banco (apagar y prender el adaptador),
  salvo que entren en el "todo probado".

**Pruebas del usuario que faltan:**

1. Las de pantalla en el banco (lista de más arriba).
2. Subir el firmware de esta rama (después de la segunda sesión cambiaron
   la regla de `SIN_ECU` y se agregaron los pedidos de varios PIDs).
3. En el auto, con el monitor abierto: que al encontrar la ECU diga
   `ECU: pedidos de varios PIDs: SI`, y en la tabla, `RPM 6/s` o cerca.
   Un `!descubrir` mide cuánto tarda `010C0D050F`.
4. En el auto, mirando la pantalla:
   - En `ECU OK` (verde), las RPM en grande y a la derecha refrigerante,
     velocidad, admisión y `Volt ELM`. Que coincidan con el tablero y que
     las RPM sigan a la aguja de cerca.
   - Unos minutos en ralentí sin ningún `SIN ECU`. Si en el monitor
     aparecen `NO DATA` seguidos, que no pase a `SIN ECU` mientras el
     motor anda.
   - Al sacar la llave, a los ~3 s pasa a `SIN ECU` (ámbar), sin números
     a la vista.

Para E4:

- `Pantalla::datos()` y `dibujarDatos()` de `main.cpp` son provisorios:
  los reemplaza `paginas`. `VISIBLES` de `main.cpp` pasa a salir de la
  página actual (`Sondeo::visibles()`), y también la prueba de
  `probarVariosPids()`, que hoy pide juntos los visibles de la pantalla
  provisoria.
- Con varios PIDs por consulta (~148 ms cada una, pidan 1 o 6), una página
  de hasta 6 datos se renueva entera en cada consulta. Mezcla tiene 5
  (`03`, `06`, `07`, `34`, `15`): entra en una. `PIDS_POR_CONSULTA` es 6;
  lo que sobra de cada consulta se usa para los datos que no se ven.
- La pantalla se redibuja en cada consulta (`leer()` llama a `dibujar()`
  antes de consultar). Si una página nueva tarda mucho en dibujarse, se
  nota en las consultas por segundo.
- El touch todavía no se usa. Los eventos de LVGL corren dentro de
  `lv_timer_handler()`, que también se llama mientras se espera al ELM:
  un evento no puede usar el ELM. Que deje un pedido y `loop()` cambie
  de página, como hacía el botón del anterior.
- `textoDato()` usa punto decimal (`14.1V`), como el anterior; la prueba
  de E1 usaba coma. Lo decide el diseño.

### E4

**E4a, maqueta (agente, 2026-10-25):** lienzo en
https://claude.ai/artifact/41eQde6wkKUL81ZuDnHBGX (privado, del usuario),
con las pantallas a 640×172, Montserrat y los colores del firmware:

- Prototipo: se cambia de página tocando la mitad izquierda o derecha, o
  con pestañas en la barra (se elige en Tweaks). Puntos de página a la
  derecha de la barra.
- Motor en tres versiones: A (RPM grande y refrigerante y velocidad a la
  derecha, como la provisoria), B (RPM grande con una barra de
  revoluciones de 0 a 7000 abajo, que se mueve como la aguja) y C (tres
  columnas iguales).
- Aire/Volt (admisión y voltaje; `Volt ECU` no, el DS3 no lo da), Mezcla
  (lazo, ajustes, lambda como barra rica/pobre en ámbar y cian con la
  marca en verde, sonda post con barra), Fallas con y sin códigos (E5) e
  Info con las acciones de E6.
- Etiquetas en gris `#8C8C8C`, más claro que el `#707070` del firmware
  (que queda para los datos no vigentes). Hacen falta fuentes nuevas: 28,
  36 o 44, 48 y 64 px (las de LVGL llegan a 48; 64 se genera como la de
  110).

Elegido por el usuario (2026-10-25): tocar a los costados para cambiar de
página, Motor B y el botón BOOT además del touch. Además pidió:

- Aire/Volt igual que Motor B: la temperatura de admisión en grande con
  una barra de 0 a 80 °C abajo, y el voltaje a la derecha.
- Una página nueva, **CARGA**, con carga del motor (`04`), avance (`0E`)
  y acelerador (`11`), los tres soportados por el DS3, cada uno con su
  barra (0–100 %, avance de −10 a 40°). `0E` no está en la tabla de
  `pids`: hay que agregarlo (A/2 − 64, grados antes del PMS).
- Quedan 6 páginas: Motor, Aire/Volt, Carga, Mezcla, Fallas e Info. La
  maqueta ya está así.

El usuario confirmó la maqueta con esos cambios el 2026-10-26 (ver E4b).

Para E4b (lo que se sabe hasta acá):

- `src/paginas.h/.cpp` con las 6 páginas como en la maqueta (las medidas
  exactas están en el HTML de cada pantalla del lienzo). Reemplaza
  `Pantalla::datos()` y `dibujarDatos()`; las pantallas de estado siguen
  en `pantalla`. Lo de LVGL queda en `pantalla_hw`, `pantalla` y
  `paginas`.
- Navegación: tocar la mitad derecha = siguiente, la izquierda =
  anterior, solo en `LEYENDO`. El evento de LVGL corre dentro de
  `lv_timer_handler()`, que también se llama mientras se espera al ELM:
  que deje un pedido y `loop()` cambie de página. En Info, los botones de
  E6 tendrán prioridad sobre las zonas.
- Botón BOOT (GPIO0, atrás de la placa): pulsación corta = página
  siguiente, con antirrebote (como `boton` del anterior, que usaba GPIO16).
  Es pin de arranque: se lee como entrada después del boot.
- Nuevas magnitudes en `datos`: carga (`04`), avance (`0E`) y acelerador
  (`11`). `0E` va a la tabla de `lib/obd/pids` con su test.
- El sondeo toma los visibles de la página actual (`Sondeo::visibles()`),
  y `probarVariosPids()` también. Cada página tiene como mucho 6 PIDs:
  entra entera en una consulta.
- Fuentes: Montserrat 28, 36 y 48 vienen con LVGL (prenderlas en
  `lv_conf.h`); la de 110 ya está en `src/fuentes/`. Todas traen el °.
- Colores: etiquetas `#8C8C8C`, datos no vigentes `#707070` (el `GRIS`
  de hoy), puntos de página blanco y `#444444`, fondo de las barras
  `#262626`, barra de lambda ámbar (rica) y cian (pobre) con la marca en
  verde cerca de 1. Las convenciones de `N/D`, `...` y `--` no cambian.
- Fallas (contenido de E5) e Info (acciones de E6): por ahora un
  marcador, como hizo el anterior en su E6.

**E4b (agente, 2026-10-26):** con la maqueta confirmada. Antes de
programar, el usuario eligió además:

- La marca de lambda en verde hasta 0,03 de 1; más allá, ámbar (rica) o
  cian (pobre), como las mitades de la barra.
- Toda la pantalla cuenta para cambiar de página, también la barra de
  arriba (en la maqueta, solo los 144 px de abajo).
- Info con lo que ya se sabe: adaptador, versión del ELM, protocolo, PIDs
  y consultas por segundo. RSSI y los botones quedan para E6.

Hecho:

- `lib/obd/pids`: PID `0E` (avance, A/2 − 64, unidad `grad`), con tests de
  la fórmula y de `separarPids()` con `04`, `0E` y `11`.
- `datos`: magnitudes `CARGA` (`04`), `AVANCE` (`0E`) y `ACELERADOR`
  (`11`). Los ajustes salen con signo (`+1.6`). `textoValor()` da el
  número sin la unidad, para las páginas. Salió `lazoCorto()`: en Mezcla
  entra el texto completo (`Abierto carga` ocupa 193 de 245 px).
- `dibujo.h/.cpp` (nuevo): los colores y las funciones chicas de LVGL que
  estaban en `pantalla.cpp` y que `paginas` también usa (`ponerTexto`,
  `ponerPosicion`, `mostrar`, `caja`, `rectangulo`, `etiqueta`...), más
  `yDeBase()`. Todas tocan el objeto solo si cambia.
- `pantalla`: sale `datos()` (la provisoria de E3). Entran
  `pagina(titulo, numero, total)`, que pone el título en blanco y un punto
  por página, y `areaPaginas()`, el área de 640×144 donde `Paginas` arma
  las suyas. Las pantallas de estado ocultan el área.
- `paginas.h/.cpp` (nuevo):

  ```cpp
  struct InfoConexion {
    const char *version;         // "v2.1"
    const char *protocolo;       // "6", o "A6" si lo eligió en automático
    uint8_t pids;                // PIDs soportados en el servicio 01
    int16_t consultasPorSegundo; // -1: todavía no se midió
  };
  constexpr uint8_t MAX_VISIBLES = 6; // entran en una consulta
  class Paginas {
  public:
    explicit Paginas(Pantalla &pantalla);
    void iniciar();             // después de Pantalla::iniciar()
    void cambiar(int8_t paso);  // +1 la siguiente, -1 la anterior; da la vuelta
    const char *titulo() const;
    uint8_t visibles(Magnitud lista[MAX_VISIBLES]) const; // las de la página actual
    uint8_t primeras(Magnitud lista[MAX_VISIBLES]) const; // las de Motor
    int8_t tomarToque();        // -1, +1 o 0 desde la última llamada
    void dibujar(uint32_t ahora, const InfoConexion &info);
  };
  ```

  - Las 6 páginas con las medidas de la maqueta. Las y de los textos son
    de la línea de base (pasadas del CSS con las métricas de cada fuente):
    así un valor que cambia de fuente queda en el mismo renglón. `N/D` en
    el número grande va en Montserrat 48 (la de 110 no tiene letras).
  - Visibles: Motor `RPM`, `REFRIGERANTE`, `VELOCIDAD`; Aire/Volt
    `ADMISION`, `VOLT_ELM`; Carga `CARGA`, `AVANCE`, `ACELERADOR`; Mezcla
    `LAZO`, `AJ_CORTO`, `AJ_LARGO`, `LAMBDA`, `SONDA_POST`. Fallas e Info,
    ninguna.
  - El número y la unidad son etiquetas aparte; la unidad (gris, 20 px)
    se ve solo con un valor actual. Con `--`, `...` o `N/D` va el texto
    solo, en gris, como siempre; las barras quedan vacías y la marca de
    lambda no se ve.
  - Barras con rectángulos de LVGL (sin el widget `bar`): RPM de 0 a 7000
    con la zona roja desde 6000, admisión de 0 a 80 °C, carga y acelerador
    de 0 a 100 %, avance de −10 a 40° rellenando desde el 0, lambda de 0,8
    a 1,2 y sonda post de 0 a 1 V.
  - Las fuentes de LVGL no traen tildes, `×` ni `·`: `admision`, `x1000`
    y comas en Info.
  - Fallas: un marcador (`Todavia no se leen`). Info: `OBDBLE`
    (`BLE_NOMBRE_REMOTO`), versión, protocolo con su nombre y
    `17 PIDs, 6 consultas/s` (antes de la primera tabla, solo los PIDs).
  - Toque: `LV_EVENT_CLICKED` en la raíz. Ningún objeto de la barra ni de
    las páginas recibe toques, así que el evento llega siempre ahí; x menor
    que 320 es la anterior. Solo cuenta con el área de páginas a la vista.
- `boton.h/.cpp`: el del anterior, en GPIO0 (BOOT). `PIN_BOTON` y
  `ANTIRREBOTE_MS` (30) en `config.h`. Siempre pasa a la siguiente;
  mantenerlo apretado no repite.
- `lv_conf.h`: Montserrat 28, 36 y 48. La flash pasa de 43,2 % a 50,1 %.
- `main.cpp`:
  - `atenderPaginas()` toma el toque y el botón, y solo en `LEYENDO` (y
    fuera de `!descubrir`) cambia la página, actualiza los visibles del
    sondeo, escribe `Pagina: X` en el monitor y dibuja. Se llama desde
    `loop()` y desde `alEsperar()`: la página cambia aunque haya una
    consulta en curso, sin esperar los ~148 ms. No usa el ELM.
  - El sondeo toma los visibles de la página actual. Lo que no está a la
    vista se sigue leyendo cada 2,5 s, así que al cambiar de página casi
    siempre hay valores vigentes enseguida.
  - `probarVariosPids()` prueba con los PIDs de la página actual; si son
    menos de dos (Aire/Volt, Fallas, Info), con los de Motor. Ya no
    incluye `VOLT_ELM`, que no es un PID: con la lista vieja no pasaba,
    con las páginas sí podía.
  - La página se mantiene al pasar por `SIN ECU`; al arrancar, Motor.
  - La tabla del monitor guarda las consultas por segundo para Info.

Verificado por el agente:

- `pio run` compila (RAM 9,9 %, flash 50,1 % de 3 MB). Con `-Wall
  -Wextra -Wshadow`, sin warnings nuevos salvo el parámetro del
  constructor de `Paginas` con el nombre del miembro (como ya tenían
  `Pantalla` y `Consola`). `pio test -e native` 54/54.
- **Render en la PC** (`herramientas/render/`, pedido del usuario después
  de verlo): LVGL 9.3 compilado para la PC con `pantalla`, `paginas`,
  `dibujo`, `datos`, `lib/obd`, `lv_conf.h` y las fuentes del proyecto, y
  un `PantallaHw` de mentira. `make -C herramientas/render -j8` (después
  de un `pio run`; LVGL se compila la primera vez, ~7 s) deja un PNG por
  pantalla y `todas.png` en `salida/`, y sale con error si falla alguna
  revisión de las de abajo. Las 6 páginas salen como la maqueta,
  también con valores extremos (6500 rpm, −12 °C, 188 km/h, avance
  negativo, `Abierto carga`, lambda 0,91 en ámbar) y vencidos (`--` en
  gris, barras vacías).
  - Con un touch simulado: izquierda −1 y derecha +1, también sobre la
    barra; con `SIN ECU` a la vista, nada.
  - Dibujar dos veces lo mismo no deja nada para redibujar en ninguna
    página (en la placa, cada redibujo cuesta ~33 ms de envío). Se
    encontró y arregló un caso: los puntos de página se ocultaban y se
    volvían a mostrar en cada dibujo.
  - Los objetos de las páginas piden ~27 KB de `malloc` en la PC, con
    punteros de 64 bits. En la placa salen de la RAM interna (había
    157–158 KB libres conectado): falta medirlo con `!info`.

Sin probar (falta la placa): todo lo que se ve en la pantalla real, el
touch y el botón BOOT, la RAM interna con las páginas y las consultas por
segundo en Mezcla, que pide 5 PIDs (3 frames).

**Pruebas del usuario:**

1. En el banco, sin auto: llega a `SIN ECU` como en E3, y tocar la
   pantalla o apretar BOOT no hace nada. `!info`: la RAM interna libre
   (antes 157–158 KB conectado).
2. En el auto, con el monitor abierto:
   - Arranca en `MOTOR`, con el primer punto en blanco: RPM en grande, la
     barra que sigue a la aguja, refrigerante y velocidad.
   - Tocar la mitad derecha recorre `AIRE/VOLT`, `CARGA`, `MEZCLA`,
     `FALLAS`, `INFO` y vuelve a `MOTOR`; la izquierda, al revés. Que
     cambie enseguida y que también ande tocando sobre la barra.
   - BOOT (atrás de la placa) pasa a la siguiente; mantenerlo apretado no
     repite.
   - El monitor dice `Pagina: X` en cada cambio, y la tabla sigue en ~6
     consultas por segundo también en Mezcla.
   - Valores contra el tablero. En Carga, que el acelerador suba al pisar
     y el avance se mueva con las RPM.
   - Mezcla: la marca de lambda en verde cerca de 1.
   - Info: `OBDBLE`, `v2.1`, `6, CAN 11b 500k` y `17 PIDs, N consultas/s`.
   - Que se lea bien de día y de noche.
3. Al sacar la llave: `SIN ECU` a los ~3 s, sin números, como en E3.
4. `!info` con el motor andando: la RAM interna libre.

**Verificado por el usuario en el auto** (2026-10-26, de día, con el
firmware de E5 y el monitor abierto): páginas, toques, BOOT, valores y
consultas por segundo ("funcionó todo bien"; detalle en las notas de E5).
Faltaba verlo de noche: el usuario lo dio por resuelto el 2026-10-27, con el
brillo de E15. En Fallas e Info el sondeo quedaba espaciado; lo
arregló E5.

Para E5:

- Con `herramientas/render` se ven las pantallas nuevas antes de subir:
  agregar en `render.cpp` los casos de Fallas (con y sin códigos).
- La vista de Fallas es `armarFallas()` en `paginas.cpp`, hoy un
  marcador. La lectura (al entrar y cada 30 s mientras se ve) tiene que
  saber cuándo se entra en la página: `atenderPaginas()` de `main.cpp` es
  el lugar, como `cambiarPagina()` del anterior, que llamaba a
  `reiniciarFallas()`. Falta algo como `Paginas::muestraFallas()`.
- Ya están las fuentes de 28 (códigos) y 48 (`Sin fallas`) de la maqueta.

Para E6:

- Los botones de Info van a la derecha (x 424–632 en la maqueta). Un
  objeto tocable recibe el toque y no llega a la raíz, así que no cambia
  la página: tienen prioridad solos. Hace falta prender `LV_USE_BUTTON` o
  usar objetos con `LV_OBJ_FLAG_CLICKABLE`.
- RSSI y la dirección del adaptador van en `InfoConexion`.

### E5

Hecho (rama `e5-codigos-de-falla`, desde `main` con E4). Antes de
programar, el usuario eligió (2026-10-26) lo que la maqueta no resolvía:

- **4 renglones de 36 px** en vez de 3 de 44. Si hay más de 4 códigos se
  alternan grupos cada 3 s, y el título dice `FALLAS 1/2`.
- **Error de lectura:** la columna izquierda como siempre, con el servicio
  que falló en ámbar (`Pendientes: error`), y a la derecha los códigos del
  que sí contestó. Nunca `Sin fallas`.
- **Un código guardado y pendiente a la vez va una sola vez**, como
  guardado. Las cantidades de la izquierda siguen siendo las de cada
  servicio.
- **Con la luz encendida y 03 y 07 sin códigos no es `Sin fallas`:** la luz
  en ámbar y `Ningun codigo guardado ni pendiente`.

Lo que se hizo:

- **`lib/obd/dtc`** (ya estaba: se copió en E0 con lo de E7 del anterior):
  - Sale la `pantallaFallas()` de la Nokia (renglones de 14 caracteres) y
    sus tests. No la usaba nadie.
  - `pantallaFallas(lectura, vuelta)` → `PantallaFallas {vista, grupos,
    grupo, cantidad, renglones[4]}`. `VistaFallas`: `LEYENDO`,
    `SIN_FALLAS` (`sinFallas()` y además la luz no encendida) o `CODIGOS`.
    Cada código va una vez, con el primer tipo en que aparece (`TipoDtc`):
    guardados, permanentes y pendientes. Los de un servicio que no contestó
    bien no están; los de los otros, sí. Grupo número `vuelta % grupos`.
  - `errorAlLeer()`: `03` o `07` no contestaron bien.
  - `textoCantidad()`: `2 guardadas`, `1 pendiente` o `Pendientes: error`.
  - `LARGO_DESCRIPCION` (14), el largo que ya tenía la tabla de
    descripciones.
  - No cambian `interpretarCodigos()`, `sinFallas()`, `textoLuz()` ni la
    tabla de `descripcionDtc()`.
- **`config.h`:** `PERIODO_FALLAS_MS` (30 s), `REINTENTO_FALLAS_MS` (5 s) y
  `PERIODO_GRUPO_FALLAS_MS` (3 s), los valores del anterior.
- **`paginas`:**
  - API: `muestraFallas()`, y `dibujar(ahora, info, fallas, reintento)`
    recibe la última lectura y los segundos hasta el reintento.
  - Izquierda: `Luz de falla` y debajo `Encendida` (ámbar), `Apagada`
    (blanco) o `--` (gris, si `01011` falló); más abajo las cantidades de
    guardados y pendientes en gris, o en ámbar si fallaron. Los
    permanentes, solo si `0A` trajo alguno (tres renglones). Separador en
    x 220.
  - Derecha: código en Montserrat 28 y descripción en 20, centrados en el
    renglón como en la maqueta. La descripción empieza en x 348, 8 px más
    cerca que en la maqueta: así entra la más ancha con `(pendiente)`
    (`Sens. mariposa (pendiente)`, 279 de 284 px). Los pendientes van en
    gris `#8C8C8C` con `(pendiente)`; los permanentes en blanco con
    `(permanente)`, o `(perm.)` si no entra. Sin descripción (los `P1xxx`
    de la marca), el código solo.
  - Al centro: `Leyendo...` (gris, 48 px) y `Sin fallas` (verde, 48 px) con
    `Luz de falla apagada, ningun codigo guardado ni pendiente` (o
    `sin leer` si `01011` falló).
  - Si no hay códigos para la derecha: `Ningun codigo guardado ni
    pendiente` (luz encendida) o `Reintento en N s` (error).
  - Las descripciones tienen ancho fijo y `LV_LABEL_LONG_MODE_CLIP`, por si
    algo no entra. `DOTS` no sirve: LVGL escribe los puntos en el texto de
    la etiqueta, y `ponerTexto()` la cambiaría en cada dibujo.
- **`main.cpp`:**
  - `leerFallas()` portada del anterior: `01011`, `03`, `07` y `0A`, todos
    ya en la lista blanca (no se tocó). Se lee al entrar en la página, al
    entrar en `LEYENDO` con la página a la vista y después cada 30 s
    mientras se ve; si `03` o `07` fallan, a los 5 s. En esa vuelta no hay
    consulta del sondeo, y no cuenta en las consultas por segundo.
  - Al entrar en la página se borra la lectura anterior (se ve
    `Leyendo...`); las relecturas cada 30 s no la borran.
  - La página se puede cambiar durante la lectura (~4 consultas): el toque
    se atiende mientras se espera al ELM, como en E4.
  - Un aviso del ELM vuelve a `INICIANDO_ELM`; una desconexión, a
    `BT_PERDIDO`.
  - Monitor serie, como el anterior: `Fallas: luz apagada, 0 guardadas
    segun 0101`, una línea por servicio con la lista completa y, al final,
    `Fallas: sin fallas` (solo si la página lo dice) o `Fallas: error al
    leer, reintento en 5 s`.
- **No existe ninguna función para borrar fallas.** `04` sigue rechazado.

Verificado por el agente:

- `pio run` compila (RAM 10,0 %, flash 50,4 % de 3 MB). Con `-Wall -Wextra
  -Wshadow`, sin warnings nuevos (los de los constructores ya estaban).
- `pio test -e native` 57/57: salen 2 tests de la pantalla de la Nokia y
  entran 5 (vistas, renglones, grupos, cantidades y `errorAlLeer()`), con
  las respuestas reales del DS3 y armadas. Se comprobó que fallan si se
  saca la condición de la luz encendida o el control de repetidos.
- **Render** (`make -C herramientas/render -j8`): los casos `c` a `m` de
  `salida/` son de Fallas: leyendo, sin fallas (respuestas del DS3), sin
  fallas con la luz sin leer, luz encendida sin códigos, solo pendientes,
  repetido, error en `07`, error en todo, con permanentes y 6 códigos en
  dos grupos. `5-fallas` es la de la maqueta. Revisa que dibujar dos veces
  lo mismo no redibuje nada, que ningún texto pase del borde derecho
  (x 633–639) ni de la columna izquierda (x 209–219), que el grupo cambie a
  los 3 s y no antes, y que en Fallas el toque siga cambiando de página.
  Viendo los PNG se encontró y arregló un búfer corto que cortaba
  `Ningun codigo guardado ni pendiente`.
- Los objetos de las páginas piden ~31,8 KB de `malloc` en la PC (en E4,
  ~27 KB): Fallas suma ~4,8 KB, con punteros de 64 bits.

**Prueba en el auto (2026-10-26, motor en ralentí y tibio):** el usuario
subió el firmware y tocó la pantalla; el agente siguió el monitor serie
desde el USB (con su permiso). El usuario confirmó la pantalla y BOOT
("funcionó todo bien"). Log, con la hora de cada línea:
`docs/descubrimiento/2026-10-26-fallas.log`.

- **`FALLAS` da `Sin fallas`, como en E3:** luz apagada, 0 guardadas, 0
  pendientes y `0A` no disponible, en las 8 lecturas. Cada comando tarda
  102–165 ms; la lectura entera, ~0,6 s.
- **Relee cada 30 s** mientras se ve la página (10:21:08, 10:21:38,
  10:22:09...). En 87 s en `INFO`, ninguna lectura.
- **Al apagar el motor con `FALLAS` a la vista**, `SIN ECU` a los 4,6 s de
  la última respuesta buena (en Motor, ~3 s), sin códigos a la vista.
- Además, de E4: las 6 páginas para los dos lados, y BOOT. Consultas por
  segundo y RAM en **Datos descubiertos**.
- En ~6 minutos, el adaptador devolvió 3 respuestas `INVALIDA` que el
  parser rechazó bien: una de varios frames mezclados
  (`"008\r0:410C0C66055B\r008\r1:410C\r2:0D00AAAAAAAAAA\r\r"`) y dos con un
  solo frame de los anunciados (`"00C\r0:41030200347F\r\r"`).

**Cambio después de la prueba (pedido del usuario): en Fallas e Info el
sondeo pide los datos de Motor.** Esas páginas no tienen datos a la vista,
así que el sondeo pedía cada dato a los 2,5 s, casi siempre de a un PID y
con pausas. En `INFO`, con el motor andando, hubo 4 `NO DATA` seguidos con
1,65 s sin respuesta: con uno más y 3 s de silencio habría pasado a un
`SIN ECU` falso, porque con las consultas espaciadas la regla del silencio
protege menos. Ahora `actualizarVisibles()` usa los de Motor
(`Paginas::primeras()`) cuando la página no tiene ninguno: la ECU se
consulta como en Motor, de a varios PIDs y sin pausas, y al volver a Motor
las RPM ya están al día. La lectura de fallas sigue teniendo prioridad
cuando le toca.

**Probado en el auto con el cambio** (2026-10-26, contacto puesto y motor
apagado; el agente subió el firmware con permiso del usuario). Log:
`docs/descubrimiento/2026-10-26-fallas-sondeo.log`.

- En `FALLAS` e `INFO`, 5–7 consultas por segundo y RPM 5–6 por segundo
  (antes, 1–5 y 0), como en Motor.
- `FALLAS` lee al entrar (~0,6 s, `Sin fallas`) y relee a los 30 s
  justos, también con el sondeo andando.
- Hubo otra racha de `NO DATA` en `INFO` (10:52:15–19), ahora mezclada con
  respuestas buenas: como mucho 4 seguidos y nunca más de 0,6 s sin
  respuesta.
- Con el motor andando y `INFO` a la vista, el usuario sacó la llave: el
  adaptador dejó de contestar (dos consultas vacías por timeout), a los
  3 s se cortó el BLE (`SIN CONEXION`) y no apareció más en la búsqueda.
  Es lo esperado: el usuario ya había pasado la alimentación del adaptador
  al pin 1, que se corta con la llave (ver **Decisiones tomadas**).

Con el adaptador alimentado por el contacto, sacar la llave lleva a
`SIN CONEXION`; `SIN ECU` queda para el contacto puesto sin respuesta de
la ECU.

**Pruebas del usuario que faltan, por el adaptador en el pin 1** (para E7,
que revisa la alimentación con la placa instalada):

1. Al dar arranque, ¿se corta el BLE? Si el pin 1 se corta durante el
   arranque, el adaptador se reinicia y la placa reconecta sola (5 s de
   espera, hasta 4 s por la dirección guardada y hasta 6 s buscando).
2. Al poner el contacto, cuánto tarda en llegar a `ECU OK`. El adaptador
   arranca junto con la placa: puede que el primer intento por la
   dirección guardada falle y lo encuentre buscando.

Para E6:

- `Paginas::dibujar()` ahora recibe también la lectura de fallas y el
  reintento; `InfoConexion` no cambió.

Para E7:

- El README describe la página Fallas: qué se lee, cada cuánto y qué
  quiere decir cada pantalla.

### E6

Hecho (rama `e6-brillo-y-ajustes`, desde `main` con E5). **El brillo no
se regula: la luz queda siempre al máximo** (decisión del usuario,
2026-10-26), porque en esta placa la retroiluminación no responde a nada
de lo que se probó (ver **El brillo**, más abajo).

Antes de programar, el usuario eligió (2026-10-26) lo que la maqueta no
resolvía:

- **Sin adaptador, una pantalla tranquila:** `SIN ADAPTADOR` en gris en
  vez de `SIN CONEXION` en rojo, y los reintentos sin pasar por las
  pantallas cian. No se puede distinguir la llave sacada de un adaptador
  colgado: los dos se ven así.
- **La dirección en Info, con ELM y protocolo en un renglón**
  (`v2.1, protocolo 6`): sale el nombre del protocolo (`CAN 11b 500k`).
- El brillo en Info (5 niveles con `-` y `+`): se hizo y se sacó.

Lo que quedó:

- **`config.h`:** `PIN_RETROILUMINACION` y nada más de la luz (salen
  `RETROILUMINACION_INVERTIDA`, `BRILLO_INICIAL` y `PASO_BRILLO`). Nuevos
  `PERIODO_RSSI_MS` (2 s) y `MANTENER_OLVIDAR_MS` (3 s).
- **`pantalla_hw`:** la luz es una salida común: en alto (apagada, según el
  esquemático) mientras se manda el primer cuadro negro y después en bajo
  (al máximo). Salen `ponerBrillo()` y `brillo()`. Nuevo `huecoMaximo()`:
  lo más que pasó entre dos `atender()` (el touch se lee solo ahí).
- **`elm327`:** `direccion()` (la del adaptador conectado, en mayúsculas)
  y `rssi()`, que es `BLEClient::getRssi()`: con NimBLE,
  `ble_gap_conn_rssi()`. Lo da la radio de la placa: no le escribe nada al
  ELM. 0 sin conexión o si falla.
- **`pantalla`:** `sinConexion(motivo, segundos)` pasa a
  `sinAdaptador(segundos)`: `SIN ADAPTADOR` en gris (`ETIQUETA`),
  `Se prende con el contacto` y abajo `Reintento en N s` o, con 0,
  `Buscando...`. El motivo queda solo en el monitor serie.
- **`paginas`:**
  - `InfoConexion` pasa a `DatosInfo`, con `direccion` y `rssi` (0 sin
    medir). Nuevos `enum class Accion { NINGUNA, RECONECTAR, OLVIDAR }`,
    `tomarAccion(ahora)` y `muestraInfo()`.
  - Info como en la maqueta: 4 renglones de 36 px (`Adaptador`
    `OBDBLE, -58 dBm`, `Direccion`, `ELM` `v2.1, protocolo 6` y
    `Lectura`) y a la derecha Reconectar (borde cian) y Olvidar adaptador
    (borde rojo, `mantener 3 s`), de 208×56 px.
  - Los botones son objetos con `LV_OBJ_FLAG_CLICKABLE` (sigue
    `LV_USE_BUTTON 0`): se quedan con el toque, que no llega a la raíz, así
    que no cambian la página. Sin `PRESS_LOCK`: si el dedo se va del botón,
    recibe `PRESS_LOST` y al soltar no cuenta. LVGL tampoco le manda el
    `CLICKED` a la raíz, porque el toque no empezó ahí.
  - Reconectar se pide al soltarlo (`CLICKED`), con el fondo cian oscuro
    mientras se aprieta. Olvidar se pide una vez por apretada, a los 3 s
    (lo mide `tomarAccion()`); mientras tanto crece un relleno rojo oscuro.
    Soltarlo antes o correr el dedo lo cancela.
  - Los fondos oscuros se llaman por su color (`ROJO_OSCURO`,
    `AMBAR_OSCURO`, `CIAN_OSCURO`): los usan también los botones.
- **`consola`:** salen `+` y `-` (de E1).
- **`main.cpp`:**
  - `atenderAcciones()`, desde `loop()` y no mientras se espera al ELM:
    Reconectar y Olvidar hacen lo mismo que `!reconectar` y `!olvidar`
    (`volverAConectar()`), solo en `LEYENDO`. Ninguno le escribe al ELM ni
    toca la lista blanca. `!olvidar` en la consola sigue siendo inmediato.
  - `volverAConectar()` muestra `CONECTANDO` o `BUSCANDO ADAPTADOR` antes
    de soltar el adaptador, que puede tardar hasta 2 s; antes la pantalla
    quedaba quieta mientras tanto.
  - RSSI: con Info a la vista, cada 2 s (`medirRssi()`). Al entrar en Info
    se borra el anterior y se mide en la vuelta siguiente.
  - `avisarConexion`: las pantallas cian `CONECTANDO` y
    `BUSCANDO ADAPTADOR` se ven al arrancar y después de Reconectar u
    Olvidar (pantalla o consola). Después de perder la conexión o de un
    intento fallido, los reintentos quedan en `SIN ADAPTADOR` y solo
    cambia el renglón de abajo.
  - `!info` agrega la dirección y el RSSI.
  - La tabla de cada segundo termina en `touch sin leer hasta N ms`
    (`huecoMaximo()`): un toque más corto que eso se puede perder. Es para
    la queja de botones lentos (abajo).

Verificado por el agente:

- `pio run` compila (RAM 10,0 %, flash 50,2 % de 3 MB). Con `-Wall
  -Wextra -Wshadow`, solo los warnings de siempre. `pio test -e native`
  57/57 (`lib/obd` no cambió).
- **Render**, casos nuevos: `0-sin-adaptador`, `0-sin-adaptador-buscando`,
  `6-info` con los botones, `n-info-reconectar-apretado` y
  `o-info-olvidar-a-la-mitad`. Revisa que cada botón pida lo suyo sin
  cambiar de página; Reconectar, una sola vez; Olvidar, nada antes de los
  3 s, una vez a los 3 s, nada más si se sigue apretando y nada si el dedo
  se va del botón; que al soltarlo el relleno desaparezca; que los datos no
  lleguen a los botones; que fuera de los botones (los datos, entre los
  dos botones) Info cambie de página como las demás, y que con
  `SIN ADAPTADOR` tocar no haga nada. Las 54 revisiones dan bien. El
  touch de mentira se lee cada 100 ms; en la placa, cada 33 ms.
- Los objetos de las páginas piden ~33,8 KB de `malloc` en la PC (en E5,
  ~31,8 KB), con punteros de 64 bits.
- En la placa, en el banco y sin el ELM (con permiso del usuario):
  arranca, `CONECTANDO` y `BUSCANDO ADAPTADOR` y después `SIN ADAPTADOR`.
  El firmware final quedó subido.

**El brillo (2026-10-26).** Se hizo con 5 niveles en Info y en
`Preferences`. El usuario lo probó en el auto y por la consola: la luz no
cambiaba nada. El agente lo investigó con la placa por el USB (con
permiso del usuario, que miraba la pantalla):

- `pantalla_hw` usaba `ledcAttach()`/`ledcWrite()` del core, que no avisan
  si fallan. Se pasó al driver LEDC del ESP-IDF, igual que el ejemplo de
  Waveshare (`lcd_bl_pwm_bsp.c`: baja velocidad, timer 3, canal 1, 8 bits,
  50 kHz, RC_FAST), y se leyó el ruteo del pin: GPIO8 con la señal 74 (la
  del LEDC), como salida, 50 kHz. Con duty 0 y 255, la luz igual.
- `0x53` = `0x2C` y `0x51` = 0 al AXS15231B, por QSPI (`ESP_OK`): igual.
- GPIO8 como salida común, sin el LEDC, leyendo la pata: lee 0 y 1, así
  que el pin cambia de verdad. La luz, igual.
- Un firmware mínimo (aparte, en una carpeta temporal) con solo el código
  de la luz del ejemplo de Waveshare, cambiando el duty cada 2 s: la luz
  no cambia nunca.
- **El esquemático** (`ESP32-S3-Touch-LCD-3.49-Schematic.pdf` de la wiki;
  es vectorial: se lee con `pdftotext` y `pdftoppm`): la luz es un AP3032
  (U10), un regulador de LEDs. `LCD_BL` = IO8 entra por R52 10 kΩ y C73
  100 nF a tierra, y por R49 39 kΩ a FB; R48 5,1 kΩ va de FB a `VLED-`,
  con R50 27 Ω a tierra. Haciendo la cuenta, con IO8 en alto la luz se
  apaga y en bajo queda al máximo (de ahí la lógica invertida del
  ejemplo). `BL_EN` (CTRL, encendido) = EXIO1 del TCA9554, con R46 100 kΩ
  a `VSYS`: como el expansor arranca con todo como entrada, queda
  encendido. El resto del TCA9554: EXIO0 `TP_INT`, EXIO2 y EXIO3
  interrupciones de la IMU, EXIO4 `RTC_INT`, EXIO5 `LCD_TE`, EXIO6
  `SYS_EN` y EXIO7 `NS_MODE` (audio). IO8 no va a ningún otro lado.
- **Conclusión:** en esta placa, hoy, el GPIO8 no le llega a la luz, ni
  con el código del fabricante. El usuario vio regular el brillo con el
  programa de fábrica (una barra, `setUpduty(0xff - valor)` en el GPIO8,
  igual que acá) y, según las notas de E1, también con `+` y `-`: algo
  cambió en el hardware después. Si algún día se quiere revisar: con un
  tester, medir con el GPIO8 en alto y en bajo la tensión en R52, en C73 y
  en FB (pata 3) del AP3032. Si en R52 no aparecen los 3,3 V, la pista
  está cortada; si aparecen y FB no cambia, el problema está en R49 o en
  el AP3032.
- El usuario decidió sacar el brillo y dejar la luz siempre al máximo.
  Salieron `brillo.h/.cpp`, el renglón de Info, `+`, `-` y `!brillo N`, y
  los comandos de diagnóstico de la prueba.

**Botones lentos.** El usuario también vio los botones de Info lentos
(en el auto). Sin la ECU no se pudo reproducir. Se cambió: Reconectar y
Olvidar muestran la pantalla siguiente antes de soltar el adaptador (antes,
hasta 2 s sin cambiar nada), y la tabla del monitor mide cuánto se tarda
en leer el touch.

**Pruebas del usuario** (el firmware ya está en la placa):

1. En Info, con el auto:
   - `OBDBLE, -NN dBm` (se actualiza cada 2 s), `81:23:45:67:89:BA`,
     `v2.1, protocolo 6` y `17 PIDs, N consultas/s`.
   - Tocar un botón no cambia de página; tocar los datos o entre los
     botones, sí.
   - Reconectar: enseguida `CONECTANDO` (cian) y vuelve a Info con datos.
   - Olvidar adaptador: soltarlo antes de 3 s, o correr el dedo, no hace
     nada. Mantenerlo 3 s pasa enseguida a `BUSCANDO ADAPTADOR`, lo
     encuentra, guarda la dirección y vuelve a Info (en el monitor:
     `Pantalla: olvidando el adaptador` y `BLE: direccion guardada`).
   - Si sigue lento: cuál botón, si tarda en reaccionar o hay que tocarlo
     más de una vez, y cuánto dice `touch sin leer hasta` en la tabla.
   - Que no se disparen sin querer con el auto en marcha.
2. Con la llave sacada: `SIN ADAPTADOR` en gris y
   `Se prende con el contacto`; abajo alternan `Buscando...` y
   `Reintento en 5 s`, sin las pantallas cian. Al poner el contacto
   vuelve solo.
3. `!info` con el motor andando: la RAM interna libre (en E5, 128 KB).

Para E7:

- El README explica Info (Reconectar y Olvidar adaptador) y
  `SIN ADAPTADOR`, y que el brillo no se regula.
- De noche la pantalla queda al máximo: si molesta, se puede oscurecer la
  imagen (los colores) en vez de la luz.

**Verificado por el usuario en el auto** (2026-10-26, con el firmware de
E7: "ya lo probé todo ok"): Info, Reconectar, Olvidar adaptador y
`SIN ADAPTADOR`.

### E7

Hecho (rama `e7-cierre-e-instalacion`, desde `main` con E6). Antes de
empezar, el usuario decidió (2026-10-26):

- **Montaje** como en el banco: `PANTALLA_GIRADA = false`, sin cambios.
- **La placa en un USB del auto que se corta con la llave, sin batería.**
  No se toca el TCA9554 (`SYS_EN` es su EXIO6).
- **Sin la prueba larga** (la memoria cada minuto durante 30 min): el
  usuario la descartó. Lo que se sabe de la memoria: sin pérdidas con 6
  reconexiones seguidas (E3) y 128 KB de RAM interna libre con el motor
  andando (E5).

Lo que se hizo:

- **Warnings:** con `-Wall -Wextra -Wshadow` quedaban 5, todos de un
  parámetro con el nombre de un miembro: los constructores de `Consola`,
  `Paginas` y `Pantalla`, y `Consola::ejecutar()`. Se renombraron los
  parámetros. No quedan warnings propios, ni en `src/` ni en `lib/obd`
  (también en los tests nativos). Los de LVGL y del driver del panel no
  son propios.
- **README final:** qué muestra cada página, cómo se ve cada dato, el uso
  (páginas, pantallas de estado, Fallas, Info y consola), el hardware, la
  instalación (alimentación de la placa y del adaptador, montaje), el
  desarrollo, los tests y el render. Incluye lo que pedían las notas de E5
  y E6.
- **Detector de brownout:** sigue activo, como desde E3. En esta placa no
  hubo ningún reinicio por brownout (siempre `encendido` o `USB`). Se
  decide con la prueba en el auto (abajo): si aparece `ALIMENTACION`,
  primero el capacitor del anterior (470–1000 µF entre 5 V y GND), y
  desactivar el detector, como hizo el anterior, solo si sigue pasando.

Verificado por el agente: `pio run` compila (RAM 10,0 %, flash 50,2 % de
3 MB) sin warnings propios, también con `-Wall -Wextra -Wshadow`.
`pio test -e native` 57/57. Render: 54 revisiones, todas bien.

Sin probar: la placa alimentada desde el USB del auto. Hasta ahora siempre
se probó con la notebook.

**Pruebas del usuario, en el auto y sin la notebook** (con la notebook, la
placa se alimenta de ella y no se ve lo del USB del auto):

1. Poner el contacto: la placa y el adaptador arrancan juntos. ¿Cuánto
   tarda en aparecer MOTOR con datos? Puede que el primer intento por la
   dirección guardada falle porque el adaptador todavía no se anuncia, y
   lo encuentre buscando.
2. Dar arranque y mirar la pantalla:
   - Sigue en MOTOR con datos: no se cortó nada.
   - Pasa a `SIN ADAPTADOR` y vuelve sola: se cortó el adaptador (el pin 1),
     no la placa. Anotar cuánto tarda en volver.
   - Se apaga y vuelve con `DS3 OBDII`: el USB se cortó durante el
     arranque (muchos autos cortan los accesorios mientras se da arranque).
     Vuelve sola; anotar cuánto tarda en llegar a MOTOR.
   - Aparece `ALIMENTACION` (rojo): reinicio por brownout. Hace falta el
     capacitor.
3. Con el motor andando un rato, que no se reinicie ni se cuelgue (las RPM
   siguen a la aguja).
4. Lo que quedó de E6: los botones de Info (que respondan bien y no se
   disparen sin querer) y el RSSI. Al sacar la llave se apaga todo, así que
   `SIN ADAPTADOR` casi no se va a ver.
5. Ya montada: que se lea bien de día (lejos del sol directo) y que de
   noche no encandile (la luz queda al máximo; si molesta, se puede
   oscurecer la imagen).

**Verificado por el usuario en el auto** (2026-10-26, con la placa en el
USB del auto: "ya lo probé todo ok"), salvo una cosa: "a veces como que se
cuelga entre que lo prendo y apago el auto, no se conecta". Eso pasó a E8,
que después se descartó. No quedaron anotados cuánto tarda en llegar a los
datos al poner el contacto ni qué se corta al dar arranque. El usuario no
mencionó `ALIMENTACION`: el detector de brownout queda activo, y el
capacitor solo si aparece.

El usuario pidió además (2026-10-26) una pantalla de espera con un DS3 en
vez de las de estado, y pasar a etapas las ideas que había: quedaron
E8–E14. Se sacaron del plan las pruebas con el iPhone (no tiene).

### E8

Descartada antes de programar (2026-10-26). Lo que se había visto, por si
el cuelgue vuelve a molestar (no está confirmado con datos del auto):

- Posibles causas que tenía el plan: el intento por la dirección guardada
  antes de que el adaptador se anuncie (hasta ~15 s por vuelta entre
  dirección, búsqueda y espera); la posición de accesorios con el USB
  prendido y el pin 1 apagado; un corte del USB o del pin 1 al dar
  arranque; un corte de luz mientras se escribe la dirección en
  `Preferences`; un cuelgue de verdad del BLE o de LVGL.
- **Esperas largas en la biblioteca `BLE` del core (NimBLE).** Si el
  adaptador conecta pero no contesta, `BLEClient::connect()` espera el
  intercambio de MTU hasta 31 s (`m_connectTimeout` + 1 s): el
  temporizador de `TIMEOUT_CONEXION_MS` ya no sirve, porque
  `ble_gap_conn_cancel()` solo cancela una conexión que no se estableció.
  `getServices()` y `subscribe()` esperan sin límite propio (cortan con el
  timeout de ATT, 30 s). Mientras tanto `loop()` no corre: la pantalla
  queda quieta y el touch no responde.
- **El watchdog de tareas del core no vigila `loop()`:** está prendido
  (5 s, con pánico) pero solo con la tarea idle del núcleo 0. Hoy un
  cuelgue de `loop()` no reinicia la placa. Se agrega con
  `enableLoopWDT()` (y `esp_task_wdt_reconfigure()` para otro tiempo).
- El core guarda un volcado de memoria en la partición `coredump` en cada
  pánico (`CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH`); se lee con
  `esp_core_dump_get_summary()`.

### E9

Hecho (rama `e9-pantalla-de-espera`, desde `main` con el plan sin E8). Antes
de programar, el usuario decidió (2026-10-26):

- **Las imágenes:** trajo dos animaciones hechas aparte, en cuadros PNG de
  640×172. Un DS3 rojo con techo negro que entra desde la izquierda (40
  cuadros de 40 ms, una vez, y queda en el último): va en la espera, **a los
  5 s de prender la pantalla**. Y el mismo auto andando, con nubes y el
  camino (60 cuadros de 50 ms, en bucle): **una página más, al final**, "una
  chuchería de espera".
- **Cuando el auto termina de entrar, queda quieto** y abajo dice
  `Toca para reintentar`.
- **Tocar reintenta ya; mantener 3 s reinicia la placa.**
- **Reemplaza** `CONECTANDO`, `BUSCANDO ADAPTADOR`, `SIN ADAPTADOR`,
  `INICIANDO ELM`, `BUSCANDO ECU` y `SIN ECU`; quedan `ALIMENTACION` y
  `DESCUBRIENDO`. Con el auto a la vista, la información es solo el renglón
  de abajo. Si se pierde la ECU con el motor andando, el mismo auto con
  `Sin respuesta de la ECU` en ámbar.

Lo que se hizo:

- **Las imágenes** (`herramientas/imagenes/`). Los cuadros enteros no entran
  en la flash (100 × 220 KB). `convertir.py` (solo Python 3, con un lector de
  PNG propio) los separa en partes y escribe `src/imagenes/entrada.c` y
  `paseo.c`; `imagenes.h` declara lo que hay. En `fuentes/` quedan el auto,
  los `animacion.json`, los `LEEME.txt` y 4 cuadros, no los 100.
  - Entrada: el auto (`auto.png`) mezclado con el negro y sin alfa (RGB565,
    334×139), la tabla de x y giro de cada cuadro y las dos llantas quietas.
  - Paseo: las nubes (una franja de 640×50 que se corre todo el ancho en
    cada vuelta; lo que el techo tapa en el cuadro 0 sale del cuadro 30,
    corrido medio ancho), el fondo quieto (cielo, auto y camino, 640×109),
    el techo que asoma entre las nubes (con alfa) y las llantas. Desde y 159,
    el camino y las rayas los dibuja LVGL.
  - En total ~315 KB: la flash pasa de 50,2 % a 61,1 % de 3 MB.
  - **Las llantas no giran en círculo.** Cada una es una elipse de ~21×23 px,
    y en los cuadros se la estira a un círculo, se la gira y se la vuelve a
    achicar: con ese modelo el error contra los cuadros es ~7 de 255
    (girando en círculo, 9–35). LVGL escala y gira en el otro orden, así que
    `llanta.cpp` las gira en un búfer RGB565A8 (bilineal, con el borde
    suavizado), solo cuando cambia el ángulo. Las elipses salen de comparar
    el auto quieto con un cuadro donde giró.
  - Medido en los cuadros del paseo: las llantas giran 24° por cuadro
    (480°/s; el cuadro 15 es igual al 0), las nubes y las rayas se corren
    640 px por vuelta (3 s) y las rayas son de 35×3 px cada 80.
  - Los colores del camino salen redondeados como los de las imágenes: LVGL
    trunca al pasar a RGB565, y sin eso se veía una costura en y 159.
- **`llanta.h/.cpp`** (sin LVGL ni Arduino): `girarLlanta(fuente, grados,
  salida)`.
- **`animaciones.h/.cpp`:** `Entrada` (`crear(padre, aparece, y)`,
  `mover(ahora, visible)`, `quieto()`) y `Paseo` (`crear()`, `mover()`). Los
  mueve un temporizador de LVGL de quien los usa. Las posiciones salen de la
  hora, no de contar cuadros: si LVGL se atrasa, saltea cuadros pero no se
  frena. Los búferes de las llantas (4 de ~6,5 KB) van a la PSRAM.
- **`pantalla`:** salen `inicio()`, `conectando()`, `sinAdaptador()`,
  `iniciandoElm()`, `elmNoResponde()`, `buscandoEcu()`, `sinEcu()` y
  `LineaDato`. Entran:

  ```cpp
  void espera(const char *estado, bool problema = false);
  bool tomarReintento();                      // un toque desde la última llamada
  void alMantenerApretada(void (*funcion)()); // a los MANTENER_REINICIO_MS
  void paginaCompleta();                      // DS3: 640x172, sin la barra
  lv_obj_t *areaCompleta() const;
  ```

  - La espera ocupa toda la pantalla, sin la barra: el auto 8 px más arriba
    que en los cuadros (para que entre el renglón), el estado abajo a la
    izquierda (gris, o ámbar si es un problema) y a la derecha
    `Toca para reintentar`, con el auto quieto.
  - Toda la espera recibe el toque. Al soltar, es un reintento. Apretada más
    de 0,5 s, abajo crece una barra roja y dice `Mantener para reiniciar`; a
    los 3 s llama una vez a la función de `alMantenerApretada()`. Lo mide un
    temporizador de LVGL (cada 40 ms), que también mueve el auto.
  - El auto entra a los `ESPERA_ENTRADA_MS` (5 s) de `Pantalla::iniciar()`,
    si la espera está a la vista; si no, después aparece quieto.
- **`paginas`:** la página `DS3`, al final (son 7), en `areaCompleta()`. El
  toque en la raíz cuenta también con esa área a la vista. No tiene datos:
  el sondeo pide los de Motor, como en Fallas e Info.
- **`pantalla_hw`:** `tomar()` y `soltar()` (un mutex recursivo) y una tarea
  `pantalla` (núcleo 1, prioridad 1, 8 KB de pila) que atiende LVGL solo
  cuando `loop()` lo soltó. `atender()`, `dibujarYa()` y `medirRefresco()`
  lo toman. **Cambia la decisión de concurrencia:** hasta acá LVGL era solo
  de `loop()`, y mientras conectaba (`connect()`, hasta 4 s; hasta 30 s si el
  adaptador conecta y no contesta, ver notas de E8) la pantalla quedaba
  quieta. La entrada del auto cae justo en esos segundos, y mantener para
  reiniciar es para cuando la placa se traba: la pantalla tenía que seguir.
  El BLE corre en el núcleo 0.
- **`main.cpp`:**
  - `conectarBt()` suelta LVGL alrededor de `elm.conectar()`, y
    `volverAConectar()`, alrededor de desconectar u olvidar.
    `alCambiarFase()` y `alEsperar()`, que corren adentro, lo toman.
  - `dibujar()` y `alCambiarFase()` ponen la espera con su texto.
    `ecuPerdida`: al pasar de `LEYENDO` a `SIN_ECU` con las últimas RPM de
    más de 0; se borra al encontrar la ECU o al perder el adaptador.
  - `atenderReintento()`: en `BT_PERDIDO`, `SIN_ECU` o esperando para volver
    a iniciar el ELM, adelanta el próximo intento (en el monitor,
    `Pantalla: reintento`). En `BT_PERDIDO` vuelve a mostrar las fases, como
    Reconectar.
  - `reiniciarPlaca()`: `Pantalla: reiniciando la placa` y `esp_restart()`.
- **`config.h`:** `ESPERA_ENTRADA_MS` (5 s) y `MANTENER_REINICIO_MS` (3 s).
  **`lv_conf.h`:** `LV_USE_IMAGE 1`.
- Sin cambios en `lib/obd`, en la lista blanca ni en `elm327`: tocar la
  pantalla no le manda nada al auto.

Verificado por el agente:

- `pio run` compila (RAM 10,1 %, flash 61,1 % de 3 MB), sin warnings
  propios también con `-Wall -Wextra -Wshadow`. `pio test -e native` 57/57.
- **Render**, 70 revisiones, todas bien. Casos nuevos: `0-espera-*` (antes
  del auto, entrando en los cuadros 6, 13, 20 y 30, quieto, los estados y
  mantener apretado), `7-ds3` y `q-ds3-*` (cuadros 0, 5 y 30). Revisa que
  tocar la espera reintente una vez sin cambiar de página; que soltarla
  antes de 3 s reintente sin reiniciar y la barra desaparezca; que a los 3 s
  reinicie una sola vez y al soltar no reintente; que con el auto quieto no
  se redibuje nada; que si a los 5 s no se veía, aparezca quieto; que DS3 se
  mueva, vuelva a empezar a los 3 s y oculta no se mueva, y que tocar a los
  costados cambie de página. Los objetos piden ~51,8 KB de `malloc` en la PC
  (en E6, ~33,8 KB); ~27 KB son los búferes de las llantas, que en la placa
  van a la PSRAM.
- **Contra los cuadros originales** (pasados a RGB565), los PNG del render
  difieren en promedio de 0 a 1,2 sobre 255. Lo que cambia es el borde de los
  rayos de las llantas (otra interpolación), el borde del techo cuando pasa
  una nube por atrás y, en el cuadro 5, las nubes y las rayas (en el
  original caen en fracciones de píxel).

Sin probar (falta la placa): todo en la pantalla real, la tarea nueva de
`PantallaHw`, cuántos cuadros por segundo salen, las consultas por segundo
con DS3 a la vista (redibuja ~20 veces por segundo, y cada redibujo cuesta
~33 ms de envío) y la RAM interna libre (la tarea suma 8 KB).

**Pruebas del usuario** (subir el firmware de esta rama):

1. Al prender: negro con `Iniciando...` y `Conectando con el adaptador...`
   abajo. Si a los 5 s no hay datos, entra el DS3 sin trabarse y queda con
   `Toca para reintentar`. Si llega a MOTOR antes, el auto no aparece.
2. Con el adaptador apagado (en el banco, o la llave en accesorios si ahí el
   pin 1 no tiene tensión): `Esperando el adaptador`. Tocar: en el monitor
   `Pantalla: reintento`, y abajo `Conectando con el adaptador...`.
3. Mantener apretada la espera: a los 0,5 s la barra roja y
   `Mantener para reiniciar`; soltar antes de 3 s reintenta; a los 3 s se
   reinicia (vuelve a `Iniciando...`). Probarlo también mientras dice
   `Conectando con el adaptador...`.
4. Con datos: la página DS3 después de INFO, fluida, y tocar a los costados
   cambia de página. En el monitor, las consultas por segundo con DS3 a la
   vista (en Motor, 5–7).
5. `!info` con el motor andando: la RAM interna libre (en E5, 128 KB).

**Verificado por el usuario** (2026-10-26: "listo probado"), con el firmware
de esta rama. No detalló cada punto: no quedaron anotadas las consultas por
segundo con DS3 a la vista ni la RAM interna libre.

**Cambio posterior del usuario** (2026-10-26, en la rama de E10, en un
commit aparte): `ESPERA_ENTRADA_MS` pasa de 5 s a 0, así que el DS3 entra
apenas se prende la pantalla, sin esperar. El render sigue dando bien.

Para E10:

- Ya hay 7 páginas: con VIAJE y FUERZA G son 9, y hay que subir
  `MAX_PAGINAS` (8). DS3 tiene que seguir última.

### E10

Rama `e10-viaje`, desde `main` con E9 y el plan del RTC. Decidido por el
usuario antes de empezar (2026-10-26):

- **E10 no guarda el viaje:** vive en la RAM, empieza al poner el contacto
  (la placa arranca con la llave) y se pierde al sacarla. Guardar cada
  viaje y su resumen es de E13, en la microSD.

**Maqueta** (agente, 2026-10-26), en el lienzo de E4
(https://claude.ai/artifact/41eQde6wkKUL81ZuDnHBGX), fila "Viaje (E10)": A
(recorrido a la izquierda y máximos a la derecha), B (ocho casillas) y C (la
distancia en grande, como las RPM de Motor), y la A con el contacto recién
puesto y mantenida apretada. Sin tildes, como en la placa.

Elegido por el usuario (2026-10-26), lo que el plan no resolvía:

- **El diseño B:** ocho casillas de 160×72, en dos filas: distancia, tiempo,
  promedio y velocidad máxima; RPM máxima, refrigerante máximo y voltaje
  mínimo y máximo.
- **Tiempo con el motor en marcha**, y el promedio es la distancia sobre ese
  tiempo (con los semáforos).
- **En marcha desde 500 rpm** (ralentí 890–1190). El voltaje mínimo y máximo
  cuentan recién a los 5 s de arrancar, sin la caída del arranque.
- **Reiniciar:** al poner el contacto y, además, manteniendo apretada la
  página 3 s (la barra de arriba se llena de rojo). Tocar sigue cambiando de
  página.
- Lo que propuso el agente y quedó: VIAJE entre INFO y DS3; `MAX_PAGINAS` a
  9; la velocidad en todas las consultas y la RPM en las que entra (en
  Mezcla no: 5 datos y la velocidad ya son 6); un hueco de más de
  `VIGENCIA_MS` no suma distancia ni tiempo; `...` hasta la primera lectura y
  `N/D` si la ECU no da el dato; el tiempo como `38 min` y desde la hora
  `1:07 h`.

Lo que se hizo:

- **`lib/obd/viaje`** (nuevo, sin Arduino):

  ```cpp
  struct ReglasViaje { uint32_t huecoMaximoMs; float rpmEnMarcha; uint32_t esperaVoltajeMs; };
  struct Extremo { bool hay = false; float valor = 0; };
  class Viaje {
  public:
    explicit Viaje(const ReglasViaje &definidas);
    void reiniciar();
    void velocidad(float kmh, uint32_t ahora);
    void rpm(float valor, uint32_t ahora);
    void refrigerante(float grados);
    void voltaje(float volts, uint32_t ahora);
    double distanciaKm() const;
    uint32_t tiempoEnMarchaMs() const;
    bool promedio(float *kmh) const; // false sin tiempo en marcha
    const Extremo &velocidadMaxima() const; // y rpmMaxima(), refrigeranteMaximo(),
                                            // voltajeMinimo(), voltajeMaximo()
  };
  void textoDistancia(double km, char *salida, size_t tam);           // "23.7", "123"
  const char *textoTiempo(uint32_t ms, char *salida, size_t tam);     // "38" min, "1:07" h
  ```

  - La distancia integra la velocidad con trapecios entre dos lecturas
    seguidas, en metros y en `double` (con `float`, a cientos de km, los
    pasos de unos pocos metros perderían precisión). Un hueco de más de
    `huecoMaximoMs` no suma.
  - El tiempo en marcha suma el lapso entre dos lecturas de RPM seguidas, las
    dos en marcha (`rpmEnMarcha` o más) y sin un hueco en el medio.
  - El voltaje cuenta con la última RPM en marcha, de hace menos de
    `huecoMaximoMs`, y en marcha desde hace `esperaVoltajeMs`. Después de un
    hueco, o al volver a arrancar, la espera empieza de nuevo.
  - Los máximos toman cualquier lectura, también 0: con el contacto y el motor
    parado se ve `0 km/h` y RPM `0`. `velocidadMaxima().hay` dice si ya hubo
    alguna velocidad, y `rpmMaxima().hay`, alguna RPM.
  - El promedio es la distancia sobre el tiempo en marcha.
- **`config.h`:** `RPM_EN_MARCHA` (500), `ESPERA_VOLTAJE_VIAJE_MS` (5 s),
  `MANTENER_VIAJE_MS` (3 s) y `AVISO_MANTENER_MS` (0,5 s), que pasó de
  `pantalla.cpp` porque ahora lo usan la espera y Viaje. El hueco máximo es
  `VIGENCIA_MS`.
- **`dibujo.h`:** `ROJO_OSCURO` y `ROJO_CLARO` pasaron de `paginas.cpp`, para
  la barra.
- **`pantalla`:** `MAX_PAGINAS` pasa de 8 a 9 (las 8 de hoy y FUERZA G).
  `pagina(titulo, numero, total, mantenida)`: con `mantenida` de 0 a 1, la
  barra se llena de `ROJO_OSCURO` hasta ahí (un rectángulo debajo del título
  y los puntos) y el título va en `ROJO_CLARO`.
- **`paginas`:**
  - `VIAJE` entre `INFO` y `DS3`. No tiene datos a la vista: el sondeo pide
    los de Motor, como en Fallas, Info y DS3.
  - Ocho casillas de 160×72 con separadores de 1 px: el nombre en 20 px (base
    a 25 px del borde de la casilla) y el valor en Montserrat 36 (base a 63),
    con la unidad detrás en gris. El tiempo cambia de unidad (`min` o `h`):
    `ponerUnidad()`. Lo que no tiene valor va en gris, sin unidad.
  - `dibujar()` recibe también el `Viaje`. `Accion::REINICIAR_VIAJE`.
  - Mantener apretado: la raíz recibe `PRESSED`, `RELEASED` y `PRESS_LOST`
    (`alApretar()`), en cualquier página. `tomarAccion()` pide reiniciar una
    vez por apretada, a los `MANTENER_VIAJE_MS`, solo con Viaje a la vista.
    Desde `AVISO_MANTENER_MS` la barra se llena y dice `Mantener para
    reiniciar el viaje`; una vez pedido, `Viaje reiniciado`. Como en la
    espera, antes de 0,5 s es un toque.
  - Al soltar, LVGL manda `RELEASED` y después el `CLICKED` de `alTocar()`,
    que anota si la apretada duró `AVISO_MANTENER_MS` o más. `tomarToque()`
    descarta ese toque solo en Viaje: en las demás páginas, una apretada larga
    sigue cambiando de página, como antes.
- **`main.cpp`:**
  - `Viaje viaje` global, con `VIGENCIA_MS`, `RPM_EN_MARCHA` y
    `ESPERA_VOLTAJE_VIAJE_MS`. `guardar()` le pasa cada RPM, velocidad y
    refrigerante (de los pedidos de varios PIDs y de a uno), y
    `leerVoltaje()` cada `ATRV` bueno, también en `SIN_ECU` (ahí no cuenta:
    no hay RPM reciente).
  - **La velocidad en todas las consultas** (tarea 2): `actualizarVisibles()`
    agrega `VELOCIDAD` y después `RPM` a los visibles de la página, si no
    están y si entran en `MAX_VISIBLES`. Quedan: Motor, Fallas, Info, Viaje y
    DS3 `RPM`, `REFRIGERANTE`, `VELOCIDAD`; Aire/Volt `ADMISION`, `VOLT_ELM`,
    `VELOCIDAD`, `RPM`; Carga los 3, `VELOCIDAD` y `RPM`; Mezcla los 5 y
    `VELOCIDAD` (6: la RPM queda cada 2,5 s). No cambia cuántas consultas hay
    por segundo: cada una tarda lo mismo pida 1 PID o 6 (E3). Con los pedidos
    de a uno (si la ECU no aceptara varios), los visibles se turnan.
  - `atenderAcciones()`: `REINICIAR_VIAJE` (solo en `LEYENDO`) escribe
    `Pantalla: viaje reiniciado (llevaba X km en N s)` en el monitor y
    reinicia el viaje. No usa el ELM.
  - La tabla de cada segundo termina en `viaje X km en N s` (el tiempo en
    marcha), para comparar la distancia con el parcial del tablero.
- Sin cambios en la lista blanca, en `elm327` ni en lo que se le manda al
  auto: el viaje sale de lo que ya se leía.

Verificado por el agente:

- `pio run` compila (RAM 10,2 %, flash 61,3 % de 3 MB; en E9, 10,1 % y
  61,1 %). Con `-Wall -Wextra -Wshadow`, sin warnings propios.
- `pio test -e native` 67/67: 10 nuevos en `test/test_viaje/` (distancia a
  velocidad fija y en rampa, huecos, tiempo en marcha con el motor parado y
  arrancando, promedio, voltaje con la espera, sin RPM reciente y después de
  un hueco, máximos, reiniciar, `millis()` que da la vuelta y los textos). Se
  comprobó que fallan si se saca el control del hueco o la espera del
  voltaje.
- **Render**, 86 revisiones, todas bien. Casos nuevos: `7-viaje` (el de la
  maqueta: 23.7 km en 38 min), `r-viaje-arranque`, `s-viaje-extremos`
  (1235 km, 12:34 h, 188 km/h, 6500 rpm, 112 °C, 11.9 y 15.2 V),
  `t-viaje-sin-velocidad` (`N/D`), `u-viaje-mantener` (1,5 s) y
  `v-viaje-reiniciado`. Revisa que ningún texto pase de su casilla, que
  dibujar dos veces no redibuje nada, que un toque cambie de página, que
  apretada menos de 0,5 s no aparezca la barra, que soltarla antes de 3 s no
  reinicie ni cambie de página y la barra desaparezca, que a los 3 s pida
  reiniciar una sola vez, que al soltarla después no cambie de página, y que
  en Motor mantener apretado no llene la barra y al soltar siga cambiando de
  página. Se comprobó que las de soltar fallan sin la regla de
  `tomarToque()`.
- Los objetos de las páginas piden ~60 KB de `malloc` en la PC (en E9,
  ~51,8 KB): VIAJE suma ~8 KB, con punteros de 64 bits. En la placa salen de
  la RAM interna: falta medirlo con `!info`.

No hay **Datos descubiertos** nuevos: nada se probó todavía en la placa.

Sin probar (falta la placa): todo en la pantalla real, mantener apretado con
el touch de verdad, la distancia contra el tablero, las consultas por
segundo con la velocidad y la RPM agregadas (en cada consulta, Aire/Volt pide
ahora 3 PIDs y Carga 5) y la RAM interna libre.

**Pruebas del usuario** (subir el firmware de esta rama):

1. Con el contacto puesto y el motor parado, VIAJE (después de INFO, el
   séptimo de 8 puntos): `0.0 km`, `0 min`, Promedio `...`, `0 km/h`, RPM
   `0`, el refrigerante, y los dos voltajes `...`.
2. Al arrancar, el tiempo empieza a contar, y a los ~5 s aparecen los
   voltajes (en marcha, 13,6–14,1 V según E3).
3. En un viaje: anotar el parcial del tablero al salir y al llegar, y
   compararlo con la distancia. La velocidad máxima puede quedar unos km/h
   por debajo de lo que marcó la aguja: los velocímetros suelen marcar de
   más, y la ECU da la del sensor.
4. Mantener apretada VIAJE: a los 0,5 s se llena la barra de rojo; soltarla
   antes de 3 s no hace nada; a los 3 s dice `Viaje reiniciado` y en el
   monitor `Pantalla: viaje reiniciado (llevaba ...)`. Un toque corto cambia
   de página, y en las otras páginas todo sigue igual.
5. En el monitor, las consultas por segundo en cada página (5–7 en E5),
   sobre todo Aire/Volt, Carga y Mezcla, y al final de la tabla
   `viaje X km en N s`.
6. Sacar la llave y volver a ponerla: el viaje empieza de cero.
7. `!info` con el motor andando: la RAM interna libre (en E5, 128 KB).

Para E11:

- `Viaje` ya sabe si el motor está en marcha (desde 500 rpm). La alarma de
  voltaje de carga puede usar la misma regla, pero hoy no es pública:
  habría que exponer algo como `enMarchaDesde()`.

Para E12:

- VIAJE quedó con las 8 casillas llenas. Los máximos de G que el plan suma a
  VIAJE no entran sin rehacerla: o van en la página FUERZA G, o se decide con
  el usuario qué casilla sale.
- `MAX_PAGINAS` ya es 9. FUERZA G tiene que ir antes de DS3.

Para E13:

- El resumen del viaje está en `Viaje` (distancia, tiempo en marcha y los
  extremos). Lo que sale en la tabla del monitor (`viaje X km en N s`) es lo
  mismo que se podría escribir en el CSV.

### E11

Rama `e11-alarmas`, desde `main` con E10. **Maqueta** (agente, 2026-10-26),
en el lienzo de E4 (https://claude.ai/artifact/41eQde6wkKUL81ZuDnHBGX),
filas "Alarmas": A (la barra roja con la alarma y el valor, y el valor en
rojo en su página) y B (un aviso grande que se acepta tocando), dos alarmas
alternadas, la luz de falla en ámbar y la alarma sobre DS3.

Elegido por el usuario (2026-10-26):

- **Alarmas:** refrigerante alto (105 °C o más, se apaga en 102), voltaje de
  carga con el motor en marcha (menos de 13,0 o más de 15,0 V durante más de
  10 s) y RPM en zona roja (6000 o más). La luz de falla no.
- **El diseño A:** la barra de arriba en rojo con la alarma y el valor, y el
  valor en rojo en su página. Tocar sigue cambiando de página: no hay nada
  que aceptar.
- **Sonido: tres pitidos al aparecer** cada alarma, sin repetir.
- **La placa es la V2** (`Rev1.1`). El agente había dicho que no traía
  parlante, mirando la wiki de la 3.49 común: el usuario lo corrigió (la
  3.49B, con caja, tiene parlante).

**La V2 cambia pines** (descubierto al buscar el audio, 2026-10-26). Según
los ejemplos de Waveshare para la V2
(`waveshareteam/ESP32-S3-Touch-LCD-3.49-V2`, `user_config.h`) y su
esquemático: la luz va por el **GPIO42** (`LCD_BL`), el GPIO8 pasa a ser
`EXIO_INT` (la interrupción del TCA9554), el GPIO21 es `LCD_TE` (una salida
de la pantalla) y el reset de la pantalla es **EXIO5** del TCA9554. El
firmware usa los de la V1 (luz en GPIO8, reset en GPIO21): por eso en E6 el
GPIO8 no cambiaba la luz y la demo de fábrica sí. No se toca en E11: queda
para un arreglo aparte (ver **Datos descubiertos**). El audio es igual en las
dos: ES8311 en `0x18` del I2C de la placa, I2S con MCLK 7, BCLK 15, WS 46,
salida de la placa (`DSDIN` del ES8311) 45 y entrada 6, y el amplificador
NS4150B prendido con EXIO7 (`NS_MODE`) en alto.

Lo que se hizo:

- **`lib/obd/alarmas`** (nuevo, sin Arduino):

  ```cpp
  struct ReglaAlarma { float umbral; float normal; bool porArriba;
                       uint32_t paraPrender; uint32_t paraApagar; };
  enum class CambioAlarma : uint8_t { NINGUNO, SE_PRENDIO, SE_APAGO };
  class Alarma {
    CambioAlarma revisar(bool hay, float valor, uint32_t momento);
    bool activa() const;
    float valor() const;
  };
  enum TipoAlarma : uint8_t { REFRIGERANTE_ALTO, VOLTAJE_BAJO, VOLTAJE_ALTO, RPM_ALTA, CANT_ALARMAS };
  class Alarmas {
    explicit Alarmas(const ReglaAlarma (&reglas)[CANT_ALARMAS]);
    CambioAlarma revisar(TipoAlarma tipo, bool hay, float valor, uint32_t momento);
    const Alarma &alarma(TipoAlarma tipo) const;
    uint8_t activas() const;
    bool aLaVista(uint32_t vuelta, TipoAlarma *tipo, uint8_t *numero) const;
  };
  void textoAlarma(TipoAlarma tipo, float valor, char *salida, size_t tam);
  ```

  - Se prende con el valor en el umbral o más allá y se apaga con el valor
    en `normal` o más acá (la histéresis). El tiempo se cuenta con el
    **momento de cada lectura**, no con el reloj: revisar la misma lectura
    otra vez no cambia nada, un `NO DATA` suelto no la cambia (el valor
    anterior sigue vigente) y un dato suelto tampoco (con `paraPrender` > 0
    hacen falta dos lecturas). Sin un valor actual se apaga, y vuelve a
    contar de cero: nunca queda un valor viejo a la vista.
  - Con varias, `aLaVista()` da una por vuelta, en el orden de `TipoAlarma`.
  - `textoAlarma()`: `REFRIGERANTE 106 °C`, `VOLTAJE BAJO 12.6 V`,
    `VOLTAJE ALTO 15.4 V`, `ZONA ROJA 6250 RPM`.
- **`Viaje::enMarchaDesdeHace(ahora, ms)`** (nuevo, público): la regla del
  voltaje del viaje, que ahora usa también la alarma. Después de reiniciar
  el viaje a mano vuelve a contar desde la RPM siguiente: la alarma de
  voltaje se apaga y espera de nuevo 5 s.
- **`config.h`:** los umbrales y tiempos (`REFRIGERANTE_ALARMA` 105 y
  `_NORMAL` 102 con 5 s; `VOLTAJE_BAJO_ALARMA` 13,0 y `_NORMAL` 13,2,
  `VOLTAJE_ALTO_ALARMA` 15,0 y `_NORMAL` 14,8, con 10 s; `RPM_ALARMA` 6000 y
  `_NORMAL` 5800, al instante), `PERIODO_ALARMAS_MS` (3 s) y los pitidos
  (3 de 150 ms cada 250 ms, a 2500 Hz y volumen 0,5). **`ALARMAS_DE_PRUEBA`**
  (en `false`) cambia los umbrales por unos que saltan manejando normal:
  refrigerante 40 °C, voltaje bajo 14,5 V y alto 13,5 V (los dos con
  13,6–14,1 V de marcha), RPM 2000. Los pines del audio y `DIRECCION_CODEC`.
- **`src/sonido`** (nuevo): `iniciar()`, `iniciado()` y `pitar(veces)`.
  - El I2C de la placa en `I2C_NUM_0` (el del touch es el 1), a 300 kHz como
    los ejemplos, con el ES8311 y el TCA9554.
  - Como el ejemplo: primero el I2S (maestro, 16 kHz, 16 bits estéreo, MCLK
    de 256 veces: 4,096 MHz, una combinación de la tabla del ES8311), que
    queda mandando ceros (`auto_clear_after_cb`); después el ES8311, con la
    secuencia del driver de Espressif (`esp_codec_dev`, Apache-2.0) para
    reproducir, solo el DAC, como esclavo y a 0 dB; y al final el
    amplificador.
  - **Del TCA9554 se toca solo el pin 7**, leyendo y cambiando un bit:
    primero la salida en alto y después como salida. Los demás quedan como
    entradas: el 1 prende la luz (con resistencia a `VSYS`), el 6 sostendría
    la placa con la batería y, en la V2, el 5 es el reset de la pantalla.
  - Los pitidos los genera una tarea (`sonido`, núcleo 0, prioridad 2, 3 KB
    de pila), con fase continua y una rampa de 5 ms al principio y al final
    para que no hagan clic. `loop()` no espera.
  - Si algo no contesta, `Sonido: no se pudo iniciar (...)` en el monitor y
    sigue sin sonido.
- **`pantalla`:** `pagina(..., alarma)`: con una alarma, la barra en `ROJO`
  con `LV_SYMBOL_WARNING` (el triángulo, que trae Montserrat 20) y el texto
  en blanco, en lugar del título; mantener apretada Viaje le gana.
  `paginaCompleta(numero, total, alarma)`: en DS3 la barra se ve solo con
  una alarma; por eso la barra pasó a estar arriba de todo
  (`lv_obj_move_to_index(fondoBarra, -1)`). `ver()` decide si se ve la barra
  de una vez, para no ocultarla y mostrarla en el mismo dibujo.
- **`paginas`:** `AlarmaALaVista {texto, magnitudes}` y `dibujar(...,
  alarma)`. Un valor actual de una magnitud con alarma va en `ROJO`:
  refrigerante y RPM en Motor, voltaje en Aire/Volt.
- **`main.cpp`:**
  - `REGLAS_ALARMAS` y `Alarmas alarmas`, y `Sonido sonido`, que se inicia en
    `setup()` después del botón.
  - `revisarAlarmas()`, en cada vuelta de `loop()`: refrigerante, voltaje
    (con `enMarchaDesdeHace(ahora, ESPERA_VOLTAJE_VIAJE_MS)`) y RPM, con
    `valorActual()` y el momento de cada dato. Fuera de `LEYENDO` se apagan.
    Al prenderse: `Alarma: REFRIGERANTE 106 °C` en el monitor y los pitidos;
    al apagarse, `Alarma: termino (...)`.
  - `alarmaALaVista()` arma el texto (con `  1/2` si hay varias, una cada
    `PERIODO_ALARMAS_MS`) y las magnitudes en rojo.
  - `!pitar` en la consola (nuevo): los pitidos de una alarma, para probar
    el parlante. `!info` dice si el sonido anda, cuántas alarmas hay y si
    los umbrales son de prueba.
- Sin cambios en la lista blanca ni en lo que se le manda al auto: las
  alarmas usan lo que ya se leía. La luz de falla no es alarma (el usuario
  no la eligió).

Verificado por el agente:

- `pio run` compila (RAM 10,7 %, flash 61,9 % de 3 MB; en E10, 10,2 % y
  61,3 %). Con `-Wall -Wextra -Wshadow`, sin warnings propios.
- `pio test -e native` 75/75: 7 nuevos en `test/test_alarmas/` (refrigerante
  con un dato suelto, 5 s seguidos e histéresis; voltaje por abajo; RPM al
  instante; sin valor; `millis()` que da la vuelta; varias alternadas; los
  textos) y uno en `test_viaje` (`enMarchaDesdeHace()`).
- **Render**, 101 revisiones, todas bien. Casos nuevos:
  `w-alarma-refrigerante`, `x-alarma-zona-roja` (las RPM grandes en rojo),
  `y-alarma-dos` (`VOLTAJE ALTO 15.4 V  1/2` en Aire/Volt),
  `z-alarma-ds3` (la barra sobre la animación) y
  `z-alarma-viaje-mantenida`. Revisa que la barra sea roja, que dibujar dos
  veces no redibuje nada (también en DS3), que tocar la barra roja siga
  cambiando de página, que sin alarma Motor quede como antes y DS3 sin la
  barra, y que en Viaje mantener apretado le gane a la alarma.

Sin probar (falta la placa): **todo el audio** (que el ES8311 conteste, que
suene, el volumen, si hace ruido al prender o entre pitidos), la barra en la
pantalla real y las alarmas en el auto.

**Pruebas del usuario** (subir el firmware de esta rama):

1. Al arrancar, en el monitor: `Sonido: listo` (si no, qué falló). La luz
   y la pantalla, igual que antes (el expansor solo cambia el pin 7).
2. `!pitar`: tres pitidos. ¿Se oyen bien en el auto? ¿Hace ruido al
   prender la placa o entre pitidos? El volumen y el tono se cambian en
   `config.h` (`VOLUMEN_PITIDO`, `FRECUENCIA_PITIDO_HZ`).
3. Con `ALARMAS_DE_PRUEBA = true` en `config.h`, en el auto: a ~2000 rpm
   sale `ZONA ROJA` (las RPM en rojo en Motor) y se va al bajar de 1800;
   con el motor tibio, `REFRIGERANTE`; a los 15 s de arrancar, los dos
   voltajes, alternando con `1/2`. Cada una con sus tres pitidos, y en el
   monitor `Alarma: ...` y `Alarma: termino (...)`. Que tocar la barra roja
   cambie de página. Al apagar el motor con el contacto puesto, las de
   voltaje se van.
4. Con `ALARMAS_DE_PRUEBA = false`, en un viaje normal no salta ninguna. Si
   salta la de voltaje, mirar en VIAJE el mínimo y el máximo y ajustar.

**Cambio posterior** (2026-10-27, en la rama `plan-cierre`, a pedido del
usuario y antes de probar los pitidos): `VOLUMEN_PITIDO` de 0,5 a 1 (+6 dB), lo
más que entra sin recortar la onda. **Probado por el usuario** el mismo día:
`!pitar` "pita perfecto" (la prueba 2). Faltan las 3 y 4, en el auto.

Para el arreglo de la V2 (o la etapa que toque la pantalla):

- La luz va por el GPIO42 y el reset por EXIO5 del TCA9554. El firmware hoy
  pone el GPIO21 como salida para el reset; en la V2 es `LCD_TE`, una salida
  de la pantalla. Con TE apagado (no se manda `0x35`) parece no molestar,
  pero conviene dejarlo como entrada. Ver **Ideas para después**.

Para E12 y E14:

- El I2C de la placa ya está iniciado (`Sonido::iniciar()`, `I2C_NUM_0`).
  El IMU y el RTC tienen que usar el mismo bus
  (`i2c_master_get_bus_handle(I2C_NUM_0, ...)`), no crear otro, y no tocar
  los pines del TCA9554 que no son suyos.

### E12

Rama `e12-fuerza-g`, desde `main` con E15. **Maqueta** (agente, 2026-10-26),
en el lienzo de E4 (https://claude.ai/artifact/41eQde6wkKUL81ZuDnHBGX), filas
"Fuerza G (E12)": A (el círculo a la izquierda, los valores de ahora y los
máximos en casillas como VIAJE), B (el círculo al centro, los máximos en lista
y marcados sobre el círculo) y C (dos barras, sin círculo); y A con el punto
en la aceleración del auto, calibrando y sin sensor.

Elegido por el usuario (2026-10-26):

- **El diseño A.**
- **El punto como una bolita:** hacia donde empuja el auto (al frenar sube, en
  una curva a la derecha va a la izquierda), no con la aceleración.
- **Calibrar "abajo" al arrancar y en cada parada:** con la placa quieta 2 s;
  después de la primera, además con la ECU en 0 km/h.
- **Los máximos en FUERZA G, con el viaje:** frenada, aceleración y curva a
  cada lado, que se reinician con el viaje. VIAJE queda como estaba.
- **La placa va de frente** (el lado largo paralelo al eje de las ruedas):
  adelante sale de la gravedad y de ese lado, sin ángulo en `config.h`.

Lo que se hizo:

- **`lib/obd/fuerza_g`** (nuevo, sin Arduino):

  ```cpp
  struct Vector { float x, y, z; };
  struct ReglasFuerzaG { Vector derecha; uint32_t filtroMs; float toleranciaQuieta;
                         uint32_t quietaMs; uint32_t vigenciaMs; };
  struct LecturaG { bool calibrada, actual; float lateral, longitudinal;
                    Extremo frenada, aceleracion, curvaIzquierda, curvaDerecha; };
  class FuerzaG {
  public:
    explicit FuerzaG(const ReglasFuerzaG &definidas);
    void medir(const Vector &a, uint32_t momento, bool parado);
    LecturaG lectura(uint32_t ahora) const;
    void reiniciarMaximos();
    uint32_t calibraciones() const;
    bool cruda(Vector *a) const; // la última muestra, sin filtrar
  };
  void textoFuerzaG(float g, char *salida, size_t tam); // "0.41", sin el signo
  const char *nombreLongitudinal(float g);              // "Frenada", "Acel." o "Long."
  const char *nombreLateral(float g);                   // "Curva der", "Curva izq" o "Lateral"
  ```

  - **Calibración:** cada muestra a menos de `toleranciaQuieta` del promedio
    de las anteriores durante `quietaMs`; si no, o después de un hueco, la
    cuenta empieza de nuevo. La primera vez alcanza con eso; después, `parado`
    tiene que estar todo el rato (una curva larga y pareja también es "quieta"
    para el sensor). Arriba es el promedio normalizado; si no mide ~1 g
    (±0,2), no calibra.
  - **Ejes:** la derecha es `reglas.derecha` sin su parte vertical (no calibra
    si queda a menos de 30° de la vertical), y adelante = arriba × derecha.
    Así la inclinación de la pantalla, y que esté un poco torcida, no importan.
  - **Filtro:** de primer orden sobre las tres componentes, con `filtroMs`;
    después de un hueco arranca de la muestra.
  - Lateral positiva: el auto acelera hacia la derecha (curva a la derecha);
    longitudinal positiva: acelerando. Que el punto vaya al revés lo decide la
    página.
  - Los máximos son positivos y del valor filtrado; con la lectura vencida,
    quedan. `actual` compara con signo: la muestra puede ser un poco más nueva
    que el `ahora` de quien lee (otra tarea).
- **`src/imu`** (nuevo): `iniciar()`, `iniciado()`, `lectura(ahora)`,
  `parado(bool)`, `reiniciarMaximos()`, `cruda()`, `calibraciones()` y
  `lecturasFallidas()`.
  - En `busPlaca()` (`0x6B`, 300 kHz), con `i2c_master_probe()` antes. Se
    reinicia por software y comprueba `WHO_AM_I`; después `CTRL1` `0x40`
    (dirección automática y little-endian; las interrupciones en alta
    impedancia), `CTRL2` `0x16` (±4 g, 125 Hz), `CTRL5` `0x03` (su filtro al
    3,63 %: 4,5 Hz) y `CTRL7` `0x01` (solo el acelerómetro).
  - Una tarea (`imu`, núcleo 0 como el sonido, prioridad 2, 3 KB) lee los 6
    bytes cada `PERIODO_IMU_MS` (20 ms, con `vTaskDelayUntil`) y se los pasa a
    `FuerzaG::medir()` con un mutex; lo demás lo llama loop() con el mismo
    mutex. No depende del ELM: corre aunque loop() espere una consulta o
    dibuje. El driver `i2c_master` del ESP-IDF 5.5 toma un cerrojo del bus
    (`bus_lock_mux`) en cada transacción, así que el sonido y el expansor
    pueden usar el bus a la vez.
  - **Del TCA9554 no se toca nada:** las interrupciones del IMU (EXIO2 y
    EXIO3) no se usan.
  - Si algo no contesta: `IMU: no se pudo iniciar (...)` en el monitor y la
    página dice `Sin sensor`.
- **`config.h`:** `EJE_DERECHA_IMU` (`{1, 0, 0}`, **una suposición**: ver
  abajo), `PERIODO_IMU_MS` (20), `FILTRO_FUERZA_G_MS` (300),
  `QUIETA_FUERZA_G_MS` (2000), `TOLERANCIA_QUIETA_G` (0,05),
  `VIGENCIA_FUERZA_G_MS` (500) y `PERIODO_FUERZA_G_MS` (50). Con
  `PANTALLA_GIRADA`, la derecha va al revés.
- **`paginas`:** `FUERZA G` entre `VIAJE` y `DS3`, que sigue última (9 de 9
  puntos). `DatosFuerzaG {conSensor, lectura}`, `dibujar(..., viaje, fuerzaG,
  alarma)` y `muestraFuerzaG()`.
  - El círculo en la primera columna: centro en (80, 72) del área, 64 px = 1 g,
    los anillos de 1 g y 0,5 g, los ejes y `1 g`. Lo que pasa de 1 g queda en el
    borde. Al lado, casillas de 160×72 como las de Viaje: `casilla()` devuelve
    también la etiqueta del nombre, para las que cambian con el lado.
  - Sin sensor, `N/D` y `Sin sensor`; hasta calibrar, `...` y `Calibrando`;
    vencida, `--` y sin el punto; los máximos quedan.
  - Los toques, como en las demás: mantenerla apretada y soltarla también
    cambia de página.
- **`main.cpp`:** `Imu imu`, iniciado después del sonido.
  - `avisarParado()`, en cada vuelta: parado si en `LEYENDO` la velocidad
    actual es 0 (la velocidad ya entra en todas las consultas, por E10).
  - `periodoDibujo()`: con FUERZA G a la vista, `PERIODO_FUERZA_G_MS` (50 ms,
    como la animación de DS3; 100 desde el 2026-10-27, ver abajo); si no,
    `PERIODO_PANTALLA_MS`. En `loop()` y en `alEsperar()`.
  - Reiniciar el viaje reinicia también los máximos de G.
  - `!imu` (nuevo): la última muestra cruda (x, y, z y el total), lateral,
    longitudinal, los máximos, las calibraciones y las lecturas fallidas.
    `!info` dice si el IMU anda. La tabla de cada segundo termina en
    `G lateral +0.00 long. +0.00` (o `G --`).
- Sin cambios en la lista blanca ni en lo que se le manda al auto: la fuerza G
  no usa el ELM.

**Qué eje va a lo largo de la pantalla no se sabe.** Ni el ejemplo
`03_I2C_QMI8658` de Waveshare ni el esquemático dicen cómo está puesto el
QMI8658 en la placa. `EJE_DERECHA_IMU` = `{1, 0, 0}` (x) es una suposición;
se confirma con la prueba 2. Si está mal, la calibración puede fallar (el eje
vertical) o el punto ir para otro lado.

Verificado por el agente:

- `pio run` compila (RAM 10,8 %, flash 62,5 % de 3 MB; en E15, 10,7 % y
  62,2 %). Con `-Wall -Wextra -Wshadow`, sin warnings propios.
- `pio test -e native` 89/89: 14 nuevos en `test/test_fuerza_g/` (sin calibrar,
  calibrar quieta, frenada y curvas, la pantalla inclinada 25° y torcida 10°,
  el filtro con una vibración de 25 Hz y con un escalón, recalibrar parado en
  una subida y no en una curva larga, parado todo el rato, moverse antes de
  calibrar, la vigencia y los huecos, una escala o un montaje imposibles,
  reiniciar los máximos, `millis()` que da la vuelta y los textos). Se
  comprobó que fallan si se saca la condición de parado, la proyección de la
  derecha, el filtro o la tolerancia de quieta.
- **Render**, 135 revisiones, todas bien. Casos nuevos: `8-fuerza-g` (el de la
  maqueta: frenando 0.41 g en una curva a la derecha de 0.28 g),
  `za-fuerza-g-frenando`, `zb-fuerza-g-acelerando`, `zc-fuerza-g-afuera`
  (1,5 g: el punto en el borde), `zd-fuerza-g-calibrando`,
  `ze-fuerza-g-vencida` y `zf-fuerza-g-sin-sensor`; DS3 pasa a `9-ds3`. Revisa
  que FUERZA G vaya antes de DS3, dónde queda el punto (frenando en una curva a
  la derecha, arriba a la izquierda; acelerando en una a la izquierda, abajo a
  la derecha), que no salga del círculo, que sin valor no se vea, que nada pase
  de su casilla, que dibujar dos veces no redibuje nada y los toques (también
  mantener y soltar). Se comprobó que las del punto fallan con el punto al
  revés o sin limitarlo al círculo.
- Los objetos de la página piden ~8 KB de `malloc` en la PC (con punteros de
  64 bits), como VIAJE.

No hay **Datos descubiertos** medidos: la fila nueva (IMU) sale del datasheet
y del ejemplo.

Sin probar (falta la placa): todo el IMU (que conteste, el orden de los bytes,
los ejes), la tarea, la calibración en el auto, si el motor en ralentí mueve la
placa más de 0,05 g (no calibraría en las paradas), las consultas por segundo
con FUERZA G a la vista (redibuja hasta ~20 veces por segundo, como DS3) y la
RAM interna libre (la tarea suma 3 KB).

**En el auto, del registro de E13** (2026-10-27, estacionado en ralentí; ver
notas de E13): el IMU contesta y calibra. Quieto da ±0.00, así que el ralentí
no mueve la placa más de la tolerancia. En `VIAJE0002` no hay valor los
primeros 12 s (no estuvo quieta 2 s). Hay ratos de 0,3 a 0,6 g con el auto
parado (hasta -0,75 de lateral) que, al quedar quieta, vuelven de golpe a
0.00: es la recalibración en las paradas; la placa se habrá movido (¿en la
mano?). Con el auto parado no se pueden ver los ejes (prueba 2) ni el sentido
del punto. El usuario confirmó (2026-10-27) que la movía con la mano, para
probar, y que creía estar en FUERZA G cuando las consultas bajaron a 3 o 4
por segundo (de 156 a 171 s, con el punto moviéndose). Como decía la prueba 6,
`PERIODO_FUERZA_G_MS` pasó de 50 a 100 ms (en la rama de E13). Falta ver las
consultas con 100.

**Pruebas del usuario** (subir el firmware de esta rama):

1. Al arrancar, en el monitor: `IMU: listo` (si no, qué falló). A los 2 s con
   la placa quieta, `!info` dice `IMU: listo; 1 calibraciones...`.
2. **Los ejes, en el banco.** Con la placa derecha, como va en el auto, y
   quieta: `!imu` da un total de ~1,00 g, casi todo en el eje que va para
   arriba. Después, parada sobre su lado izquierdo (el lado derecho de la
   pantalla para arriba): `!imu` tiene que dar ~+1 g en x. Si da en otro eje o
   en -1, pasarme las dos líneas de `!imu`: se cambia `EJE_DERECHA_IMU`.
3. Con los ejes bien, reiniciar con la placa derecha y quieta (calibra) e
   inclinarla. Hacia la derecha, `!imu` da lateral negativa (la bolita rueda
   hacia la derecha); con la pantalla hacia adelante, longitudinal negativa.
4. En el auto, FUERZA G (después de VIAJE, el octavo de 9 puntos): quieto marca
   0.00; al frenar el punto sube y dice `Frenada`; en una curva a la derecha
   va a la izquierda y dice `Curva der`. Los máximos suben, y mantener apretada
   VIAJE los reinicia.
5. Después de una parada, `!info`: las calibraciones subieron. Si no suben
   nunca, el motor en ralentí mueve la placa más de 0,05 g: avisar.
6. En el monitor, las consultas por segundo con FUERZA G a la vista, contra
   MOTOR (5–7 en E5). Si bajan, se sube `PERIODO_FUERZA_G_MS` a 100.
7. `!info` con el motor andando: la RAM interna libre (en E5, 128 KB).

Para E13:

- La fuerza G está en `imu.lectura()` (lateral, longitudinal y los máximos):
  se puede agregar al CSV. `FuerzaG` corre en otra tarea: leerla siempre por
  `Imu`, que toma el mutex.

Para E14:

- El RTC va en `busPlaca()`, como el IMU. El IMU no toca el TCA9554.

### E13

Rama `e13-registro-sd`, desde `main` con E12. **Maqueta** (agente,
2026-10-26), en el lienzo de E4
(https://claude.ai/artifact/41eQde6wkKUL81ZuDnHBGX), filas "Registro en la
microSD (E13)": A (en Info, la tarjeta en el renglón de la dirección:
grabando, sin tarjeta y con error) y B (además, una marca `SD` ámbar en la
barra de todas las páginas); y el CSV en la planilla y en texto, en los dos
formatos.

Elegido por el usuario (2026-10-26):

- **A:** el estado de la tarjeta solo en Info. La dirección del adaptador
  sale de la pantalla (queda en `!info`).
- **CSV estándar:** coma entre columnas y punto decimal (para Google Sheets o
  Python), no `;` y coma decimal para Excel en español.
- **Sin resumen por viaje:** no hay `VIAJES.CSV`. Primero eligió que sí y
  después dijo que no. La última línea de cada archivo tiene la distancia y
  el tiempo en marcha; los máximos salen en la planilla.
- **Después, la marca `SD` en la barra** (2026-10-27, al probarlo en el auto:
  esperaba verla). Siempre que grabe, en gris; en ámbar si hay tarjeta pero
  no graba (como la B); sin tarjeta, nada.

**Probado por el usuario en el auto** (2026-10-27, antes de la marca de la
barra). Pasó `VIAJE0001.CSV` (75 s) y `VIAJE0002.CSV` (191 s), y el agente
los revisó. Los dos son con el auto estacionado, en ralentí (0 km/h,
refrigerante de 23 a 43 °C), con fecha 1980-01-01 en la FAT.

- El encabezado y las 21 columnas en todas las líneas; `Seg` sin saltos. `Volt
  ECU` siempre vacía (la ECU no da el PID `42`). La línea 0 trae solo RPM,
  refrigerante y velocidad (lo de la primera consulta); el resto llega en la 1.
- `VIAJE0001`: contacto, arranque en el segundo 1 (`RPM` 0, 143, 1076; `Lazo`
  `Apagado` y después `Abierto frio`; el voltaje de 11,9 a 13,6 V) y `En
  marcha s` desde ahí.
- Eventos: `Inicio de la placa: encendido | LEYENDO`, `Fallas: luz apagada;
  sin codigos` (al pasar por FALLAS) y `BT_PERDIDO (Conexion perdida)`.
- **Dos cortes de luz con el motor andando,** y la tarjeta se siguió leyendo
  en la PC, con un archivo por encendido. `VIAJE0001` termina en el segundo
  74 con 896 rpm y sin aviso de desconexión: se cortó la luz y el archivo
  quedó hasta el último bloque. En `VIAJE0002`, a los 188 s las consultas dan
  0, a los 189 llega `BT_PERDIDO` y hay líneas hasta los 190: la placa siguió
  prendida por lo menos 2 s después de que se apagó el adaptador.
- Consultas por segundo: casi siempre 6 o 7 (en E5, 5–7), con ratos de 2 a 4
  (`VIAJE0002`, de 156 a 171 s), que coinciden con la fuerza G moviéndose:
  puede ser FUERZA G redibujando (prueba 6 de E12). El CSV no dice qué página
  se veía.

No probado: la marca de la barra (se hizo después), `!sd`, sin tarjeta y con
una exFAT, y un viaje andando. **La marca de la barra la verificó el usuario**
el 2026-10-27, y también lo del banco ("en el banco funciona perfecto": las
pruebas 1 a 3, `!sd`, sin tarjeta y con una exFAT); un viaje andando, todavía
no.

**Un viaje andando, verificado** (2026-10-28, el agente con los registros del
2026-10-27, `VIAJE0003` a `VIAJE0008`). `VIAJE0004` (8,4 km, hasta 145 km/h) y
`VIAJE0007` (34 km) son viajes andando, con una línea por segundo y sin saltos
mientras hubo datos (2530 y 1907 s). Los 6 archivos están enteros: sin bytes
nulos, 32 columnas en todas las filas y la última línea completa. Cada uno
termina con el corte de luz: cinco, de 31 s a 4 min 15 s después de sacar la
llave (ya en `BT_PERDIDO`, con los saltos de 10 s de los intentos de conexión),
y `VIAJE0003` con el motor andando. La fecha de modificación de cada archivo
cae a 3 s o menos de su última línea: se perdió como mucho el bloque que
faltaba grabar. Con eso E13 queda verificada.

Lo decidió el agente, sin objeción del usuario: un archivo por encendido,
abierto con la primera lectura de la ECU (prender la placa en el banco no
deja archivos); el número, el siguiente al más alto de la tarjeta (no en
`Preferences`, como proponía el plan: así nunca pisa un archivo, aunque se
cambie la tarjeta); grabar cada 5 s. El corte de luz ya estaba decidido: si se
corta, se corta.

**La microSD va en modo SD de 1 bit, no por SPI.** Los pines del plan (CS 38,
MOSI 39, MISO 40, SCLK 41) son D3, CMD, D0 y CLK de la tarjeta. El ejemplo
`04_SD_Card` de la V2 la usa con el controlador SDMMC del S3 en 1 bit: es lo
probado por Waveshare en esta placa y no ocupa un SPI. Según el esquemático,
las cuatro líneas tienen 10 kΩ a 3,3 V (D3 alto deja la tarjeta en modo SD,
así que el GPIO38 no se toca), D1 y D2 no van a ningún GPIO y `CD` va a masa:
no hay detección de tarjeta. El ejemplo también pone el GPIO8 en alto (en la
V2 es `EXIO_INT`): parece un resto de la V1; el firmware no lo toca.

Lo que se hizo:

- **`lib/obd/registro`** (nuevo, sin Arduino):

  ```cpp
  class LineaCsv {                 // un renglón; los textos con , " o \n van entre comillas
  public:
    LineaCsv(char *destino, size_t tam);
    void texto(const char *t);     // nullptr o "": campo vacío
    size_t terminar();             // con '\n'; 0 si no entró
  };
  class Eventos {                  // "SIN_ECU (No responde) | LEYENDO", hasta 159 bytes
  public:
    void agregar(const char *texto);  // lo que no entra, cortado con "..." (sin partir UTF-8)
    const char *texto() const;
    void vaciar();
  };
  bool numeroDeViaje(const char *nombre, uint16_t *numero); // "viaje0012.csv" -> 12
  void nombreDeViaje(uint16_t numero, char *salida, size_t tam); // "VIAJE0012.CSV"
  class BuferTexto {               // cada línea entra entera o se cuenta como perdida
  public:
    void usar(char *memoria, size_t tam);
    bool agregar(const char *texto, size_t largo);
    size_t tomar(char *destino, size_t tam); // lo más viejo primero
    size_t usados() const;
    uint32_t perdidas() const;
  };
  enum class EstadoTarjeta { BUSCANDO, SIN_TARJETA, ILEGIBLE, LISTA, GRABANDO, ERROR, LLENA };
  void textoTarjeta(EstadoTarjeta estado, uint16_t viaje, char *salida, size_t tam);
  bool problemaTarjeta(EstadoTarjeta estado); // ILEGIBLE, ERROR o LLENA: ámbar
  ```

- **`src/tarjeta`** (nuevo): `iniciar()`, `abrir(encabezado, largo)`,
  `agregar(linea, largo)`, `estado()`, `viaje()` e `imprimir()`.
  - Una tarea (`tarjeta`, núcleo 0, prioridad 1, debajo del IMU y del sonido,
    4 KB de pila) monta la tarjeta al arrancar con
    `esp_vfs_fat_sdmmc_mount()` en `/sdcard` (20 MHz, sin formatear si falla,
    2 archivos). Sin tarjeta, el ESP-IDF da `ESP_ERR_TIMEOUT`
    (`SIN_TARJETA`); sin FAT, `ESP_FAIL` (`ILEGIBLE`, como cualquier otro
    error). **No se vuelve a buscar**: hay que ponerla antes de prender.
  - Cada `PERIODO_REGISTRO_MS` (5 s), o al pedir el archivo, pasa el búfer
    compartido a uno propio, lo escribe con `write()` y hace `fsync()`: la
    entrada del directorio queda con el tamaño de ese bloque. Si falla, cierra
    el archivo, queda en `ERROR` y lo reabre (`O_APPEND`) en la vuelta
    siguiente, con lo que faltó escribir; `ENOSPC` es `LLENA` y no sigue.
  - El archivo se crea con `O_CREAT | O_EXCL` después de leer la raíz
    (`opendir`/`readdir`). Arriba de `VIAJE9999.CSV`, `LLENA`.
  - Los dos búferes, de `TAM_BUFER_REGISTRO` (8 KB, ~50 s de líneas), en la
    PSRAM. Lo compartido, con un mutex; loop() nunca espera a la tarjeta.
  - El espacio libre (`esp_vfs_fat_info()`) se mide al montar y después de
    cada bloque.
- **`config.h`:** `PIN_SD_CLK` (41), `PIN_SD_CMD` (39), `PIN_SD_D0` (40),
  `PERIODO_LINEA_MS` (1000), `PERIODO_REGISTRO_MS` (5000) y
  `TAM_BUFER_REGISTRO` (8192).
- **`main.cpp`:**
  - `abrirRegistro()`, en `cambiarA(LEYENDO)` la primera vez: pide el
    archivo con el encabezado y anota `Inicio de la placa: <motivo>` (el de
    `!info`: `encendido`, `brownout (baja tension)`...).
  - `registrarSegundo()`, en cada vuelta de `loop()`: desde entonces, una
    línea por segundo, también sin conexión (con los datos vacíos). `Seg` es
    el segundo desde la primera línea; si `loop()` estuvo bloqueado, salta.
  - Columnas: `Seg`, las 14 magnitudes con la etiqueta y la unidad de la tabla
    (`unidadDe()`, nuevo en `datos`), `Consultas/s` (solo en `LEYENDO`),
    `Distancia km`, `En marcha s`, `G lateral`, `G long.` y `Eventos`. Los
    valores salen de `textoValor()`: vacíos si no son actuales (sin `N/D`,
    `...` ni `--`).
  - `anotar(formato, ...)` junta los eventos de la línea siguiente: los
    cambios de estado (como `== ...` en el monitor), las alarmas (se prende y
    termina), las fallas leídas en FALLAS si cambiaron desde la última
    (`anotarFallas()`: `Fallas: luz encendida; guardadas P0300 P0171`),
    `Viaje reiniciado` y `Tarjeta: error al escribir`.
  - `revisarTarjeta()`: cada cambio de estado de la tarjeta sale en el monitor
    (`tarjeta.imprimir()`).
  - `!sd` (nuevo) y `!info`: el estado, el nombre y el tamaño de la tarjeta,
    el espacio libre, el archivo, los KB grabados, las líneas perdidas, los
    errores y la pila libre de la tarea.
- **`pantalla`:** `MarcaSd` (`NINGUNA`, `GRABANDO`, `PROBLEMA`), nuevo
  parámetro de `pagina()` y `paginaCompleta()`: un rectángulo de 26×16 con
  `SD` en negro, 12 px antes del primer punto, en `ETIQUETA` grabando y en
  `AMBAR` con un problema. También sobre la barra roja de una alarma.
- **`paginas`:** `DatosInfo` cambia `direccion` por `tarjeta` y `viaje`;
  `tarjeta` elige también la marca (`GRABANDO`, o `PROBLEMA` si
  `problemaTarjeta()`). El renglón `Tarjeta`: `Grabando VIAJE0012` en
  blanco; `...` (buscándola) y `Sin tarjeta` en gris; `No se puede leer`,
  `Error al escribir` y `Tarjeta llena` en ámbar; `Lista` (montada, sin
  archivo) casi no se ve, porque Info es de `LEYENDO`.
- Sin cambios en la lista blanca ni en lo que se le manda al auto.

Verificado por el agente:

- `pio run` compila (RAM 11,1 %, flash 65,0 % de 3 MB; en E12, 10,8 % y
  62,5 %: suman el driver SDMMC y la FAT). Con `-Wall -Wextra -Wshadow`, sin
  warnings propios.
- `pio test -e native` 102/102: 13 nuevos en `test/test_registro/` (los
  renglones, campos vacíos al final, comillas, lo que no entra, los eventos
  cortados sin partir una letra de UTF-8, los nombres, el búfer lleno, de a
  partes y sin memoria, y los textos de Info).
- **Render**, 160 revisiones, todas bien. Casos nuevos: `6-info` (grabando),
  `pa-info-sin-tarjeta` y `pb-info-tarjeta-con-error`, y sin guardar, llena,
  ilegible, buscando y lista: el texto en el color de su estado (se comprobó
  que falla con el color cambiado), sin llegar a los botones, la marca de la
  barra como corresponde, y que dibujar dos veces no redibuje nada. En las
  alarmas, que el texto no llegue a la marca. Se comprobó que las de la marca
  fallan sin ella. Las demás páginas salen con la marca gris (grabando).
- El esquemático (la parte "SD Card") y el ejemplo `04_SD_Card` de la V2.

Sin probar (falta la placa y el auto): todo lo de la tarjeta (que monte, los
errores sin tarjeta y con una exFAT, escribir, el `fsync`, reabrir), cuánto
tarda la tarea, si alguna vez se pierden líneas, la RAM interna libre (la
tarea suma 4 KB de pila y la FAT sus búferes) y qué pasa con la tarjeta al
cortar la luz. `esp_vfs_fat_info()` puede tardar unos segundos la primera vez
en una tarjeta grande si la FAT no tiene la cuenta del espacio libre; es en la
tarea, pero retrasa abrir el archivo.

**Pruebas del usuario** (subir el firmware de esta rama; las 1 a 3 se hacen
en el banco):

1. Sin tarjeta: en el monitor, `Tarjeta: no hay, o no contesta (0x107)...`.
   Si el código es otro, pasármelo.
2. Con una microSD en FAT32 puesta antes de prender: `Tarjeta: <nombre>, X GB,
   Y GB libres; el archivo se abre con la primera lectura de la ECU`. `!sd`
   da lo mismo. Si dice `no se puede leer`, pasarme el código y el tamaño de
   la tarjeta.
3. Con una exFAT (o una de más de 32 GB sin formatear): `no se puede leer
   (0x...)`.
4. En el auto, con la tarjeta: al llegar a los datos, `Tarjeta: ...;
   VIAJE0001.CSV, 0 KB...` en el monitor, `Grabando VIAJE0001` en Info y la
   marca `SD` gris en la barra de todas las páginas. Cada tanto, `!sd`: los
   KB suben, sin líneas perdidas ni errores. Sin tarjeta, sin la marca.
5. Pasar por FALLAS y por VIAJE (mantenerla apretada para reiniciar).
   Después del viaje, en la PC: el CSV abre en Google Sheets, con una línea
   por segundo, los datos, la fuerza G y en `Eventos` `Inicio de la placa:
   encendido | LEYENDO`, las fallas y `Viaje reiniciado`.
6. Sacar la llave varias veces con el motor andando: cada vez la tarjeta se
   sigue leyendo en la PC, y cada encendido tiene su archivo, que termina unos
   segundos antes del corte.
7. En el monitor, las consultas por segundo con la tarjeta grabando, contra
   las de E12 (no deberían cambiar), y `!info`: la RAM interna libre.

Para E14:

- Con la hora, el registro puede llevar una columna `Hora` y el nombre o la
  fecha del archivo (la FAT toma la hora de `time()`; hoy los archivos
  quedan con 1980-01-01).
  `encabezadoRegistro()` y `registrarSegundo()` están en `main.cpp`.

### E14

Rama `e14-reloj`, desde `main` con E13. **Maqueta** (agente, 2026-10-27), en el
lienzo de E4 (https://claude.ai/artifact/41eQde6wkKUL81ZuDnHBGX), filas "Reloj
(E14)": A (la hora a la derecha, antes de la marca SD, en blanco), B (al centro
de la barra) y A en gris; y A con una alarma y sin hora.

Elegido por el usuario (2026-10-27):

- **A, en blanco.**
- **Ponerla solo con `!hora`**, no desde la pantalla (Info está llena).
- **También en la espera,** arriba a la derecha, chica y en gris.
- **La batería está puesta, sin medir C32.** Se hizo igual: si el RTC no queda
  alimentado con la placa apagada, se ve `--:--` al prender (el bit `OS`).

Lo decidió el agente, sin objeción del usuario: el RTC guarda la hora local,
sin zona horaria, en 24 h (con horario de verano, se corrige con `!hora`); la
columna `Hora` del CSV, con fecha y hora; y la fecha real en los archivos de la
FAT.

Lo que se hizo:

- **`lib/obd/reloj`** (nuevo, sin Arduino):

  ```cpp
  struct FechaHora { uint16_t anio; uint8_t mes, dia, hora, minuto, segundo; };
  constexpr uint8_t REGISTROS_HORA = 7; // 0x04 a 0x0A, en BCD
  bool leerRegistrosHora(const uint8_t registros[7], FechaHora *f); // false con OS o inválida
  void registrosHora(const FechaHora &f, uint8_t registros[7]);     // con el día de la semana
  bool interpretarHora(const char *texto, FechaHora *f); // "2026-10-27 14:35[:12]", 2000 a 2099
  bool fechaValida(const FechaHora &f);
  uint8_t diaDeLaSemana(uint16_t anio, uint8_t mes, uint8_t dia);   // 0 domingo
  int64_t segundosDesde1970(const FechaHora &f);  // la hora local, como si fuera UTC
  FechaHora fechaHoraDe(int64_t segundos);
  void textoHora(const FechaHora &f, char *salida, size_t tam);      // "14:35"
  void textoFechaHora(const FechaHora &f, char *salida, size_t tam); // "2026-10-27 14:35:12"
  ```

- **`src/rtc`** (nuevo): `iniciar()`, `poner(f)`, `ahora(&f)` e `imprimir()`.
  - En `busPlaca()` (`0x51`, 300 kHz), con `i2c_master_probe()` antes. Se
    inicia en `setup()` justo después de `iniciarPlaca()`, antes que la
    pantalla, para que la hora ya esté al prenderla (lo que imprime se pierde
    si el monitor no está abierto: `!info` lo repite).
  - Si `CONTROL_1` tiene el bit de detenido o el de 12 h, los pone en 0; si
    estaba en 12 h, la hora guardada no se usa. Lo demás (la capacidad del
    cristal) queda como está.
  - Lee los 7 registros: si son válidos y sin `OS`, pone la hora del sistema
    (`settimeofday()`, con `TZ` en `UTC0`). De ahí la toman la barra, el
    registro y la FAT (`get_fattime()` usa `localtime()`), sin volver a leer
    el RTC: el reloj del ESP32 se corre menos de 0,1 s por hora.
  - `poner()` escribe los 7 registros en una sola transmisión (el chip congela
    la hora mientras dura; escribir los segundos borra `OS`) y la hora del
    sistema.
  - `ahora()` lee la hora del sistema: se puede llamar desde la tarea de la
    pantalla.
  - Del TCA9554 no toca nada: la interrupción del RTC (EXIO4) no se usa.
- **`pantalla`:** `alPedirHora(funcion)`. Un timer de LVGL la pide cada 500 ms
  y pone el texto en dos etiquetas, que redibujan solo si cambia: así anda
  también mientras `loop()` está trabado conectando el BLE.
  - En la barra, en blanco (`--:--` en `GRIS` sin hora), alineada a la
    derecha 12 px antes de la marca SD, o de los puntos sin la marca, o del
    margen en las pantallas de estado. Con una alarma queda, en blanco sobre el
    rojo.
  - En la espera, `lv_font_montserrat_14` en `ETIQUETA`, arriba a la derecha.
- **`main.cpp`:** `Rtc rtc`; `horaParaPantalla()`; la columna `Hora` del CSV
  después de `Seg`, vacía sin hora; `!hora` (nuevo, en `consola`): sin nada
  imprime la hora, con fecha y hora la pone y anota `Reloj puesto en hora:
  ...` en el registro; `!info` con la hora.
- Sin cambios en la lista blanca ni en lo que se le manda al auto.

Verificado por el agente:

- `pio run` compila (RAM 11,1 %, flash 65,2 %). Con `-Wall -Wextra -Wshadow`,
  sin warnings propios.
- `pio test -e native` 113/113: 11 nuevos en `test/test_reloj/` (leer los
  registros, con bits sueltos, con `OS` y con valores imposibles; escribirlos;
  `!hora` bien y mal escrito, con años bisiestos y fuera de 2000–2099; el día de
  la semana; los segundos desde 1970 contra Python, y la ida y vuelta de cada
  día de 2000 a 2099).
- **Render**, 171 revisiones, todas bien. Casos nuevos: `pc-barra-sin-hora` y
  `0-espera-sin-hora`, y la hora en todas las páginas (`14:35`, de mentira).
  Revisa que la hora quede antes de la marca, o de los puntos sin ella, que la
  alarma no llegue a la hora, `--:--` en gris en la barra y en la espera, y que
  vuelva al blanco. Se comprobó que fallan con la hora sin correrse por la
  marca.

Sin probar (falta la placa): que el RTC conteste, la hora al arrancar, `!hora`,
si sigue con la placa apagada (la batería y C32) y cuánto se corre.

**Pruebas del usuario** (subir el firmware de esta rama):

1. Al arrancar, `!info` termina en `Reloj: ...`. La primera vez lo normal es
   `Reloj: sin hora (se detuvo...)`, y la barra y la espera dicen `--:--`. Si
   dice `Reloj: no anda`, avisar.
2. `!hora AAAA-MM-DD HH:MM` con la hora de ahora: `Reloj: <la hora>`, y en la
   pantalla aparece al medio segundo.
3. Reiniciar con el USB puesto (botón RESET): la hora sigue bien.
4. **La prueba de la etapa:** una noche con el auto apagado. Al día siguiente,
   la hora sigue bien. Si dice `--:--`, el RTC no quedó alimentado por la
   batería: medir C32 con la llave sacada.
5. En el CSV del viaje siguiente, la columna `Hora`, y la fecha del archivo en
   la PC.
6. Después de unas semanas, cuánto se corrió (para saber si hace falta
   corregirlo con el registro `Offset` del PCF85063).

**Verificado por el usuario** (2026-10-27): "funciona perfecto, se mantuvo
desde anoche" (la prueba 4, una noche con el auto apagado). Así que la batería
alimenta al RTC con la placa apagada, como dice el esquemático; C32 no se
midió. Falta ver la columna `Hora` del CSV, la fecha de los archivos y cuánto
se corre en unas semanas.

**La columna `Hora` y la fecha de los archivos, verificadas** (2026-10-28, el
agente con los registros del 2026-10-27, `VIAJE0003` a `VIAJE0008`). En los 6,
`Hora` está llena en todas las líneas y avanza con `Seg` (la diferencia entre
las dos se mueve 1 s como mucho en todo el archivo). En la Mac, la fecha de
creación de cada archivo es la de su primera línea (1 s antes) y la de
modificación, la de la última (3 s o menos después: el último bloque grabado;
la FAT guarda los segundos de a 2). Falta cuánto se corre en unas semanas.

### E15

Rama `placa-v2`, desde `main` con E11 (que el usuario pidió mergear antes).

Lo que se hizo:

- **`src/placa`** (nuevo): `iniciarPlaca()`, `busPlaca()` y
  `salidaExpansor(pin, nivel)`. El I2C de la placa (`I2C_NUM_0`, 300 kHz) y el
  TCA9554, que antes creaba el sonido. Se inicia en `setup()` antes que la
  pantalla. `salidaExpansor()` lee y cambia solo el bit del pin: primero el
  nivel y después la dirección, así no pasa por el otro. Comprueba el
  expansor con `i2c_master_probe()`.
- **Pines del TCA9554 que se usan:** 1 (`BL_EN`, el regulador de la luz), 5
  (el reset de la pantalla, V2) y 7 (el amplificador, E11). Los demás quedan
  como entradas, como arrancan; el 6 (`SYS_EN`) no se toca.
- **`pantalla_hw`:**
  - `iniciar(brillo)`: la luz por PWM en el GPIO42 con el LEDC del ESP-IDF,
    como el programa de fábrica de la V2 (baja velocidad, timer 3, canal 1, 8
    bits, `FRECUENCIA_RETROILUMINACION_HZ` 50 kHz), arranca apagada (duty
    255) con `BL_EN` en bajo; después del primer cuadro negro, el brillo y
    `BL_EN` en alto.
  - El reset del panel por EXIO5 (alto 30 ms, bajo 250, alto 30, los tiempos
    del ejemplo). Sin el expansor no se hace (al prender, el panel arranca
    reseteado) y lo dice el monitor.
  - Ya no toca el GPIO21 (en la V2, `LCD_TE`: antes quedaba como salida en
    alto) ni el GPIO8 (`EXIO_INT`).
  - `ponerBrillo(0-255)` y `brillo()`: duty = 255 − brillo, como el programa
    de fábrica (en bajo, al máximo).
- **`src/brillo`** (nuevo, como el de E6 que no quedó en ningún commit):
  `iniciar()`, `nivel()`, `valor()` y `poner(nivel)`. Niveles de 1 a
  `CANT_NIVELES_BRILLO` (5), con `BRILLOS` {12, 40, 90, 160, 255} en
  `config.h`. Se guarda en `Preferences` (`pantalla`/`brillo`); sin nada
  guardado, el más alto (como hasta ahora).
- **`paginas`, Info:** los 4 renglones pasan de 36 a 24 px, y abajo va
  `Brillo` con `-` y `+` (56×38, borde gris, `LV_SYMBOL_MINUS` y
  `LV_SYMBOL_PLUS`) y un segmento por nivel en el medio. `DatosInfo` suma
  `brillo` (el nivel). `Accion::BAJAR_BRILLO` y `SUBIR_BRILLO`, al soltar el
  botón, que se queda con el toque (no cambia la página).
- **`main.cpp`:** `cambiarBrillo()` (desde Info o `!brillo N`): lo guarda, lo
  pone en la luz y escribe `Pantalla: brillo N de 5`. `!info` dice si anda el
  I2C de la placa y el brillo.
- **`consola`:** `!brillo N` (de 1 a 5), que anda en cualquier estado: así
  se prueba en el banco, sin la ECU.
- **`sonido`:** usa `busPlaca()` y `salidaExpansor()`; ya no crea el bus.
- **`config.h`:** sale `PIN_PANTALLA_RST`; `PIN_RETROILUMINACION` pasa a 42;
  nuevos `PIN_EXPANSOR_RESET_PANTALLA`, `PIN_EXPANSOR_LUZ`,
  `FRECUENCIA_RETROILUMINACION_HZ`, `BRILLOS` y `CANT_NIVELES_BRILLO`.

Verificado por el agente:

- `pio run` compila (RAM 10,7 %, flash 62,2 % de 3 MB). Con `-Wall -Wextra
  -Wshadow`, sin warnings propios. `pio test -e native` 75/75 (`lib/obd` no
  cambió).
- **Render**, 105 revisiones, todas bien. Caso nuevo `p-info-brillo-maximo`;
  `6-info` con el brillo en 3. Revisa que `-` y `+` pidan lo suyo una sola
  vez sin cambiar de página, que dibujar dos veces no redibuje nada y que
  los datos de Info sigan sin llegar a los botones.

Sin probar (falta la placa): todo. Es la primera vez que el firmware toca el
GPIO42, `BL_EN` y el reset por el expansor.

**Pruebas del usuario** (subir el firmware de esta rama):

1. Al prender: la pantalla negra, sin basura, y después la espera como
   siempre. En el monitor, sin `Placa: no se pudo iniciar...` ni `Pantalla:
   sin el TCA9554...`. `!info`: `Placa: I2C listo; brillo 5 de 5`.
2. `!brillo 1` a `!brillo 5` en el banco: la luz cambia en cada nivel. Si va
   al revés (1 es el más claro), la polaridad es la otra: avisar. Anotar el
   nivel cómodo de día y de noche.
3. En el auto, en Info: `-` y `+` cambian el brillo y los segmentos, sin
   cambiar de página.
4. Apagar y prender: arranca con el último brillo.
5. Que siga sonando `!pitar` (el sonido ahora usa el bus de `placa`).

**Probado por el usuario** (2026-10-27): la pantalla arranca bien y el brillo
cambia, pero **bajar un nivel desde el máximo dejaba la pantalla casi
apagada**, y era difícil volver a subirlo sin ver. Se midió en el banco (rama
`brillo-curva`): el agente subió el firmware y mandó `!luz V` (nuevo: la luz
de 0 a 255, sin guardar) por el USB, con el usuario mirando.

- Con 90 o menos la luz se apaga del todo; con 97 se ve. 160 a 5 kHz se ve
  igual que a 50 kHz: no es la frecuencia del PWM (la hoja de datos del
  AP3032 pide más de 25 kHz para el modo standby, y 50 kHz lo cumple). Con los
  niveles de E15 (12, 40, 90, 160, 255), los tres de abajo quedaban apagados y
  el 4 muy oscuro.
- Los niveles nuevos los eligió el usuario: **100, 133, 167, 200 y 255** (255
  el máximo, 200 el anterior y parejos hasta 100). Los vio uno por uno con
  `!brillo 1` a `!brillo 5`: el 1 se lee y los pasos se ven bien. El 100 deja
  poco margen sobre el corte (90-97): si en el auto el 1 se ve demasiado
  oscuro, subirlo.
- `!brillo N` ahora pone la luz aunque sea el mismo nivel (para volver después
  de un `!luz`).
- Falta: que el nivel se mantenga al apagar y prender (prueba 4), y verlo de
  día en el auto.

En el mismo banco, `!info` dio **74 KB de RAM interna libre** (bloque mayor
34 KB), sin el adaptador conectado; en E5 eran 128 KB. Se la llevan la
microSD (E13), las tareas del IMU, el sonido y la tarjeta y el RTC. Conviene
mirar `!info` en el auto, con el BLE conectado. Y el reloj marcaba 13:13 con
la Mac en 01:14: tenía hora (sin `OS`), pero 12 horas corrida; se pone con
`!hora`.

Para E12 y E14:

- El IMU y el RTC van en `busPlaca()`; el TCA9554, con `salidaExpansor()`
  (sus interrupciones son entradas: EXIO2 y EXIO3 del IMU, EXIO4 del RTC,
  que se leerían con un registro de lectura que hoy no hay).

### E16

Rama `e16-swipe`, desde `main` con el arreglo del brillo. Sin maqueta: no
cambia lo que se ve.

Lo que se hizo:

- **`paginas`:** `alDeslizar()`, en el indev del touch
  (`lv_indev_add_event_cb()`, con `PRESSED` y `RELEASED`). LVGL 9.3 le manda
  al indev el `PRESSED` del objeto donde se apoyó el dedo y el `RELEASED`
  del objeto donde se levantó, antes que a ellos y antes del `CLICKED`; si
  el dedo pasa de un botón a otro objeto, el botón recibe `PRESS_LOST` y el
  otro no recibe `PRESSED` ni `CLICKED` (`indev_proc_press()`). Así:
  - Al apoyar, guarda dónde. Al soltar, si el dedo se corrió `DESLIZAR_PX`
    (80) o más de costado, y más de costado que en vertical, es un
    deslizamiento: con las páginas a la vista, `tomarToque()` da +1 (hacia
    la izquierda, la siguiente) o -1.
  - Los botones de Info no cuentan su `CLICKED` si fue un deslizamiento
    (`alSoltarBoton()`): un deslizamiento dentro de Reconectar no reconecta.
  - Sale `alTocar()`: tocar no cambia de página. Sale el "toque largo" de
    Viaje, que ya no hace falta.
  - Mantener apretada Viaje se cancela si el dedo se corre más de
    `QUIETO_PX` (20) (`LV_EVENT_PRESSING` en la raíz): deslizar despacio no
    la reinicia.
- **`config.h`:** `DESLIZAR_PX` y `QUIETO_PX`.
- La espera sigue igual: tocarla reintenta y mantenerla reinicia la placa.

Verificado por el agente:

- `pio run` compila. `pio test -e native` 113/113 (no hay lógica nueva en
  `lib/obd`).
- **Render**, 181 revisiones, todas bien. `deslizar()` mueve el dedo en 5
  pasos de 30 ms. Todas las de toques pasaron a deslizamientos; nuevas:
  tocar no cambia de página (Info, DS3, Viaje, Motor), deslizar desde
  Reconectar, dentro de Reconectar, de `-` a `+` y desde Olvidar cambia de
  página sin apretarlos, justo `DESLIZAR_PX` cuenta y menos no, más vertical
  no cuenta, y en Viaje el dedo corrido no reinicia. Se comprobó que fallan
  sin la protección de los botones y sin cancelar Viaje.

Sin probar (falta la placa): cómo responde el touch real. Un deslizamiento
necesita por lo menos dos lecturas del touch (cada ~33 ms, salvo mientras se
manda una imagen): uno muy rápido, de menos de ~70 ms, se puede perder.

**Pruebas del usuario:**

1. En todas las páginas, deslizar hacia la izquierda pasa a la siguiente y
   hacia la derecha, a la anterior; también en DS3 y sobre la barra.
2. Tocar no cambia de página.
3. En Info, deslizar empezando sobre Reconectar, Olvidar, `-` o `+`: cambia
   de página y no reconecta, no olvida ni cambia el brillo. Tocar los botones
   sigue andando.
4. En Viaje, mantener apretada 3 s, quieta, la reinicia; moviendo el dedo,
   no.
5. Si a veces no toma el deslizamiento: si es rápido o lento, y en qué
   página. `DESLIZAR_PX` se puede bajar.

**Verificado por el usuario** (2026-10-27): deslizar sobre los botones de Info
no los aprieta, "aprobado y funcionando bien". En la prueba de E17 ya había
recorrido las páginas deslizando en el auto. No mencionó mantener Viaje con el
dedo corrido.

### E17

Rama `e17-pantalla-fluida`, desde `main` con el plan del asistente. Sin
maqueta: no cambia lo que se ve.

Elegido por el usuario (2026-10-27), antes de programar:

- **LVGL y el giro de la imagen a `-O2`**, no todo el proyecto (ver abajo).
- **Cada paso en un commit aparte, que sube y mide el usuario**: en el banco
  con `!info`, en el auto con el CSV. El agente no usó el USB.
- **El QSPI queda en 40 MHz:** el paso 5 no se hizo.
- **FUERZA G vuelve a 50 ms si las mediciones dan:** está en el último paso y
  se confirma en el auto.

**Todo iba a `-O0`, no a `-Os`.** El plan decía que LVGL estaba compilado para
tamaño. En el `pio run -v` no hay ningún `-O` en los comandos de `src`, LVGL,
`lib/obd`, el driver del panel ni el core de Arduino (ver **Datos
descubiertos**), así que GCC queda en `-O0`. Por eso el giro costaba ~20 ms
(~40 ciclos por píxel) y dibujar una pantalla entera ~30–60 ms. Optimizar
achica el programa en vez de agrandarlo: 65,2 % → 58,9 % de 3 MB con LVGL y
el giro a `-O2`.

**Los commits, para medir cada paso:**

| Commit    | Paso | Qué cambia |
| --------- | ---- | ---------- |
| `5c8a494` | 1 | Solo las mediciones (la base) |
| `d5d2edc` | 2 | El giro de a 2x2 píxeles, todavía a `-O0` |
| `58c97a3` | 3 | LVGL y el giro a `-O2` |
| `76cda5c` | 4 | El envío en otra tarea, con dos imágenes |
| `9e65ccb` | 6 | Cada envío empieza con el barrido del panel (`LCD_TE`) |
| `7176bbd` | 7 | FUERZA G cada 50 ms |
| `e21b10e` | — | Sin warnings con `-Wshadow` (no cambia lo que hace). Después, solo el PLAN |

Lo que se hizo:

- **Paso 1, las mediciones** (`pantalla_hw`):

  ```cpp
  struct TiemposCuadro {   // en microsegundos
    uint32_t dibujo;       // LVGL dibujando lo que cambió, en quien la tiene
    uint32_t espera;       // quien tiene LVGL, esperando el envío
    uint32_t giro;         // pasar la imagen a vertical (las 10 tandas)
    uint32_t envio;        // desde que se empieza a mandar hasta que el panel recibió todo
  };
  struct MedidasPantalla { uint16_t cuadros; TiemposCuadro promedio, maximo; uint16_t barridos; };
  TiemposCuadro medirRefresco();   // dibuja y manda la pantalla entera (para !info)
  MedidasPantalla medidas() const; // las del último segundo completo
  ```

  - `dibujo` va desde `LV_EVENT_REFR_START` (un `lv_display_add_event_cb`)
    hasta la última zona, sin lo que esperó. El envío termina cuando el panel
    recibió la última tanda: antes, `ultimoEnvioMs` se medía hasta encolarla
    (~1 ms menos).
  - Hasta el paso 4, `espera` es todo el envío (`loop()` lo esperaba). Desde
    el 4, lo que LVGL espera al envío anterior. Lo que se le resta a las
    consultas es `dibujo` + `espera`.
  - El segundo se cierra en `atender()`, con LVGL tomado: los cuadros, la suma
    y el máximo pasan a `medidas()`.
  - **La tabla de cada segundo** suma
    `pantalla 12 cuadros/s, dibujo 3.2/7.9, espera .../..., giro .../..., envio .../... ms`
    (promedio/máximo).
  - **`!info`:** `Pantalla en el ultimo segundo: ...; LCD_TE N barridos/s` y
    `Pantalla: refresco completo X ms: dibujo, espera, giro, envio` (refresco
    completo = dibujo + envío).
  - **El CSV** suma 10 columnas antes de `Eventos`: `Pagina` (el título, solo
    en `LEYENDO`; antes no se sabía qué página se veía), `Cuadros/s` y el
    promedio y el máximo de cada tiempo. La línea pasa de `char[400]` a
    `LARGO_LINEA_REGISTRO` (512); el búfer de 8 KB de la tarjeta queda en ~35 s
    de líneas (antes ~50), y se escribe cada 5.
- **Paso 2, el giro por bloques** (`src/giro.h/.cpp`, nuevo, sin Arduino ni
  LVGL): `girarTanda(imagen, ancho, alto, fila, filas, girada, destino)`. De a
  2x2 píxeles: lee dos palabras de 32 bits, una de cada fila de la imagen (x y
  x + 1), y escribe dos (las filas x y x + 1 del panel, con las columnas de
  las dos filas de la imagen juntas), con los bytes de cada píxel dados vuelta
  de a dos. Lee la PSRAM en orden, fila por fila: cada línea de la caché (32
  bytes) se trae una vez. No hacen falta bloques más grandes: lo que se
  escribe va a la RAM interna, sin caché, y recorrer la imagen por columnas
  sería peor (con 1280 bytes por fila, 172 filas caen en solo 16 conjuntos de
  la caché de 8 vías). Usa `uint32_t` con `may_alias` para no romper las
  reglas de alias a `-O2`. Pide `ancho`, `alto`, `fila` y `filas` pares y los
  punteros de a 4 bytes (las imágenes y las tandas lo son).
- **Paso 3, `-O2`** (`optimizar.py`, nuevo, `pre:` en `platformio.ini`): con
  `env.AddBuildMiddleware()`, los archivos de `*/lvgl/src/*` y `*src/giro.cpp`
  se compilan con `-O2` al final de los flags. Comprobado en el `pio run -v`:
  solo esos llevan `-O2`; `pantalla_hw.cpp`, `main.cpp`, `lib/obd`, el core y
  el driver siguen sin `-O`. El lazo del giro queda en 23 instrucciones cada 4
  píxeles, con el `loop` del Xtensa: unos 3 ms por cuadro más los fallos de
  caché (estimado, sin medir).
- **Paso 4, el envío en otra tarea** (`pantalla_hw`):
  - LVGL dibuja en **dos imágenes** de 640x172 en la PSRAM (modo `DIRECT` con
    dos búferes, +220 KB). En cada cuadro dibuja solo lo que cambió, y antes
    copia de la otra imagen lo que cambió en el cuadro anterior (lo hace LVGL:
    `refr_sync_areas()`).
  - **La tarea `envio`** (núcleo 0, prioridad 2, 4 KB de pila) espera una
    notificación con el puntero de la imagen, la gira y la manda en las 10
    tandas de siempre, anota giro y envío y da `envioListo`.
  - `alDibujar()` (el `flush_cb`), en la última zona, le pasa la imagen a la
    tarea y **no** llama a `lv_display_flush_ready()`: lo hace
    `terminarEnvio()` cuando la tarea dio `envioListo`. LVGL ya sigue en la
    otra imagen.
  - `terminarEnvio(espera)` lo llama quien tiene LVGL: `atender()` sin esperar,
    en cada vuelta (`loop()` o la tarea de E9), y el `flush_wait_cb` de LVGL
    esperando hasta `TIMEOUT_ENVIO_MS` (2 s, más que lo que tarda la tarea con
    todas las tandas vencidas). LVGL lo llama cuando necesita la imagen que se
    está mandando: al empezar el cuadro siguiente, para copiar lo que cambió.
  - Al arrancar, el primer cuadro negro se espera antes de prender la luz,
    como antes. `medirRefresco()` espera el envío para tener sus tiempos.
- **Paso 6, `LCD_TE`:** en los comandos del panel, `TEON` (`0x35 0x00`, solo el
  blanking vertical), entre `0x11` y `0x29`. El GPIO21 como entrada con
  pull-down (si el panel no lo maneja, queda en bajo y sin interrupciones) y
  una interrupción en el flanco de subida (`gpio_install_isr_service(0)`, que
  acepta que ya esté instalado), que cuenta y da un semáforo. La tarea de envío
  espera el barrido siguiente (hasta `ESPERA_TE_MS`, 40) antes de mandar,
  **solo si hubo uno en los últimos 200 ms**: si el TE no llega, manda como en
  el paso 4. El envío medido no incluye esa espera. `PIN_PANTALLA_TE` en
  `config.h`. No se toca nada del TCA9554.
- **Paso 7:** `PERIODO_FUERZA_G_MS` de 100 a 50.
- **El render** compila `giro.cpp` y compara `girarTanda()` con el giro de a un
  píxel que tenía `pantalla_hw`, derecha y girada, con una imagen al azar en
  tandas de 64. La tarea de envío y las dos imágenes no se pueden probar ahí
  (el `PantallaHw` del render es de mentira).
- README: las columnas nuevas del CSV, qué es cada tiempo, `!info` y cómo se
  compila.

**Cómo queda la concurrencia.** LVGL sigue teniendo un solo dueño por vez,
`loop()` o la tarea `pantalla` de E9, con el mismo mutex: todas las funciones
de LVGL (también `lv_display_flush_ready()`) las llama quien lo tiene. La tarea
`envio` no llama a LVGL ni toca sus objetos: recibe un puntero a una imagen
que LVGL ya terminó, y LVGL no la vuelve a usar (ni para leerla) hasta que
alguien con el mutex tomó `envioListo`. La notificación y el semáforo de
FreeRTOS ordenan la memoria entre las dos tareas (los tiempos que anota la
tarea se leen después de tomar el semáforo). Las tandas, su semáforo y el panel
pasan a ser solo de la tarea de envío (el panel se inicia antes de crearla).
La caché de la PSRAM del S3 es una sola para los dos núcleos. La interrupción
del `LCD_TE` solo cuenta, anota el tick y da su semáforo.

Verificado por el agente:

- Cada commit compila. Programa: 65,2 % (paso 1) → 58,9 % (paso 3) → 59,0 %
  (pasos 4 a 7) de 3 MB; RAM estática 11,2 %. Con `-Wshadow`, sin warnings
  propios (hubo dos y se arreglaron en `e21b10e`). `pio test -e native`
  113/113 (`lib/obd` no cambió).
- **Render**, 183 revisiones, todas bien: las 181 de antes y las dos del giro.
  Se comprobó que las del giro fallan con un píxel cambiado de lugar y sin dar
  vuelta los bytes.
- Los tamaños de **Datos descubiertos**, compilando cada variante.

Sin probar (falta la placa): todo en el hardware. Los tiempos reales de cada
paso, que la pantalla se vea bien con las dos imágenes (sin restos de cuadros
viejos), si el `LCD_TE` llega y a cuánto barre el panel, si con él
desaparecen los cortes, la RAM interna libre (la tarea suma 4 KB) y las
consultas por segundo en cada página, también con FUERZA G a 50 ms y DS3.

**Pruebas del usuario.** Para medir un paso: `git checkout <commit>`,
`pio run -t upload` y volver con `git checkout e17-pantalla-fluida`. En el
banco, con cada commit, `!info` dos o tres veces (después de que entró el DS3)
y pasarme las líneas `Pantalla...` y `RAM interna libre`:

1. `5c8a494` (la base): debería dar como antes, refresco completo ~64 ms,
   giro ~20, envío ~32 y espera igual al envío.
2. `d5d2edc`: baja el giro (todavía a `-O0`).
3. `58c97a3`: el giro, menos de 8 ms (el objetivo); baja el dibujo. El envío
   queda cerca de 11–12 ms, lo que tarda el QSPI a 40 MHz.
4. `76cda5c`: la espera, en 0 o casi. La pantalla, igual que antes: el DS3
   entrando sin saltos ni restos, tocar para reintentar, mantener para
   reiniciar con la barra roja creciendo pareja. En el monitor, sin
   `Pantalla: no se pudo crear la tarea del envio` ni `el envio de una imagen
   no termino`.
5. `9e65ccb`: `LCD_TE N barridos/s`. Anotar N (con 0, el TE no llega y se
   manda sin esperar). Mirar la entrada del DS3: sin una línea donde la imagen
   se vea cortada.
6. La rama entera (`e17-pantalla-fluida`, con FUERZA G a 50), **en el
   auto** con la tarjeta: ~1 minuto en cada página, también FUERZA G
   (moviendo la placa) y DS3, y pasarme el CSV. Las consultas por segundo,
   iguales en todas (6–7); en FUERZA G, ~20 cuadros por segundo. Si en
   FUERZA G bajan, se vuelve a 100 (`git revert 7176bbd`). Cambiar de página
   deslizando, más suave.
7. `!info` en el auto con el motor andando: la RAM interna libre.

**Verificado por el usuario** (2026-10-27: "funciona bien la verdad"), con la
rama entera en el auto: `VIAJE0003.CSV`, 2 min 10 s estacionado en ralentí,
con el motor frío (18–24 °C). Fue directo a la prueba 6: no midió cada paso
en el banco. Del CSV (ver también **Datos descubiertos**):

- **Por cuadro:** giro ~7,2 ms (máximo 11; el objetivo era menos de 8),
  envío ~13,8 (máximo 17,4). El dibujo depende de la página: DS3 ~14 ms a 20
  cuadros/s, Motor ~9 a 4–6 cuadros/s, Info ~3, y al cambiar de página
  11–29. Antes, un refresco completo eran 64 ms en el banco y 93 en Motor.
- **Espera:** 0,0 en todos los segundos (0,1 de máximo en algunos de DS3),
  salvo al volver de reconectar (4,3 de promedio y 12,8 de máximo).
- **Consultas por segundo:** 6–7 en Motor (6,4 de promedio, como en E3),
  Info (6,6), Aire/Volt, Carga, Mezcla y Viaje. Fallas, 3 los dos segundos de
  la entrada (la lectura de las fallas, como en E5).
- **DS3: 5,6** (4–7, en 38 s), ~1 menos que las demás. Son 20 cuadros/s de
  ~14 ms: ~280 ms de cada segundo dibujando en `loop()`. Antes de E17 no se
  midió (en E9 quedó sin anotar), pero con el envío de 33 ms en `loop()` y
  LVGL a `-O0` no podía dar mejor. En DS3 no hay datos a la vista: lo que se
  pierde es para el registro, el viaje y las alarmas. Cómo bajarlo, en
  **Ideas para después**.
- **FUERZA G a 50 ms, sin probar de verdad:** con el auto y la placa quietos
  la G queda en ±0.00, el punto no se mueve y LVGL no redibuja (0
  cuadros/s). Las consultas dieron 6–7, pero sin la carga que importa. Queda
  en 50.
- **Reconectar:** a los 35 s, `CONECTANDO_BT` sin `BT_PERDIDO` (estaba en
  Info: es Reconectar) y `LEYENDO` ~3 s después. Falta la línea del segundo
  35 porque conectar bloquea `loop()`, como antes de E17. Mientras tanto LVGL
  lo tuvo la tarea de E9 y el envío aparte siguió andando.
- Las medidas de cada línea son las del último segundo completo: al cambiar de
  página, la línea puede llevar la página nueva con las medidas de la
  anterior (el segundo 72 dice `FUERZA G` con los 20 cuadros/s de DS3).

Falta verificar:

- FUERZA G con el punto moviéndose (andando, o moviendo la placa con la mano
  como en E13): ~20 cuadros/s y las consultas en 6–7. Si bajan,
  `git revert 7176bbd`.
- `!info` una vez, con el motor andando: `LCD_TE N barridos/s` (si el TE
  llega) y la RAM interna libre.

Para E20:

- Compilar todo el proyecto a `-Os` bajaría el programa a ~50 % (ver **Ideas
  para después**): ayuda con la flash de E20, pero hay que volver a probar todo.
- La tarea `envio` corre en el núcleo 0 con el BLE (y con el Wi-Fi de E20): si
  hace falta, se le puede bajar la prioridad, a costa de más `espera`.

### E19

Rama `e19-microfono`, desde `main` con el plan sin E18. **Descartada** por el
usuario (2026-10-27) después de probarla: "funciona", pero el parlante se oye
bajo. **El código se sacó y no quedó en ningún commit;** la rama se borró. Lo
que sigue es para quien la retome.

Elegido por el usuario antes de programar: grabar con **dos dedos** apoyados
0,5 s en cualquier pantalla (no estaba entre las opciones del plan; sin BOOT
de respaldo), mientras quede alguno apoyado y hasta 30 s; `!grabar N`; una
marca roja `MIC` y un pitido corto antes de grabar; guardar en la microSD como
`VOZ0001.WAV` (mono, 16 kHz, el promedio de los dos micrófonos) y reproducir
enseguida.

Cómo estaba hecho:

- **El I2S en full duplex:** la entrada (GPIO6) creada junto con la salida
  (`i2s_new_channel(&config, &salida, &entrada)`), con la misma configuración;
  la salida siempre prendida, como en E11, y la entrada solo mientras se
  grababa. En el S3 (I2S versión 2) prender y apagar la entrada no corta la
  salida.
- **El ES7210** en el I2C de la placa, sin crear otro bus, con la secuencia
  del driver de Espressif del ejemplo (`es7210.c`): como esclavo, MIC1 y MIC2
  sin TDM, 16 bits en formato I2S, apagado salvo al grabar. Una tarea en el
  núcleo 0 (prioridad 3) avisaba con el pitido, lo prendía, tiraba 150 ms y
  pasaba a mono en un búfer de 30 s en la PSRAM.
- **El sonido** reproducía PCM desde la PSRAM, de a 10 ms, y los pitidos
  cortaban la reproducción. **El WAV** lo escribía la tarea de la tarjeta, de a
  2 KB por un búfer en la RAM interna (la microSD no lee de la PSRAM con DMA),
  siempre después del bloque del registro.
- **Los dos dedos:** la respuesta del touch trae la cantidad en `[1]`. Al
  aparecer el segundo dedo, `lv_indev_reset()` desde la lectura del touch le
  manda `LV_EVENT_INDEV_RESET` al objeto apretado (sin `RELEASED` ni
  `CLICKED`), y hasta levantar todos LVGL ve el dedo levantado; la espera, la
  raíz de las páginas y Olvidar tienen que tomar `INDEV_RESET` como
  `PRESS_LOST`.
- Costaba ~12 KB de RAM interna (estimado) y 0,4 % de programa.

Lo que no se llegó a saber: si el touch informa dos dedos (el usuario no lo
detalló), cuánto bajaban las consultas grabando, la RAM interna libre y cómo
se oía una grabación en el auto con el motor andando.

### E22

Rama `e22-viajes-pasados`, desde `main` con el cierre del plan (PR #24: el
usuario pidió commitearlo y mergearlo antes de empezar). Sin maqueta aparte:
el usuario eligió el diseño entre dos maquetas de texto (las 8 casillas con la
fecha en la barra, o una tabla de los últimos 4).

**Cómo se guarda** (`src/guardados`, con la lógica en
`lib/obd/src/anteriores`):

- En el espacio `viajes` de `Preferences`: `ultimo` (el número del último
  viaje guardado) y `v0` a `v9` (40 bytes cada uno). El viaje N va en
  `v(N % 10)`: el de ahora se sobrescribe en su lugar y los demás no se
  mueven. Al arrancar se leen de `ultimo` para atrás, 10 como mucho.
- Primero se escribe `ultimo` y después el viaje: si se corta en el medio, en
  ese lugar queda uno con otro número (va adentro de los bytes), que al leer
  se descarta.
- Se guarda al llegar a `MINIMO_VIAJE_GUARDADO_MS` en marcha (60 s) y después
  cada `PERIODO_GUARDAR_VIAJE_MS` (60 s), solo si cambiaron los bytes: con el
  motor parado no se escribe. Estimado, sin medir: ~4 entradas de NVS por
  escritura, una página de 4 KB llena cada ~30 min de manejo.
- Desde `loop()`, sin tarea aparte: escribir la flash frena los dos núcleos
  igual. Cada guardado sale en el monitor serie con lo que tardó.
- Los bytes: versión, número, banderas (si hay fecha y qué extremos hay),
  fecha (año, mes, día, hora y minuto), km (`float`), tiempo en marcha (ms) y
  los 5 extremos. Si cambia el formato, subir `VERSION_RESUMEN`: los
  guardados con otra se descartan en lugar de leerse mal.
- La fecha es la del RTC al arrancar la placa, o al reiniciar el viaje a mano.
  Reiniciarlo (mantener VIAJE) lo guarda como está y lo pasa a ANTERIORES, si
  llegó al minuto; el nuevo lleva otro número.
- El viaje en curso no está en ANTERIORES: la lista se lee al arrancar.

**Las páginas sin datos** (`paginas` y `main`):

- `Paginas::conDatos()` lo llaman `cambiarA()` (con datos solo en LEYENDO) y
  `alCambiarFase()` (Reconectar u Olvidar, que empiezan todavía en LEYENDO).
  Sin datos se recorren la espera, ANTERIORES y DS3. La espera es un valor
  más de `actual` (`ESPERA`, igual a `CANT_PAGINAS`) y la dibuja `main` con
  `Pantalla::espera()`, en `sinDatos()`.
- Al irse los datos, ANTERIORES y DS3 quedan; las demás pasan a la espera, y
  se recuerda la última con datos. Al volver: la que se veía, o si era la
  espera, la última con datos. Como puede ser otra, al entrar a LEYENDO se
  llama `actualizarVisibles()`.
- **Sin datos, los pedidos se atienden en el mismo evento de LVGL**
  (`alPedirSinDatos(atenderPaginas)`): conectar por la dirección guardada
  bloquea `loop()` hasta 4 s sin llamar a `alEsperar()` (sin el adaptador,
  una vez cada ~15 s), y deslizar se habría demorado. Corre con LVGL tomado,
  en `loop()` o en la otra tarea, como mantener la espera para reiniciar. Con
  datos sigue como antes: pedidos que toma `loop()`.
- `textoConexion`: lo que dice la espera en CONECTANDO_BT. Hasta E22 esa
  espera quedaba como estaba y solo la cambiaba `alCambiarFase()`; ahora hace
  falta para volver deslizando a la espera mientras se conecta.
- La espera: tocar reintenta y mantener reinicia **solo con el dedo quieto**
  (`QUIETO_PX`, como mantener VIAJE); si no, cada deslizamiento reintentaba.
  Cambia algo ya verificado: mantenerla 3 s con el dedo corrido más de 20 px
  ya no reinicia.
- La barra tiene 10 puntos (`MAX_PAGINAS`): la hora y la marca SD se corren
  16 px a la izquierda. El título más largo (`Mantener para reiniciar el
  viaje`) sigue entrando.
- VIAJE y ANTERIORES comparten las casillas (`CasillasViaje`,
  `ponerCasillas()`), y VIAJE se dibuja desde un `ResumenViaje`. Debajo de la
  barra, las pantallas de VIAJE del render dan igual píxel a píxel que en
  `main`.
- En ANTERIORES, lo que el viaje no tuvo es `N/D`; en VIAJE sería `...`
  (todavía no llegó).

Lo decidió el agente, sin consultar: guardar desde 1 min en marcha; el título
`ANTERIOR 1/3  Lun 26/10 18:40` (`Sin fecha` sin reloj); que después del más
viejo vuelva al más nuevo y que al entrar a la página empiece por el más
nuevo; que ANTERIORES y DS3 queden al cortarse la conexión; los tres puntos
sin datos; `!viajes`.

Verificado en la PC: los tests nativos (118; 5 de `test_anteriores`), el
render (224 revisiones; las nuevas: ANTERIORES con 1, 3, 10 y ningún viaje,
tocar y deslizar, el título con 10 viajes, el modo sin datos y sus cambios,
deslizar en la espera sin reintentar, con el dedo corrido no reinicia, y los
pedidos desde el evento solo sin datos) y `pio run` sin warnings (59,3 % de
3 MB). **Nada probado en la placa.**

**Pruebas del usuario:**

1. En el banco (USB de la PC, sin el adaptador): deslizar a la izquierda
   desde la espera lleva a ANTERIORES (`Sin viajes guardados`, o los que haya)
   con tres puntos arriba; otra vez, a DS3 andando; otra, a la espera. Que
   cambie enseguida, también mientras dice `Conectando con el adaptador...` o
   `Buscando el adaptador...`, y que deslizar no reintente.
2. Tocar la espera sigue reintentando, y mantenerla 3 s sigue reiniciando.
3. En el auto, un viaje de más de 1 min en marcha. En el monitor,
   `Viajes: guardado el N ...` al minuto y después cada minuto, con lo que
   tarda (si pasa de ~50 ms, anotarlo). `!viajes` dice que el de ahora es el N.
4. Sacar la llave y volver a ponerla: en ANTERIORES (después de VIAJE) está
   ese viaje, con la fecha y los datos que tenía VIAJE, salvo el último minuto.
5. Con dos o más: tocar ANTERIORES pasa al anterior, y deslizar cambia de
   página sin pasar de viaje.
6. Mantener VIAJE 3 s: el viaje que termina aparece enseguida en ANTERIORES.
7. Estando en DS3 o ANTERIORES, cortar la conexión (Reconectar desde INFO, y
   deslizar hasta DS3 mientras conecta): al volver los datos sigue DS3. Desde
   MOTOR, al cortarse va a la espera y al volver, a MOTOR.
8. Si con la llave en accesorios el USB da corriente y el adaptador no, lo
   mismo que en el banco.

Para después:

- Si guardar en la flash tarda mucho o se nota en la pantalla, pasarlo a la
  tarea de la microSD o a una propia.
- El viaje en curso no está en ANTERIORES; si se lo quiere ahí, va primero,
  marcado como en curso.
- Son 10 por `VIAJES_GUARDADOS`; más es cambiar ese número (40 bytes de flash
  y ~60 de RAM por viaje).

### E23

Hecho (rama `e23-sin-cuelgues`, desde `main` con el plan de E23 a E25). La
causa estaba confirmada antes de programar (ver la etapa): con un host USB que
no lee, cada escritura esperaba hasta 2 s, y `setup()` y `loop()` escriben con
LVGL tomado.

Lo que se hizo:

- **Escribir no espera.** En `setup()`, antes de `begin()`:
  `Serial.setTxBufferSize(TAM_SALIDA_USB)` (4 KB; eran 256 bytes) y
  `Serial.setTxTimeoutMs(0)`. Con un host que no lee, lo que no entra se
  pierde; con el monitor abierto, 4 KB alcanzan para `imprimirInfo()` entera.
  Si dos tareas escriben a la vez, la que no consigue el cerrojo del `HWCDC`
  pierde su línea (antes esperaba 100 ms).
- **El arranque no frena la entrada:** `setup()` suelta LVGL alrededor del
  sonido, el IMU, la tarjeta y `elm.iniciar()`, y mientras tanto la atiende la
  tarea de E9. `boton.iniciar()` va antes: `atenderPaginas()` (desde un toque)
  lo lee.
- **El guardián** (`pantalla_hw.cpp`): una tarea `guardian` (prioridad 5, sin
  núcleo fijo, 4 KB de pila) que cada segundo mira cuándo se atendió LVGL por
  última vez y desde cuándo la tarea de envío tiene una imagen. Si algo pasa
  `GUARDIAN_MS` (10 s):
  - anota en la memoria del RTC (`RTC_NOINIT_ATTR`: sobrevive al reinicio, no
    a un corte de luz) qué se trabó, quién tenía LVGL
    (`xSemaphoreGetMutexHolder()`) y el estado de la conexión (lo da `main`
    con `alTrabarse()`);
  - imprime una línea y llama a `abort()`: el pánico guarda el volcado en la
    partición `coredump` y reinicia.

  Lee las marcas de tiempo antes que el reloj: al revés, otra tarea podía
  anotar una más nueva en el medio y la resta sin signo daba enorme.
- **Al arrancar,** `PantallaHw::iniciar()` lee lo anotado si el reinicio fue un
  pánico con su marca (`trabaAnterior()`). `!info` y el registro lo muestran:
  `Ultimo reinicio: guardian (la pantalla no se atendia; LVGL lo tenia
  loopTask, en CONECTANDO_BT)`.
- **Cambio respecto del plan:** iba a mostrar en `!info` la tarea y la función
  del volcado (`esp_core_dump_get_summary()`). Pero el resumen es de la tarea
  que provocó el pánico, que es el guardián: no dice dónde estaba trabado
  `loop()`. Por eso el guardián anota quién tenía LVGL. `!info` igual dice la
  tarea y el `pc` del último volcado (sirve para un pánico de verdad). El
  volcado completo, con la pila de cada tarea, se lee con
  `esptool.py read_flash 0xFF0000 0x10000 coredump.bin` y
  `esp-coredump info_corefile -t raw -c coredump.bin .pio/build/esp32s3/firmware.elf`
  (sin probar).
- **`!escanear` y `!explorar`** bloqueaban hasta 10 y 30 s con LVGL tomado: el
  guardián los cortaría. Ahora lo sueltan, como `conectarBt()`
  (`Consola::despuesDeExplorar()`), y la pantalla sigue andando mientras
  tanto. Salió `PantallaHw::dibujarYa()`, que usaban solo ellos.
- **`!trabar`** en la consola: retiene LVGL sin atenderlo, para probar el
  guardián.
- README: el USB de la radio (se prende al abrir la puerta y se corta unos
  minutos después de sacar la llave), cuándo empieza y termina el viaje, **Si
  se traba**, la consola y `!trabar`.

Lo que podría disparar el guardián sin una traba de verdad, revisado: lo que
espera al ELM (`enviar()`, buscar el adaptador) atiende LVGL con
`alEsperar()`; conectar el BLE lo suelta; la tarjeta monta y mide el espacio
libre en su tarea; `imprimirInfo()` tarda menos de 1 s (un refresco completo).
Si aparece un `guardian` con `loopTask` en una operación nueva que bloquea, esa
operación tiene que soltar LVGL o atenderlo.

Verificado por el agente: `pio run` compila (RAM 11,6 %, flash 59,4 % de
3 MB), sin warnings propios con `-Wall -Wextra -Wshadow -Wvolatile`;
`pio test -e native` 118/118; render, 224 revisiones bien. **Nada probado en
la placa.**

**Pruebas del usuario:**

1. Subir el firmware y desenchufar. Enchufarla a la Mac **sin abrir el
   monitor**: el DS3 entra entero, sin trabarse, y tocar o deslizar responde
   enseguida. Repetirlo 2 o 3 veces.
2. Abrir el monitor y `!info`: `Ultimo reinicio`, `Volcado del ultimo panico:
   ninguno` (o uno de antes) y la RAM interna libre (con 4 KB más del búfer).
3. `!trabar`: la pantalla queda quieta y a los ~10 s la placa se reinicia sola
   (en el monitor, `Guardian: la pantalla no se atiende; LVGL lo tiene
   loopTask, en ...`). Después, `!info`: `Ultimo reinicio: guardian (...)` y
   `Volcado del ultimo panico: tarea guardian`.
4. `!escanear`: mientras busca (10 s), la espera sigue respondiendo y la placa
   no se reinicia.
5. En el auto, unos días sin desenchufarla: al abrir la puerta el DS3 entra
   entero, y con la llave sacada la espera responde. Si alguna vez se reinicia
   sola, mirar `!info` o el `Inicio de la placa` del CSV, y anotarlo acá.

### E24

Hecho (rama `e24-pantalla-principal`, desde `main` con E23 y el arreglo de la
fuerza G). Antes de programar, el usuario eligió (2026-10-28) entre tres
maquetas en un lienzo (https://claude.ai/artifact/54bsJponxNHRKAVdiymbRH,
privado, del usuario; a 640×172, con los colores y la letra del firmware): A
(la barra de las RPM abajo, como antes), B (dos relojes, RPM y refrigerante) y
**C, la elegida**: las RPM como luces de cambio arriba y, abajo, el reloj del
refrigerante, la marcha, las RPM, la velocidad y el voltaje. Cambio respecto
del plan: la maqueta no se hizo con el render sino en el lienzo, como en E4;
la elegida sí está en el render.

Lo que se hizo:

- **`lib/obd/src/marcha.h/.cpp`**, sin Arduino:

  ```cpp
  struct ReglasMarcha {
    const float *relaciones; // km/h por 1000 rpm de cada marcha, desde 1ª
    uint8_t cantidad;
    float tolerancia;        // de 2ª en adelante (0.05 = 5 %)
    float toleranciaPrimera; // 1ª, más: pesan los km/h enteros
    float velocidadMinima;   // km/h; debajo, sin marcha
    uint8_t lecturasIguales; // seguidas, para cambiar la que se muestra
    uint32_t vigenciaMs;
  };
  uint8_t marchaDe(const ReglasMarcha &reglas, float rpm, float kmh); // 0 sin marcha
  class Marcha {
    void lectura(float rpm, float kmh, uint32_t ahora);
    bool actual(uint32_t ahora) const;
    uint8_t numero() const; // la que se muestra; 0 sin marcha
  };
  ```

  Después de un hueco de más de `vigenciaMs` empieza de cero, sin marcha, así
  no muestra la vieja mientras confirma. `test/test_marcha`: 8 tests, con
  filas de `VIAJE0004.CSV` y `VIAJE0007.CSV` de cada marcha y del embrague.
- **`config.h`:** `RELACIONES_MARCHAS` (8,07, 14,57, 20,95, 27,24 y 33,74),
  `TOLERANCIA_MARCHA` (5 %), `TOLERANCIA_PRIMERA` (15 %),
  `VELOCIDAD_MINIMA_MARCHA` (5 km/h) y `LECTURAS_PARA_MARCHA` (2). Elegidas
  con los registros: con el motor andando (10 km/h o más y 1000 rpm o más),
  el 97 % de las lecturas tienen marcha (con 4 %, 95,7; con 6 %, 97,2).
- **`main.cpp`:** la marcha se calcula en `guardarVarias()`, solo con la RPM
  y la velocidad de la misma respuesta (en MOTOR, en cada consulta; en MEZCLA,
  cada ~2,5 s, cuando entra la RPM). De a un PID no se calcula y queda `--`.
  En el CSV, una columna `Marcha` después de `Vel km/h` (de 1 a 5, 0 sin
  marcha, vacía sin un valor actual), y en la tabla del monitor `Marcha N`.
- **MOTOR** (`paginas.cpp`), en el área de 640×144:
  - 14 luces de 500 rpm (y 8–24), apagadas en `FONDO_ESCALA` (las de la zona
    roja, en `ROJO_OSCURO`); prendidas en verde, ámbar desde 4500 y rojo desde
    6000. Debajo, los miles (base 42).
  - El reloj del refrigerante (centro 64, 98; radio 40 al medio del arco, 9 de
    ancho; 270°, de 40 a 130 °C): cuatro `lv_arc` sin toques (si no, el arco
    se mueve con el dedo y se come los deslizamientos), solo con la parte
    principal: la pista, la zona roja (redondeada), la ámbar (encima, para tapar
    la punta de la roja) y el valor. El valor en `NORMAL` al medio, en blanco,
    ámbar desde 110 y rojo desde 115 o con la alarma; `°C` en el hueco de abajo.
    Cambiar los ángulos invalida solo lo que cambió.
  - La marcha en un recuadro de 92×96 (x 124, y 44), con la fuente de 110;
    `-` sin marcha, en `ETIQUETA`; `...`, `--` o `N/D` en Montserrat 48 y gris.
  - RPM (Montserrat 48, x 232, base 100), velocidad (48, x 440, base 88) y
    voltaje (28, x 440, base 126), con la unidad pegada.
  - `DE_MOTOR` suma `VOLT_ELM`, que igual sale de `ATRV` cada
    `PERIODO_ATRV_MS`.
- **Sale AIRE/VOLT:** quedan 9 páginas (y 9 puntos: el primero pasa de x 480 a
  496, y la marca `SD` y la hora se corren 16 px). La admisión se sigue
  leyendo (cada `PERIODO_LENTO_MS`) para el CSV.
- `Paginas::dibujar()` recibe la marcha, después del viaje.
- `lv_conf.h`: `LV_USE_ARC 1`. La flash pasa de 59,4 % a 59,7 % de 3 MB.
- **Render:** la marcha (global, como `reloj`), confirmada con los datos de la
  maqueta: la velocidad pasa de 62 a 58 km/h, para que con 2140 rpm sea 4ª.
  Casos nuevos `a-motor-andando` y `a-motor-caliente` (112 °C), y revisa que
  el refrigerante vaya en blanco, ámbar o rojo según la zona y que dibujar dos
  veces no redibuje. En los extremos, sin marcha (6500 rpm a 188 km/h). La
  alarma doble (`y-alarma-dos`) pasa de AIRE/VOLT a MOTOR. Las constantes de
  la barra (`X_MARCA_SD`, `BORDE_HORA`), para 9 puntos.
- README: las páginas, **La marcha y el reloj**, la columna `Marcha`, la tabla
  del monitor y los tests.

- **MEZCLA:** el usuario confirmó (2026-10-28) que lo que veía era la página
  quieta, con los valores fijos. `lazoRegulando()` en `pids.h` (lazo cerrado:
  `0x02`, o `0x10` con una falla en alguna sonda; con test). Con el lazo
  abierto, lambda y la sonda post van en gris (la marca y la barra también) y
  al lado de sus etiquetas dice `sin regular`, en `CHICA`. `ponerValor()` y
  `ponerMagnitud()` tienen un parámetro `atenuado` para eso. En el render, que
  con el lazo abierto (los extremos, `Abierto carga`) diga `sin regular` y con
  el cerrado no.

Verificado por el agente: `pio run` compila (RAM 11,6 %, flash 59,7 % de
3 MB), sin warnings propios con `-Wall -Wextra -Wshadow -Wvolatile`;
`pio test -e native` 127/127 (9 nuevos); render, 229 revisiones bien. **Nada
probado en la placa.**

**Pruebas del usuario:**

1. En el banco: MOTOR se ve como la maqueta C, con `...` hasta los datos y
   `--` sin ellos. Deslizar sobre el reloj cambia de página (no lo mueve).
2. En el auto, en cada marcha: el número coincide con la palanca; con el
   embrague apretado o en punto muerto, `-`; al cambiar, no muestra una marcha
   equivocada en el medio.
3. Las luces suben con las RPM, ámbar desde 4500 y rojas desde 6000; el reloj
   del refrigerante, en blanco hasta 110 °C.
4. El voltaje en MOTOR, y la alarma de voltaje (en rojo) si corresponde.
5. En el monitor, las consultas por segundo en MOTOR (6–7, como antes) y lo que
   tardan sus cuadros (`dibujo`): las luces y el reloj cambian más seguido que
   la barra de antes.
6. Después de un viaje, la columna `Marcha` del CSV contra lo que se vio.
7. MEZCLA con el motor frío: `sin regular` en gris, y al cerrarse el lazo
   (~30–90 s) los valores en blanco y moviéndose. Soltando el acelerador
   andando (corte de inyección), otra vez `sin regular`.
