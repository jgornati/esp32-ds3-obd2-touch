# Monitor OBDII para Citroën DS3 — pantalla táctil

Monitor de datos del motor con una placa Waveshare
ESP32-S3-Touch-LCD-3.49 que se conecta por BLE a un adaptador ELM327 y
muestra los datos en su pantalla táctil.

Es la segunda versión de `../esp32-ds3-obd2` (ESP32-CAM, ELM327 por
Bluetooth clásico y pantalla Nokia 5110), con la misma lógica OBD.

Solo lee: no borra fallas ni manda nada que cambie la configuración o el
funcionamiento del auto. Nunca muestra datos simulados ni vencidos.

Con una microSD puesta, graba cada viaje en un CSV para verlo en la PC.

## Qué muestra

Seis páginas con los datos que da el DS3 VTI 1.6 16v por OBD genérico
(17 PIDs del servicio 01, CAN 11 bit a 500 kbaud), una con el viaje en curso,
otra con la fuerza G y al final una animación:

| Página    | Datos |
| --------- | ----- |
| MOTOR     | RPM en grande, con una barra de 0 a 7000 (zona roja desde 6000); temperatura del refrigerante y velocidad |
| AIRE/VOLT | Temperatura del aire de admisión en grande, con una barra de 0 a 80 °C; voltaje medido por el ELM327 |
| CARGA     | Carga del motor, avance del encendido y acelerador, cada uno con su barra |
| MEZCLA    | Lazo (`Cerrado`, `Abierto frio`, `Abierto carga`...), ajustes de combustible corto y largo, lambda con una barra rica/pobre y la sonda post-catalizador |
| FALLAS    | Luz de falla y códigos guardados, pendientes y permanentes, con su descripción |
| INFO      | Adaptador y señal (RSSI), la microSD, versión del ELM y protocolo, PIDs y consultas por segundo; brillo; botones Reconectar y Olvidar adaptador |
| VIAJE     | Distancia, tiempo con el motor en marcha y velocidad promedio; velocidad, RPM y refrigerante máximos, y voltaje mínimo y máximo con el motor en marcha |
| FUERZA G  | Un punto en un círculo de 1 g con la aceleración del auto; la frenada o aceleración y la lateral de ahora, y los máximos del viaje para cada lado |
| DS3       | El DS3 andando, con nubes y el camino corriéndose. Ocupa toda la pantalla, sin la barra ni datos |

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

- Deslizar el dedo hacia la izquierda: la página siguiente. Hacia la
  derecha: la anterior. Alcanza con correrlo un dedo y medio (80 px), más de
  costado que en vertical. Cuenta toda la pantalla, también la barra de
  arriba y los botones de Info, que así no se aprietan. Después de la última
  vuelve a la primera.
- Tocar no cambia de página: un bache o un roce no la cambian.
- El botón BOOT, atrás de la placa: la siguiente.
- Los puntos de la barra muestran en qué página se está.

Las páginas se ven solo con la ECU conectada. La página se mantiene si la
conexión se corta y vuelve. En DS3 no se ve la barra: se cambia de página
igual, deslizando.

### Pantalla de espera

Mientras no hay datos, la pantalla es negra, con un DS3 rojo y una línea
abajo que dice en qué anda la conexión. El auto entra desde la izquierda
apenas se prende la pantalla y se queda; si después se cortan los datos,
aparece quieto.

| Abajo a la izquierda            | Cuándo |
| ------------------------------- | ------ |
| `Iniciando...`                  | Al arrancar |
| `Conectando con el adaptador...` | Por la dirección guardada (hasta 4 s) |
| `Buscando el adaptador...`      | Por nombre (`OBDBLE`) o por servicio (hasta 6 s) |
| `Esperando el adaptador`        | No aparece o se perdió. Con la llave sacada es lo normal: el adaptador se prende con el contacto. Reintenta cada 5 s |
| `Iniciando el adaptador...`     | Configurando el ELM327. En ámbar, `El adaptador no responde`: reintenta a los 5 s |
| `Buscando la ECU...`            | Hasta 10 s |
| `Esperando la ECU`              | El adaptador anda pero la ECU no contesta. Reintenta cada 10 s |
| `Sin respuesta de la ECU` (ámbar) | La ECU dejó de contestar con el motor andando |

