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
| E3    | El monitor completo en el auto         | E1, E2     | Pendiente |
| E4    | Diseño de páginas y navegación táctil  | E3         | Pendiente |
| E5    | Códigos de falla                       | E4         | Pendiente |
| E6    | Brillo y ajustes                       | E4         | Pendiente |
| E7    | Cierre e instalación                   | E5, E6     | Pendiente |

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
| Alcance                | Solo lectura, como el anterior. Todo comando pasa por `comandoPermitido()`; la lista blanca no se amplía sin pedido del usuario. |
| Bluetooth              | **Solo BLE.** El ESP32-S3 no tiene Bluetooth clásico, así que el ELM anterior (`OBDII`, SPP) no sirve con esta placa. Se usa el adaptador nuevo, el que anda con iPhone. |
| Plataforma             | `espressif32 @ 8.0.0` (Arduino-ESP32 3.3.x), la misma del anterior. Waveshare pide Arduino-ESP32 3.1.0 o más nuevo. |
| Placa en PlatformIO    | No hay definición para la 3.49. Se usa `esp32-s3-devkitc-1` con lo que Waveshare indica para el Arduino IDE: flash 16 MB QIO 80 MHz, PSRAM OPI (`qio_opi`), USB CDC al arrancar, particiones `app3M_fat9M_16MB.csv`. Un programa de prueba con BLE, `esp_lcd` e `i2c_master` compiló así (2026-10-24); falta subirlo a la placa. |
| Subir el firmware      | Por el USB-C, sin adaptador USB–TTL ni puente GPIO0–GND. Si no entra en modo de descarga: mantener BOOT, apretar RESET, soltar BOOT. |
| Biblioteca BLE         | *Propuesta:* la `BLE` del core (viene con la plataforma, sin dependencia nueva). Si da problemas, NimBLE-Arduino con versión fijada. E0 ya la usa: en el S3 corre sobre NimBLE, no Bluedroid (ver notas de E0). |
| Gráficos               | *Propuesta:* LVGL 9.3.0 (la versión de los ejemplos de Waveshare) y el driver `esp_lcd_axs15231b` del ejemplo `10_LVGL_V9_Test`. |
| Orientación            | *Propuesta:* horizontal, 640×172, con rotación por software como en el ejemplo de Waveshare. El sentido (USB-C a la izquierda o a la derecha) queda en `config.h`. |
| Concurrencia           | *Propuesta:* la lógica sigue en un solo `loop()`. Las notificaciones BLE llegan desde la tarea del stack y se pasan a un buffer (único punto concurrente). LVGL no tiene tarea propia: `lv_timer_handler()` se llama desde `loop()` y mientras se espera una respuesta del ELM. |
| Protocolo              | `ATSP6` (CAN 11 bit 500 kbaud), confirmado en el DS3. Heredada.                                |
| Sufijo de cantidad     | `01PP1`. Heredada; se reconfirma con el adaptador nuevo en E3.                                 |
| Fallas                 | Solo OBD genérico (motor y emisiones). Heredada.                                               |
| Alimentación           | 5 V desde un USB del auto que se corta al apagarlo. Sin deep sleep. *Propuesta:* sin batería en la placa (ver E7). |
| Hardware sin usar      | No inicializar microSD, audio, IMU, RTC ni el expansor TCA9554 hasta que una etapa lo pida.    |

## Hardware de la placa

Según los ejemplos de Waveshare (`waveshareteam/ESP32-S3-Touch-LCD-3.49`,
`Examples/Arduino/10_LVGL_V9_Test/user_config.h`). Se verifica en E0 y E1.

| Parte                                  | Detalle                                                    |
| -------------------------------------- | ---------------------------------------------------------- |
| Chip                                   | ESP32-S3R8: 2 núcleos, 240 MHz, 8 MB de PSRAM (OPI), 16 MB de flash, Wi-Fi y BLE 5 |
| Pantalla                               | IPS 3,49", 172×640, controlador AXS15231B por QSPI (`SPI3_HOST`) |
| Pines de la pantalla                   | CS 9, CLK 10, D0–D3 11–14, RST 21                          |
| Retroiluminación                       | GPIO8 con PWM (LEDC), activa en bajo: duty = 255 − brillo, como en los ejemplos (`lcd_bl_pwm_bsp.h`). Confirmado en E1 |
| Touch                                  | Capacitivo, en el mismo AXS15231B. I2C en SDA 17 / SCL 18, dirección `0x3B`, sin INT ni RST |
| I2C de la placa                        | SDA 47 / SCL 48: RTC PCF85063 (`0x51`), IMU QMI8658 (`0x6B`), expansor TCA9554 (`0x20`) |
| TCA9554, pin 6                         | Mantiene la alimentación desde la batería (ejemplo `07_BATT_PWR_Test`) |
| GPIO16                                 | El ejemplo 07 lo lee para saber si está prendida por batería. No usar. |
| Botones                                | BOOT (GPIO0), PWR y RESET, atrás                           |
| Otros                                  | Audio ES8311/ES7210 con parlante, microSD, conector de batería 3,7 V, 22 pines de expansión |

## Arquitectura

```
include/
  config.h          pines, BLE, tiempos, brillo, rotación
  lv_conf.h         configuración de LVGL (E1)
src/
  main.cpp          exploración (E0); máquina de estados (E3)
  explorar.h/.cpp   !escanear, !explorar (E0; !usar salió en E2)
  elm327.h/.cpp     transporte BLE con la interfaz del anterior (E2)
  consola.h/.cpp    consola por el USB (E0, E2)
  pantalla_hw.h/.cpp  QSPI, rotación, touch, brillo y puerto de LVGL (E1)
  pantalla.h/.cpp   pantallas de estado (E3)
  paginas.h/.cpp    páginas y navegación táctil (E4)
  datos, sondeo, descubrimiento   portados del anterior (E3)
lib/obd/src/        igual que el anterior: lógica pura, SIN Arduino.h
lib/axs15231b/      driver del panel, del ejemplo de Waveshare (E1)
src/fuentes/         números grandes generados con lv_font_conv (E1)
test/               igual que el anterior, más lo nuevo de lib/obd
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
| Bluetooth perdido                         | barra: `SIN CONEXION`, todos los datos `--` |

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
| Brillo cómodo de día / noche  |       | E1    |
| Refresco de pantalla          | Completo (dibujar todo y mandarlo): 96–131 ms. Solo el envío al panel: 32–33 ms, se cambie lo que se cambie | E1    |
| Memoria con pantalla y BLE    | RAM interna libre 164 KB (bloque mayor 123 KB); PSRAM libre 7972 KB | E1    |
| Conexión guardada / buscando  | Por dirección guardada: conecta en 389–1348 ms y queda listo en 974–1928 ms. Buscando: lo encuentra en 23–1526 ms y queda listo en 1536–1923 ms. Casi no hay diferencia, porque la búsqueda lo encuentra enseguida. De lo que tarda en quedar listo, ~530 ms son servicios, características y suscripción | E2    |
| Parámetros de conexión        | Con 7,5–15 ms pedidos queda en 15 ms (latencia 0, supervisión 2 s). El adaptador pide 25–35 ms dos veces por conexión; se rechaza (ver notas de E2) | E2    |
| Tiempo por comando AT         | Con 15 ms: `ATZ` 29–89, `ATE0` 59–89, `ATI` 35–90, `ATRV` 64–100 ms. Con 50 ms (E0): 70–390 ms. Con SPP era ~40 ms | E2    |
| ¿Acepta `010C1`? ms con y sin |       | E3    |
| Consultas por segundo         |       | E3    |

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

### E4

### E5

### E6

### E7
