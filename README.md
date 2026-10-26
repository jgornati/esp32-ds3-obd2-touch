# Monitor OBDII para Citroën DS3 — pantalla táctil

Monitor de datos del motor con una placa Waveshare
ESP32-S3-Touch-LCD-3.49 que se conecta por BLE a un adaptador ELM327 y
muestra los datos en su pantalla táctil.

Es la segunda versión de `../esp32-ds3-obd2` (ESP32-CAM, ELM327 por
Bluetooth clásico y pantalla Nokia 5110), con la misma lógica OBD.

Solo lee: no borra fallas ni manda nada que cambie la configuración o el
funcionamiento del auto. Nunca muestra datos simulados ni vencidos.

## Qué muestra

Seis páginas, con los datos que da el DS3 VTI 1.6 16v por OBD genérico
(17 PIDs del servicio 01, CAN 11 bit a 500 kbaud):

| Página    | Datos |
| --------- | ----- |
| MOTOR     | RPM en grande, con una barra de 0 a 7000 (zona roja desde 6000); temperatura del refrigerante y velocidad |
| AIRE/VOLT | Temperatura del aire de admisión en grande, con una barra de 0 a 80 °C; voltaje medido por el ELM327 |
| CARGA     | Carga del motor, avance del encendido y acelerador, cada uno con su barra |
| MEZCLA    | Lazo (`Cerrado`, `Abierto frio`, `Abierto carga`...), ajustes de combustible corto y largo, lambda con una barra rica/pobre y la sonda post-catalizador |
| FALLAS    | Luz de falla y códigos guardados, pendientes y permanentes, con su descripción |
| INFO      | Adaptador y señal (RSSI), dirección, versión del ELM y protocolo, PIDs y consultas por segundo; botones Reconectar y Olvidar adaptador |

Cómo se ve cada dato:

| Situación                                  | Se muestra |
| ------------------------------------------ | ---------- |
| Dato válido y actual                       | valor y unidad, en blanco |
| La ECU no soporta el dato                  | `N/D`, en gris |
| Todavía no llegó la primera respuesta      | `...`, en gris |
| Dato de hace más de 3 s                    | `--`, en gris, y la barra vacía |

En MEZCLA, la marca de lambda va en verde entre 0,97 y 1,03; más rica, en
ámbar; más pobre, en cian.

## Uso

### Cambiar de página

- Tocar la mitad izquierda de la pantalla: página anterior. La mitad
  derecha: la siguiente. Cuenta toda la pantalla, también la barra de
  arriba. Después de la última vuelve a la primera.
- El botón BOOT, atrás de la placa: la siguiente.
- Los puntos de la barra muestran en qué página se está.

Las páginas se ven solo con la ECU conectada. La página se mantiene si la
conexión se corta y vuelve.

### Pantallas de estado

| Barra                    | Cuándo |
| ------------------------ | ------ |
| `DS3 OBDII`              | Al arrancar |
| `CONECTANDO` (cian)      | Conectando con el adaptador por la dirección guardada (hasta 4 s) |
| `BUSCANDO ADAPTADOR` (cian) | Buscándolo por nombre (`OBDBLE`) o por servicio (hasta 6 s) |
| `SIN ADAPTADOR` (gris)   | El adaptador no aparece o se perdió. Con la llave sacada es lo normal: el adaptador se prende con el contacto. Reintenta cada 5 s |
| `INICIANDO ELM` (cian)   | Configurando el ELM327. En ámbar, `El ELM no responde`: reintenta a los 5 s |
| `BUSCANDO ECU` (cian)    | Buscando la ECU del auto (hasta 10 s) |
| `SIN ECU` (ámbar)        | El adaptador anda pero la ECU no contesta. Muestra el voltaje y reintenta cada 10 s |
| `ALIMENTACION` (rojo)    | La placa se reinició por falta de corriente (brownout): revisar los 5 V |
| `DESCUBRIENDO`           | Durante `!descubrir` (consola) |

### Fallas

La página FALLAS lee la luz de falla y los códigos al entrar y después
cada 30 s mientras se ve; si algo falla, reintenta a los 5 s.

- `Sin fallas` (verde) solo si los servicios de guardados y pendientes
  contestaron bien sin códigos y la luz no está encendida.
- A la izquierda, la luz (`Encendida` en ámbar) y la cantidad de cada tipo.
  Si un servicio no contestó, va en ámbar (`Pendientes: error`) y nunca se
  ve `Sin fallas`.
- A la derecha, los códigos con su descripción. Los pendientes van en gris
  con `(pendiente)`. Si hay más de 4, se alternan de a 4 cada 3 s y el
  título dice `FALLAS 1/2`.