- **Tocar** la pantalla reintenta ya, sin esperar los 5 o 10 s. Con el auto
  quieto, abajo a la derecha dice `Toca para reintentar`.
- **Mantenerla apretada 3 s** reinicia la placa, por si quedó trabada: abajo
  crece una barra roja y dice `Mantener para reiniciar`. Soltarla antes
  reintenta. Anda también mientras la placa conecta con el adaptador.

Ninguno de los dos le manda nada al auto. Quedan dos pantallas de estado,
con la barra arriba:

| Barra                    | Cuándo |
| ------------------------ | ------ |
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

### Viaje

El viaje empieza al poner el contacto (la placa arranca con la llave) y se
pierde al sacarla: no se guarda.

- **Distancia:** sale de la velocidad que da la ECU, leída en cada consulta
  (~6 por segundo). Si pasan más de 3 s sin velocidad (la ECU no contestó),
  ese rato no suma.
- **Tiempo:** solo con el motor en marcha, desde 500 rpm; con el motor parado
  y el contacto puesto no cuenta. El promedio es la distancia sobre ese
  tiempo, con los semáforos.
- **Máximos:** la velocidad, la RPM y el refrigerante más altos del viaje. La
  RPM se lee en cada consulta salvo en MEZCLA, donde va cada ~2,5 s: ahí se
  puede perder un pico.
- **Voltaje mínimo y máximo:** el del ELM, solo con el motor en marcha y
  desde los 5 s de arrancar (sin la caída del arranque).
- Hasta la primera lectura se ve `...`; con el voltaje, hasta 5 s después de
  arrancar. Si la ECU no da el dato del que sale, `N/D`.
- **Mantener apretada la página 3 s** reinicia el viaje: arriba la barra se
  llena de rojo y dice `Mantener para reiniciar el viaje`, con el dedo quieto:
  si se corre, no cuenta. Soltarla antes no hace nada. Reiniciar la placa
  desde la pantalla de espera también lo reinicia.

### Fuerza G

La mide el acelerómetro de la placa (el IMU QMI8658) 50 veces por segundo,
aparte de las consultas a la ECU.

- **El punto** se mueve como una bolita, hacia donde empuja el auto: al frenar
  sube, al acelerar baja y en una curva a la derecha va a la izquierda. El
  círculo de afuera es 1 g y el de adentro, 0,5 g; lo que pasa de 1 g queda en
  el borde.
- **Los valores de ahora:** arriba la frenada o la aceleración (`Frenada`,
  `Acel.`) y abajo la curva (`Curva der`, `Curva izq`). En 0.00 dice `Long.` y
  `Lateral`.
- **Los máximos** de frenada, aceleración y curva a cada lado son del viaje: se
  reinician con él, al poner el contacto y manteniendo apretada VIAJE.
- **Las vibraciones** se filtran: lo que dura menos de ~0,3 s casi no se ve.
- **La calibración:** la placa mide dónde es abajo con la gravedad, quieta 2 s.
  Lo hace al poner el contacto (mientras tanto dice `Calibrando` y `...`) y en
  cada parada, con la ECU en 0 km/h. Si se estacionó en una pendiente, se
  corrige en la primera parada. Manejando en una subida, la pendiente se ve
  como aceleración: el sensor no las distingue.
- Adelante sale de la gravedad y del lado largo de la pantalla: la placa tiene
  que ir de frente, no girada hacia un costado (la inclinación no importa).
- Si el sensor deja de contestar: `--` y sin el punto. Si no contestó al
  arrancar: `N/D` y `Sin sensor`.

### Registro en la microSD

Cada viaje queda en un archivo de la microSD, para verlo después en la PC.
La tarjeta tiene que estar en FAT32 (las de hasta 32 GB vienen así; las más
grandes vienen en exFAT y hay que formatearlas) y puesta antes de prender:
no se detecta después.

