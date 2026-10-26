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
| E0    | Base del proyecto y exploración BLE    | —          | Verificada (salvo la prueba con el iPhone) |
| E1    | Pantalla y touch                       | E0         | Verificada |
| E2    | Transporte BLE con el ELM              | E0         | Verificada |
| E3    | El monitor completo en el auto         | E1, E2     | Verificada |
| E4    | Diseño de páginas y navegación táctil  | E3         | Verificada (salvo de noche) |
| E5    | Códigos de falla                       | E4         | Verificada (salvo comparar con el iPhone) |
| E6    | Brillo y ajustes                       | E4         | Hecha (falta verificar en hardware) |
| E7    | Cierre e instalación                   | E5, E6     | Hecha (falta verificar en hardware) |

Estados posibles: Pendiente, En curso, Hecha (falta verificar en
hardware), Verificada.

E1 y E2 tocan archivos distintos y se pueden hacer en paralelo. E5 y E6
también.

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
| Bluetooth              | **Solo BLE.** El ESP32-S3 no tiene Bluetooth clásico, así que el ELM anterior (`OBDII`, SPP) no sirve con esta placa. Se usa el adaptador nuevo, el que anda con iPhone. |
| Plataforma             | `espressif32 @ 8.0.0` (Arduino-ESP32 3.3.x), la misma del anterior. Waveshare pide Arduino-ESP32 3.1.0 o más nuevo. |
| Placa en PlatformIO    | No hay definición para la 3.49. Se usa `esp32-s3-devkitc-1` con lo que Waveshare indica para el Arduino IDE: flash 16 MB QIO 80 MHz, PSRAM OPI (`qio_opi`), USB CDC al arrancar, particiones `app3M_fat9M_16MB.csv`. Un programa de prueba con BLE, `esp_lcd` e `i2c_master` compiló así (2026-10-24); falta subirlo a la placa. |
| Subir el firmware      | Por el USB-C, sin adaptador USB–TTL ni puente GPIO0–GND. Si no entra en modo de descarga: mantener BOOT, apretar RESET, soltar BOOT. |
| Biblioteca BLE         | *Propuesta:* la `BLE` del core (viene con la plataforma, sin dependencia nueva). Si da problemas, NimBLE-Arduino con versión fijada. E0 ya la usa: en el S3 corre sobre NimBLE, no Bluedroid (ver notas de E0). |
| Gráficos               | *Propuesta:* LVGL 9.3.0 (la versión de los ejemplos de Waveshare) y el driver `esp_lcd_axs15231b` del ejemplo `10_LVGL_V9_Test`. |
| Orientación            | Horizontal, 640×172, con rotación por software como en el ejemplo de Waveshare. Montada como en el banco: `PANTALLA_GIRADA = false` en `config.h` (confirmado en E7). |
| Concurrencia           | *Propuesta:* la lógica sigue en un solo `loop()`. Las notificaciones BLE llegan desde la tarea del stack y se pasan a un buffer (único punto concurrente). LVGL no tiene tarea propia: `lv_timer_handler()` se llama desde `loop()` y mientras se espera una respuesta del ELM. |
| Protocolo              | `ATSP6` (CAN 11 bit 500 kbaud), confirmado en el DS3. Heredada.                                |
| Sufijo de cantidad     | `01PP1`. Heredada; se reconfirma con el adaptador nuevo en E3.                                 |
| Fallas                 | Solo OBD genérico (motor y emisiones). Heredada.                                               |
| Alimentación           | 5 V desde un USB del auto que se corta con la llave. Sin deep sleep y sin batería en la placa (confirmado en E7). |
| Alimentación del adaptador | Con el contacto, desde el pin 1 del conector OBD (en el DS3 se corta con la llave), no desde el 16 (permanente). Lo modificó el usuario el 2026-10-26: le sacó la pata 16 y puenteó la 1 al pad. Con la llave sacada el adaptador se apaga y la placa queda en `SIN ADAPTADOR` (hasta E5, `SIN CONEXION` en rojo), no en `SIN ECU`. |
| Brillo                 | Siempre al máximo, sin regulación (2026-10-26, E6): en esta placa la luz no responde al GPIO8. |
| Hardware sin usar      | No inicializar microSD, audio, IMU, RTC ni el expansor TCA9554 hasta que una etapa lo pida.    |

## Hardware de la placa

Según los ejemplos de Waveshare (`waveshareteam/ESP32-S3-Touch-LCD-3.49`,
`Examples/Arduino/10_LVGL_V9_Test/user_config.h`). Se verifica en E0 y E1.