- No hay ninguna función para borrar fallas.

### Info

- **Reconectar:** corta la conexión con el adaptador y vuelve a conectar.
- **Olvidar adaptador:** hay que mantenerlo apretado 3 s (se llena de rojo
  mientras tanto); soltarlo antes o correr el dedo lo cancela. Borra la
  dirección guardada y busca el adaptador de cero.

Ninguno de los dos le manda nada al auto.

### Consola

Por el USB-C, a 115200 baudios. Una línea sin `!` se le manda al ELM,
siempre que esté en la lista blanca de comandos de solo lectura (`04`, que
borra las fallas, da `RECHAZADO`).

| Comando       | Qué hace |
| ------------- | -------- |
| `!info`       | Chip, memoria, último reinicio, pantalla, adaptador y ECU |
| `!reconectar` | Como el botón Reconectar |
| `!olvidar`    | Como Olvidar adaptador, sin mantener apretado |
| `!log`        | Prende o apaga el detalle de cada consulta |
| `!descubrir`  | Relevamiento de la ECU (PIDs, tiempos), con la ECU conectada |
| `!escanear`   | Busca dispositivos BLE durante 10 s |
| `!explorar N` | Muestra los servicios del dispositivo N de la búsqueda |

Cada segundo, con la ECU conectada, imprime una línea con todos los datos
y las consultas por segundo (~6).

## Hardware

- Waveshare ESP32-S3-Touch-LCD-3.49: ESP32-S3R8 (8 MB de PSRAM, 16 MB de
  flash), pantalla IPS de 172×640 con touch capacitivo (AXS15231B). Se usa
  en horizontal, a 640×172.
- Adaptador OBD BLE: anuncia `OBDBLE` con el
  servicio `FFF0` y responde como `ELM327 v2.1`. Detalle en **Datos
  descubiertos** de `PLAN.md`.
- Sin batería.

La retroiluminación queda siempre al máximo: en esta placa no responde al
pin de brillo (GPIO8), ni con el código de Waveshare. Ver las notas de E6
en `PLAN.md`.

## Instalación en el auto

- **Placa:** 5 V desde un USB del auto que se corta con la llave
  (conviene que dé 1 A o más). La placa se apaga al sacar la llave.
- **Adaptador:** alimentado desde el pin 1 del conector OBD, que en el DS3
  se corta con la llave, en lugar del 16 (permanente): se le sacó la pata
  16 y se puenteó la 1 al pad. Así no queda consumiendo con el auto
  apagado.
- **Montaje:** lejos del sol directo (el LCD se oscurece con calor). La
  imagen se ve derecha con `PANTALLA_GIRADA = false` en
  `include/config.h`; si se monta al revés, `true` la gira 180° junto con
  el touch.
- Al poner el contacto, la placa y el adaptador arrancan juntos: lleva
  unos segundos llegar a los datos.

## Entorno de desarrollo

- Visual Studio Code con PlatformIO.
- `espressif32 @ 8.0.0` (Arduino-ESP32 3.3.x), placa
  `esp32-s3-devkitc-1` con la configuración de `platformio.ini`; LVGL
  9.3.0.
- Subir el firmware por el USB-C. Si no entra en modo de descarga:
  mantener BOOT, apretar RESET y soltar BOOT.
- Monitor serie: 115200 baudios, por el USB-C. Abrir el puerto reinicia
  la placa.

Desde la carpeta del proyecto (`pio` no está en el PATH):

```text
~/.platformio/penv/bin/pio run              # compilar
~/.platformio/penv/bin/pio run -t upload    # compilar y subir
```

## Tests

La lógica que no depende del hardware (`lib/obd`: lista blanca, respuestas
del ELM, PIDs y fallas) tiene tests que corren en la PC, con respuestas
reales del DS3 y del adaptador:

```text
~/.platformio/penv/bin/pio test -e native
```

Las pantallas también se pueden ver en la PC, con el mismo LVGL y el
mismo código de dibujo (hace falta un `pio run` antes, clang y zlib):

```text
make -C herramientas/render -j8
```

Deja un PNG por pantalla en `herramientas/render/salida/` y revisa que
dibujar dos veces lo mismo no redibuje nada, que los textos no se pasen de
su lugar y que los toques hagan lo que corresponde.

## Documentación

- `PLAN.md`: decisiones, arquitectura, etapas, datos medidos en el auto y
  notas de cada etapa.
- `docs/descubrimiento/`: los logs de las sesiones en el auto.