- **Un archivo por encendido:** `VIAJE0001.CSV`, `VIAJE0002.CSV`..., en la
  raíz. Se abre con la primera lectura de la ECU, así que prender la placa
  sin el auto no deja archivos. El número es el siguiente al más alto de la
  tarjeta: nunca pisa uno. Con el reloj en hora, el archivo tiene la fecha
  de su creación (si no, 1980-01-01).
- **Una línea por segundo,** con lo mismo que la tabla del monitor. Es un CSV
  estándar: coma entre columnas y punto decimal. Google Sheets lo abre
  directo; en Excel en español, con Datos > Desde texto/CSV.
- **Se graba cada 5 s.** Al sacar la llave la placa se apaga y se pierden
  esos últimos segundos, ya con el motor parado. El archivo queda hasta el
  último bloque grabado.
- En la barra de todas las páginas, a la izquierda de los puntos, una marca
  `SD`: gris mientras graba y ámbar si hay tarjeta pero no puede grabar. Sin
  tarjeta no aparece.
- En INFO, el renglón `Tarjeta`: `Grabando VIAJE0012`; `Sin tarjeta`, en
  gris; en ámbar, `No se puede leer` (no es FAT32 o está dañada), `Error al
  escribir` (reintenta cada 5 s) o `Tarjeta llena`. `!sd` da el detalle.

| Columna | Qué tiene |
| ------- | --------- |
| `Seg` | Segundos desde la primera lectura de la ECU. Salta si la placa estuvo trabada conectando con el adaptador (hasta 30 s) |
| `Hora` | La del reloj: `2026-10-27 14:35:12`. Vacía si el reloj no tiene la hora |
| `RPM` a `Acelerador %` | Los 14 datos, con la unidad en el nombre (`Refrig C`, `Vel km/h`); `Lazo`, en texto. Vacío sin un valor actual: la ECU no lo da, no contestó en los últimos 3 s o no hay conexión |
| `Consultas/s` | Las consultas a la ECU del último segundo |
| `Distancia km`, `En marcha s` | Las del viaje, como en VIAJE: la última línea tiene el total |
| `G lateral`, `G long.` | La fuerza G en ese momento: la lateral, positiva en una curva a la derecha; la longitudinal, positiva acelerando y negativa frenando. Vacías sin calibrar o sin sensor |
| `Pagina` | La página a la vista (`MOTOR`, `FUERZA G`...). Vacía sin datos, con la pantalla de espera |
| `Cuadros/s` a `Envio max ms` | Lo que tardan los cuadros de la pantalla en el último segundo, como en la tabla del monitor: cuántos se mandaron y el promedio y el máximo de cada tiempo, en ms (ver **Consola**) |
| `Eventos` | Lo que pasó en ese segundo, separado por ` \| `: el inicio de la placa y por qué (`encendido`, `brownout`...), los cambios de la conexión (`LEYENDO`, `SIN_ECU (No responde)`...), las alarmas, las fallas leídas en FALLAS cuando cambian, `Viaje reiniciado`, los errores de la tarjeta y `Reloj puesto en hora` (la columna `Hora` salta) |

Los máximos del viaje salen en la planilla (`MAX()` de la columna).

### Reloj

La hora va en la barra de todas las páginas, antes de la marca `SD`, y arriba
a la derecha de la pantalla de espera. La da el reloj de la placa (el RTC
PCF85063), que sigue andando con la placa apagada gracias a la batería.

- Es la hora local, en 24 h. Se pone una vez desde la consola, con
  `!hora 2026-10-27 14:35` (o con los segundos, `14:35:12`), y queda: no hay
  que volver a ponerla al prender.
- Atrasa o adelanta del orden de un minuto por mes: se corrige con `!hora`.
  Con horario de verano, también se cambia con `!hora`.
- Si el reloj perdió la hora (se quedó sin batería) o no contesta, dice
  `--:--`, en gris: nunca una hora inventada.

### Alarmas