| Parte                                  | Detalle                                                    |
| -------------------------------------- | ---------------------------------------------------------- |
| Chip                                   | ESP32-S3R8: 2 núcleos, 240 MHz, 8 MB de PSRAM (OPI), 16 MB de flash, Wi-Fi y BLE 5 |
| Pantalla                               | IPS 3,49", 172×640, controlador AXS15231B por QSPI (`SPI3_HOST`) |
| Pines de la pantalla                   | CS 9, CLK 10, D0–D3 11–14, RST 21                          |
| Retroiluminación                       | GPIO8 (`LCD_BL`) al regulador AP3032; activa en bajo, como en los ejemplos (`lcd_bl_pwm_bsp.h`). En esta placa no cambia la luz (E6): queda en bajo, al máximo. El encendido del regulador es EXIO1 del TCA9554 (`BL_EN`) |
| Touch                                  | Capacitivo, en el mismo AXS15231B. I2C en SDA 17 / SCL 18, dirección `0x3B`, sin INT ni RST |
| I2C de la placa                        | SDA 47 / SCL 48: RTC PCF85063 (`0x51`), IMU QMI8658 (`0x6B`), expansor TCA9554 (`0x20`) |
| TCA9554, pin 6                         | Mantiene la alimentación desde la batería (ejemplo `07_BATT_PWR_Test`) |
| GPIO16                                 | El ejemplo 07 lo lee para saber si está prendida por batería. No usar. |
| Botones                                | BOOT (GPIO0), PWR y RESET, atrás                           |
| Otros                                  | Audio ES8311/ES7210 con parlante, microSD, conector de batería 3,7 V, 22 pines de expansión |

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
  pantalla_hw.h/.cpp  QSPI, rotación, touch, brillo y puerto de LVGL (E1)
  pantalla.h/.cpp   pantallas de estado (E3) y el marco de las páginas (E4)
  paginas.h/.cpp    páginas y navegación táctil (E4)
  dibujo.h/.cpp     colores y funciones chicas de LVGL de pantalla y paginas (E4)
  boton.h/.cpp      botón BOOT: página siguiente (E4)
  datos, sondeo, descubrimiento   portados del anterior (E3)
lib/obd/src/        igual que el anterior: lógica pura, SIN Arduino.h
lib/axs15231b/      driver del panel, del ejemplo de Waveshare (E1)
src/fuentes/         números grandes generados con lv_font_conv (E1)
test/               igual que el anterior, más lo nuevo de lib/obd
herramientas/render/  las pantallas en la PC, a PNG (E4)
docs/descubrimiento/  logs crudos de las sesiones en el auto
```

Reglas de límites entre módulos (las mismas del anterior):

- `elm327` **no interpreta** el contenido OBD. Devuelve el texto crudo
  recibido antes del `>` y un estado de transporte.
- `lib/obd` **no depende de Arduino** y se testea con
  `pio test -e native`.
- Todo comando pasa por `comandoPermitido()` dentro de
  `Elm327::enviar()`. No existe otro camino para escribirle al ELM.
- Lo de LVGL queda en `pantalla_hw`, `pantalla` y `paginas`. La máquina
  de estados no llama a LVGL directamente.

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
- Con el iPhone conectado al adaptador (la app abierta), ¿la ESP32 lo
  ve y se puede conectar? Lo esperable es que no.

**Traspaso:** completar en **Datos descubiertos** nombre, dirección,
servicio, características, MTU, tamaño de las notificaciones, si pidió
emparejamiento, tiempos por comando y qué pasa con el iPhone conectado.
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
relevamiento. Si se puede, comparar con la app del iPhone (de a uno: el
adaptador acepta una sola conexión).

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

## Ideas para después

Fuera del plan; solo si el usuario las pide. Lo que sigue usa hardware
que la placa ya trae:

- Alarmas en color (y con el parlante) para refrigerante alto o voltaje
  de carga bajo.
- Máximos del viaje (RPM, temperatura).
- Registro del viaje en la microSD.
- Reloj con el RTC.
- Fuerza G con el acelerómetro.

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
| Con el iPhone conectado       | Sin probar | E0    |
| Brillo cómodo de día / noche  | No se pudo medir: con esta placa la luz no se regula (fila siguiente). Queda siempre al máximo | E1, E6 |
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

Sin probar: la prueba con el iPhone conectado al adaptador. Se puede
hacer en E2, que es donde importa (reconexión).

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

Sin anotar: la prueba con el iPhone conectado al adaptador (de E0) y
cuánto tarda cada intento con el adaptador apagado (tendría que ser ~10 s:
4 s por dirección + 6 s buscando).

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
Falta verlo de noche. En Fallas e Info el sondeo quedaba espaciado; lo
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

Sin probar con el cambio: comparar con la app del iPhone (de a uno: el
adaptador acepta una sola conexión). Con el adaptador alimentado por el
contacto, sacar la llave lleva a `SIN CONEXION`; `SIN ECU` queda para el
contacto puesto sin respuesta de la ECU.

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