En cualquier página, la barra de arriba se pone roja con un triángulo, la
alarma y el valor (`REFRIGERANTE 106 °C`), y el valor va en rojo en su
página. Al aparecer cada una suenan tres pitidos, sin repetir. Deslizar sigue
cambiando de página: no hay nada que aceptar, y la alarma se va sola.

| Alarma                | Se prende                                   | Se apaga |
| --------------------- | ------------------------------------------- | -------- |
| `REFRIGERANTE`        | 105 °C o más, 5 s seguidos                  | 102 °C o menos, 5 s |
| `VOLTAJE BAJO` / `ALTO` | Con el motor en marcha (desde 5 s después de arrancar): 13,0 V o menos, o 15,0 V o más, 10 s seguidos (falla de carga) | 13,2 V o más / 14,8 V o menos, 10 s |
| `ZONA ROJA`           | 6000 rpm o más, al instante                 | 5800 rpm o menos |

- Con varias a la vez, se alternan cada 3 s y la barra dice `1/2`.
- Sin un valor actual (la ECU dejó de contestar, el motor se paró para el
  voltaje) la alarma se apaga: nunca muestra un valor viejo.
- En MEZCLA la RPM se lee cada ~2,5 s, así que la zona roja puede tardar.
- Los umbrales están en `include/config.h`. Con `ALARMAS_DE_PRUEBA = true`
  saltan manejando normal (40 °C, 2000 rpm y los voltajes de marcha), para
  probarlas.

### Info

- **Reconectar:** corta la conexión con el adaptador y vuelve a conectar.
- **Olvidar adaptador:** hay que mantenerlo apretado 3 s (se llena de rojo
  mientras tanto); soltarlo antes o correr el dedo lo cancela. Borra la
  dirección guardada y busca el adaptador de cero.
- **Brillo:** `-` y `+` lo cambian en 5 niveles (los segmentos del medio). El
  más bajo se sigue leyendo de noche: por debajo, la luz se apaga del todo.
  Queda guardado para el próximo arranque; sin nada guardado, al máximo.
- **Tarjeta:** la microSD (ver **Registro en la microSD**). Hasta E13 ese
  renglón era la dirección del adaptador, que ahora sale en `!info`.

Ninguno le manda nada al auto.

### Consola

Por el USB-C, a 115200 baudios. Una línea sin `!` se le manda al ELM,
siempre que esté en la lista blanca de comandos de solo lectura (`04`, que
borra las fallas, da `RECHAZADO`).

| Comando       | Qué hace |
| ------------- | -------- |
| `!info`       | Chip, memoria, último reinicio, pantalla (los cuadros del último segundo y un refresco completo), adaptador, ECU, tarjeta y reloj |
| `!reconectar` | Como el botón Reconectar |
| `!olvidar`    | Como Olvidar adaptador, sin mantener apretado |
| `!log`        | Prende o apaga el detalle de cada consulta |
| `!pitar`      | Los tres pitidos de una alarma, para probar el parlante |
| `!brillo N`   | El brillo, de 1 a 5, como en Info (anda también sin la ECU) |
| `!luz V`      | La luz con un valor de 0 a 255, sin guardar, para medir los niveles (con 90 o menos se apaga). `!brillo N` vuelve a un nivel |
| `!imu`        | La última medición del IMU en sus ejes (para ver cómo está montado), la fuerza G que sale de ella, los máximos y cuántas veces calibró |
| `!sd`         | La microSD: tamaño y espacio libre, el archivo del viaje, cuánto lleva grabado, líneas perdidas y errores |
| `!hora AAAA-MM-DD HH:MM` | Pone el reloj en hora (también con `:SS`). Sin nada, muestra la hora |
| `!descubrir`  | Relevamiento de la ECU (PIDs, tiempos), con la ECU conectada |
| `!escanear`   | Busca dispositivos BLE durante 10 s |
| `!explorar N` | Muestra los servicios del dispositivo N de la búsqueda |

Cada segundo, con la ECU conectada, imprime una línea con todos los datos,
las consultas por segundo (~6), lo que tardan los cuadros de la pantalla, la
distancia y el tiempo en marcha del viaje y la fuerza G. Cada cambio de la
tarjeta también sale ahí.

Lo de la pantalla es cuántos cuadros se mandaron en el último segundo y, de
cada tiempo, el promedio y el máximo de esos cuadros, en ms
(`dibujo 3.2/7.9` es 3,2 de promedio y 7,9 de máximo):

- `dibujo`: LVGL dibujando lo que cambió.
- `espera`: el programa esperando que se termine de mandar la imagen.
- `giro`: pasar la imagen a vertical, como la espera el panel.
- `envio`: mandar la imagen al panel, con el giro.

Lo que se le resta a las consultas es `dibujo` más `espera`.

## Hardware

- Waveshare ESP32-S3-Touch-LCD-3.49B, V2 (la plaqueta dice `Rev1.1`):
  ESP32-S3R8 (8 MB de PSRAM, 16 MB de flash), pantalla IPS de 172×640 con
  touch capacitivo (AXS15231B), parlante, con el códec ES8311 y el
  amplificador NS4150B, IMU QMI8658 (se usa solo el acelerómetro), reloj
  PCF85063 y ranura microSD (en modo SD de 1 bit). Se usa en horizontal, a
  640×172.
- Adaptador OBD BLE: anuncia `OBDBLE` con el
  servicio `FFF0` y responde como `ELM327 v2.1`. Detalle en **Datos
  descubiertos** de `PLAN.md`.
- Una batería de 3,7 V en el conector de la placa, solo para que el reloj no
  pierda la hora: la placa se sigue apagando con la llave.

El firmware usa los pines de la V2: la luz por PWM en el GPIO42 y el reset de
la pantalla por el expansor TCA9554. Hasta E15 usaba los de la V1 (la luz en
el GPIO8), y por eso el brillo no se podía regular (notas de E6, E11 y E15
en `PLAN.md`). Con una V1 no anda: habría que volver a esos pines.

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
  el touch (y la fuerza G). Para la fuerza G, de frente: el lado largo de la
  pantalla paralelo al eje de las ruedas; inclinada para atrás no importa.
- Al poner el contacto, la placa y el adaptador arrancan juntos: lleva
  unos segundos llegar a los datos. Mientras tanto se ve el DS3 de la
  pantalla de espera.

## Entorno de desarrollo

- Visual Studio Code con PlatformIO.
- `espressif32 @ 8.0.0` (Arduino-ESP32 3.3.x), placa
  `esp32-s3-devkitc-1` con la configuración de `platformio.ini`; LVGL
  9.3.0.
- Optimización: Arduino-ESP32 3.3 compila el código sin `-O` (queda en
  `-O0`). `optimizar.py` sube a `-O2` solo LVGL y el giro de la imagen
  (`src/giro.cpp`), que son lo que cuesta dibujar; lo demás queda sin
  optimizar.
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
del ELM, PIDs, fallas, el viaje, las alarmas, la fuerza G, el registro y el
reloj)
tiene tests que corren en la PC, con respuestas reales del DS3 y del
adaptador:

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
su lugar, que los toques y los deslizamientos hagan lo que corresponde y que
las animaciones se muevan.

## Las animaciones

Las animaciones del DS3 (la entrada de la pantalla de espera y la página
DS3) vienen como cuadros PNG de 640×172, que no entran en la flash. El
script `herramientas/imagenes/convertir.py` (Python 3, sin dependencias)
separa las partes (el auto, las llantas, las nubes, el fondo) y las escribe
en `src/imagenes/`; la placa las mueve y gira las llantas. Para cambiarlas,
reemplazar los archivos de `herramientas/imagenes/fuentes/` y correr:

```text
python3 herramientas/imagenes/convertir.py
```

## Documentación

- `PLAN.md`: decisiones, arquitectura, etapas, datos medidos en el auto y
  notas de cada etapa.
- `docs/descubrimiento/`: los logs de las sesiones en el auto.
