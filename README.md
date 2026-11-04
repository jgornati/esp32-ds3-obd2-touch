# Monitor OBDII para Citroën DS3 — pantalla táctil

Monitor de datos del motor con una placa Waveshare
ESP32-S3-Touch-LCD-3.49 que se conecta por BLE a un adaptador ELM327 y
muestra los datos en su pantalla táctil.

Es la segunda versión de `../esp32-ds3-obd2` (ESP32-CAM, ELM327 por
Bluetooth clásico y pantalla Nokia 5110), con la misma lógica OBD.

Solo lee: no borra fallas ni manda nada que cambie la configuración o el
funcionamiento del auto. Nunca muestra datos simulados ni vencidos.

Guarda un resumen de los últimos 10 viajes, que se ven también sin
conexión. Con una microSD puesta, graba además cada viaje en un CSV para
verlo en la PC.

## Qué muestra

Cinco páginas con los datos que da el DS3 VTI 1.6 16v por OBD genérico
(17 PIDs del servicio 01, CAN 11 bit a 500 kbaud), una con el viaje en curso,
otra con los anteriores, otra con la fuerza G y al final una animación:

| Página    | Datos |
| --------- | ----- |
| MOTOR     | Arriba, las RPM en 14 luces de 500 rpm: verdes, ámbar desde 4500 y rojas desde 6000. Abajo, el reloj del refrigerante, la marcha, las RPM, la velocidad y el voltaje medido por el ELM327 (ver **La marcha y el reloj**) |
| CARGA     | Carga del motor, avance del encendido y acelerador, cada uno con su barra |
| MEZCLA    | Lazo (`Cerrado`, `Abierto frio`, `Abierto carga`...), ajustes de combustible corto y largo, lambda con una barra rica/pobre y la sonda post-catalizador. Con el lazo abierto (el motor frío, el corte de inyección al soltar el acelerador) la ECU no regula con las sondas y quedan fijas: lambda y la sonda van en gris y dicen `sin regular` |
| FALLAS    | Luz de falla y códigos guardados, pendientes y permanentes, con su descripción |
| INFO      | Adaptador y señal (RSSI), la microSD, versión del ELM y protocolo, PIDs y consultas por segundo; botones Reconectar y Olvidar adaptador; brillo y aviso de cambio, con `-` y `+` |
| VIAJE     | Distancia, tiempo con el motor en marcha y velocidad promedio; velocidad, RPM y refrigerante máximos, y voltaje mínimo y máximo con el motor en marcha |
| ANTERIORES | Lo mismo de un viaje guardado, con el día y la hora en que empezó. Tocar la página pasa al anterior |
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

La temperatura del aire de admisión ya no tiene página (hasta E24 estaba en
AIRE/VOLT, con el voltaje): se sigue leyendo y queda en el registro de la
microSD.

### La marcha y el reloj

- **La marcha** no la da la ECU: sale de la velocidad sobre las RPM, que en
  cada marcha es fija (en km/h por 1000 rpm: 8,1 en 1ª, 14,6 en 2ª, 21,0 en
  3ª, 27,2 en 4ª y 33,7 en 5ª, medidas en el auto). Con el embrague apretado,
  en punto muerto, cambiando o a menos de 5 km/h no coincide con ninguna:
  `-`, en gris. Cambia con dos lecturas seguidas iguales (~0,3 s), para no
  saltar en el medio de un cambio. Si la RPM o la velocidad no tienen un valor
  actual, como cualquier dato: `...` o `--`.
- **El reloj del refrigerante** va de 40 a 130 °C: el valor en blanco hasta
  110 °C, en ámbar hasta 115 y en rojo desde ahí, y en rojo también con la
  alarma. La zona ámbar y la roja se ven en el arco aunque no se llegue.

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

Con la ECU conectada se recorren todas. Sin conexión se ve la pantalla de
espera, y deslizando se pasa a ANTERIORES y a DS3 (la barra tiene entonces
tres puntos). Si la conexión se corta, ANTERIORES y DS3 quedan; las demás
dejan la espera. Cuando vuelve, sigue la página que se veía; si era la
espera, la de antes del corte. En DS3 no se ve la barra: se cambia de página
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
- **Mantenerla apretada 3 s**, con el dedo quieto, reinicia la placa, por si
  quedó trabada: abajo crece una barra roja y dice `Mantener para
  reiniciar`. Soltarla antes reintenta. Anda también mientras la placa
  conecta con el adaptador.
- **Deslizar** pasa a ANTERIORES (hacia la izquierda) o a DS3 (hacia la
  derecha), sin reintentar. También anda mientras la placa conecta con el
  adaptador.

Nada de eso le manda nada al auto. Quedan dos pantallas de estado,
con la barra arriba:

| Barra                    | Cuándo |
| ------------------------ | ------ |
| `ALIMENTACION` (rojo)    | La placa se reinició por falta de corriente (brownout): revisar los 5 V |
| `DESCUBRIENDO`           | Durante `!descubrir` (consola) |

### Si se traba

Si la pantalla queda quieta y sin responder 10 s, la placa se reinicia sola.
Lo hace un guardián que mira que la pantalla se siga atendiendo y que cada
imagen se termine de mandar al panel.

- Antes de reiniciar anota qué se trabó, quién tenía la pantalla y en qué
  andaba la conexión. Lo dice `!info`, en `Ultimo reinicio: guardian (...)`, y
  el registro de la microSD, en `Inicio de la placa`.
- Además, el core guarda en la flash un volcado de memoria con todas las
  tareas, que queda hasta el próximo. `!info` dice de qué tarea es; para ver
  dónde estaba cada una hay que leer la partición `coredump` con esptool e
  interpretarla con el `.elf` del firmware.
- Hasta E23 se trababa ~1 min al prender en el auto: la radio, del otro lado
  del USB, no leía lo que la placa escribía, y cada línea esperaba hasta 2 s
  con la pantalla tomada. Ahora escribir no espera (ver **Consola**).

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

El viaje empieza al prenderse la placa (al abrir la puerta o poner el
contacto) y termina al apagarse, unos minutos después de sacar la llave (ver
**Instalación en el auto**). Lo que muestra VIAJE se guarda para ANTERIORES
(ver **Viajes anteriores**).

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
  desde la pantalla de espera también lo reinicia. El que termina pasa a
  ANTERIORES.

### Viajes anteriores

La placa guarda un resumen de cada viaje en su memoria (no hace falta la
microSD): lo mismo que muestra VIAJE y el día y la hora en que empezó.

- **Qué viajes:** los que llegan a 1 min con el motor en marcha; con menos
  (mover el auto en el garaje, poner el contacto sin arrancar) no se guardan.
  Quedan los últimos 10: con uno más, se borra el más viejo.
- **Cuándo se guarda:** al llegar a ese minuto y después cada minuto, si
  cambió algo. Al sacar la llave se pierde como mucho el último minuto, con el
  motor ya parado. Si la placa se reinicia en el medio (un corte o
  reiniciarla desde la espera), quedan dos viajes.
- **La página ANTERIORES,** después de VIAJE, muestra uno por vez, el más
  nuevo primero, con las mismas casillas. La barra dice cuál es y cuándo
  empezó: `ANTERIOR 1/3  Lun 26/10 18:40` (`Sin fecha` si el reloj no tenía la
  hora). Tocar la página pasa al anterior; después del más viejo vuelve al
  más nuevo, y al volver a la página empieza por el más nuevo.
- Lo que el viaje no tuvo (por ejemplo, el voltaje si el ELM no contestó)
  dice `N/D`. Sin ningún viaje guardado, dice `Sin viajes guardados`.
- El viaje en curso no aparece en ANTERIORES hasta el próximo encendido (o
  hasta reiniciarlo manteniendo apretada VIAJE).
- Se ven también sin conexión: desde la pantalla de espera, deslizando.
- `!viajes`, en la consola, los lista con todos sus datos.

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
- **Se graba cada 5 s.** Cuando la placa se apaga (unos minutos después de
  sacar la llave) se pierden esos últimos segundos, ya con el motor parado.
  El archivo queda hasta el último bloque grabado.
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
| `Marcha` | Después de `Vel km/h`: la calculada, de 1 a 5, o 0 sin marcha (ver **La marcha y el reloj**). Vacía sin un valor actual |
| `Consultas/s` | Las consultas a la ECU del último segundo |
| `Distancia km`, `En marcha s` | Las del viaje, como en VIAJE: la última línea tiene el total |
| `G lateral`, `G long.` | La fuerza G en ese momento: la lateral, positiva en una curva a la derecha; la longitudinal, positiva acelerando y negativa frenando. Vacías sin calibrar o sin sensor |
| `Pagina` | La página a la vista (`MOTOR`, `FUERZA G`...). Vacía sin datos, con la pantalla de espera |
| `Cuadros/s` a `Envio max ms` | Lo que tardan los cuadros de la pantalla en el último segundo, como en la tabla del monitor: cuántos se mandaron y el promedio y el máximo de cada tiempo, en ms (ver **Consola**) |
| `Eventos` | Lo que pasó en ese segundo, separado por ` \| `: el inicio de la placa y por qué (`encendido`, `brownout`...), los cambios de la conexión (`LEYENDO`, `SIN_ECU (No responde)`...), las alarmas, los avisos de cambio (`Aviso de cambio: 6215 rpm en 3ª`), las fallas leídas en FALLAS cuando cambian, `Viaje reiniciado`, los errores de la tarjeta y `Reloj puesto en hora` (la columna `Hora` salta) |

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
alarma y el valor (`REFRIGERANTE 112 °C`), y el valor va en rojo en su
página. Al aparecer cada una suenan tres pitidos, sin repetir. Deslizar sigue
cambiando de página: no hay nada que aceptar, y la alarma se va sola.

| Alarma                | Se prende                                   | Se apaga |
| --------------------- | ------------------------------------------- | -------- |
| `REFRIGERANTE`        | 110 °C o más, 5 s seguidos (el motor caliente regula en 103–108) | 108 °C o menos, 5 s |
| `VOLTAJE BAJO` / `ALTO` | Con el motor en marcha (desde 5 s después de arrancar): 13,0 V o menos, o 15,0 V o más, 10 s seguidos (falla de carga) | 13,2 V o más / 14,8 V o menos, 10 s |

- Con varias a la vez, se alternan cada 3 s y la barra dice `1/2`.
- Sin un valor actual (la ECU dejó de contestar, el motor se paró para el
  voltaje) la alarma se apaga: nunca muestra un valor viejo.
- Los umbrales están en `include/config.h`. Con `ALARMAS_DE_PRUEBA = true`
  saltan manejando normal (40 °C y los voltajes de marcha), para probarlas.
- Hasta E25 había una más, `ZONA ROJA`, desde 6000 rpm: la reemplaza el aviso
  de cambio. Las luces de MOTOR siguen rojas desde 6000.

### Aviso de cambio

Un aviso de "pasá el cambio", como en un auto de carrera: al llegar las RPM
al valor elegido, en cualquier página, toda la pantalla parpadea (azul con
`CAMBIO` en negro, y negro con `CAMBIO` en azul, ~4 veces por segundo) y suena
un pitido corto y agudo, distinto de los de las alarmas.

- **Cuánto dura:** mientras las RPM sigan en ese valor o más, hasta 2 s. Al
  pasar el cambio las RPM bajan y se va.
- **Una vez por marcha:** vuelve a avisar cuando las RPM bajan 300 del valor
  (lo que pasa al poner la siguiente).
- **No avisa** sin marcha (en punto muerto o con el embrague apretado: así no
  suena acelerando parado) ni en 5ª. La marcha es la de MOTOR (ver **La marcha
  y el reloj**); sin un valor actual de la RPM, tampoco.
- **Dónde se elige:** en INFO, o con `!cambio N` en la consola: de 2000 a 6500
  rpm, de 250 en 250, o apagado. Queda guardado; sin nada guardado, 6000.
- Mientras parpadea, la página de abajo no se dibuja, y deslizar sigue
  cambiando de página.
- En MEZCLA la RPM se lee cada ~2,5 s (y la marcha solo cuando la RPM y la
  velocidad llegan juntas), así que ahí el aviso puede llegar tarde o no
  llegar.
- Nada de esto le manda nada al auto.

### Info

- **Reconectar:** corta la conexión con el adaptador y vuelve a conectar.
- **Olvidar adaptador:** hay que mantenerlo apretado 3 s (se llena de rojo
  mientras tanto); soltarlo antes o correr el dedo lo cancela. Borra la
  dirección guardada y busca el adaptador de cero.
- **Brillo:** `-` y `+` lo cambian en 5 niveles (los segmentos del medio). El
  más bajo se sigue leyendo de noche: por debajo, la luz se apaga del todo.
  Queda guardado para el próximo arranque, un segundo después del último
  toque; sin nada guardado, al máximo.
- **Aviso de cambio:** `-` y `+` lo cambian de 250 en 250 rpm, de 2000 a 6500;
  con `-` desde 2000 queda `Apagado`, en gris (ver **Aviso de cambio**). Se
  guarda como el brillo; sin nada guardado, 6000.
- Cada toque de `-` y `+` cuenta, aunque se toque seguido, y aceptan el toque
  hasta 6 px afuera del borde. Hasta E25 se perdían algunos.
- Desde E25, Reconectar y Olvidar van abajo a la izquierda, y el brillo y el
  aviso de cambio a la derecha, uno arriba del otro.
- **Tarjeta:** la microSD (ver **Registro en la microSD**). Hasta E13 ese
  renglón era la dirección del adaptador, que ahora sale en `!info`.

Ninguno le manda nada al auto.

### Consola

Por el USB-C, a 115200 baudios. Una línea sin `!` se le manda al ELM,
siempre que esté en la lista blanca de comandos de solo lectura (`04`, que
borra las fallas, da `RECHAZADO`).

Escribir por el USB no espera nunca: sin el monitor abierto, o en el auto con
la radio del otro lado, lo que no entra en el búfer (4 KB) se pierde, en vez
de frenar la placa.

| Comando       | Qué hace |
| ------------- | -------- |
| `!info`       | Chip, memoria, último reinicio (si fue el guardián, por qué: ver **Si se traba**), el volcado del último pánico, pantalla (los cuadros del último segundo, los barridos del panel por el `LCD_TE` y un refresco completo), adaptador, ECU, tarjeta y reloj |
| `!reconectar` | Como el botón Reconectar |
| `!olvidar`    | Como Olvidar adaptador, sin mantener apretado |
| `!log`        | Prende o apaga el detalle de cada consulta |
| `!pitar`      | Los tres pitidos de una alarma, para probar el parlante. `!pitar cambio`: el del aviso de cambio |
| `!brillo N`   | El brillo, de 1 a 5, como en Info (anda también sin la ECU) |
| `!cambio N`   | Las RPM del aviso de cambio, de 2000 a 6500 de 250 en 250, o 0 para apagarlo, como en Info (anda también sin la ECU). Sin nada, dice cuál está |
| `!luz V`      | La luz con un valor de 0 a 255, sin guardar, para medir los niveles (con 90 o menos se apaga). `!brillo N` vuelve a un nivel |
| `!imu`        | La última medición del IMU en sus ejes (para ver cómo está montado), la fuerza G que sale de ella, los máximos y cuántas veces calibró |
| `!sd`         | La microSD: tamaño y espacio libre, el archivo del viaje, cuánto lleva grabado, líneas perdidas y errores |
| `!viajes`     | Los viajes anteriores guardados, con todos sus datos, y si el de ahora ya se guarda |
| `!hora AAAA-MM-DD HH:MM` | Pone el reloj en hora (también con `:SS`). Sin nada, muestra la hora |
| `!trabar`     | Traba la pantalla a propósito, para probar el guardián: a los ~10 s la placa se tiene que reiniciar sola (ver **Si se traba**) |
| `!descubrir`  | Relevamiento de la ECU (PIDs, tiempos), con la ECU conectada |
| `!escanear`   | Busca dispositivos BLE durante 10 s |
| `!explorar N` | Muestra los servicios del dispositivo N de la búsqueda |

Cada segundo, con la ECU conectada, imprime una línea con todos los datos y
la marcha, las consultas por segundo (~6), lo que tardan los cuadros de la
pantalla, la distancia y el tiempo en marcha del viaje y la fuerza G. Cada
cambio de la tarjeta también sale ahí.

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
  pierda la hora: la placa se sigue apagando cuando el auto corta el USB.

El firmware usa los pines de la V2: la luz por PWM en el GPIO42 y el reset de
la pantalla por el expansor TCA9554. Desde E17 lee también el `LCD_TE`
(GPIO21), para mandar cada imagen con el barrido del panel. Hasta E15 usaba
los de la V1 (la luz en el GPIO8), y por eso el brillo no se podía regular
(notas de E6, E11 y E15 en `PLAN.md`). Con una V1 no anda: habría que volver
a esos pines.

## Instalación en el auto

- **Placa:** 5 V desde el USB de la consola, el mismo de la radio (conviene
  que dé 1 A o más). Ese USB se prende al abrir la puerta, antes de poner la
  llave, y se corta unos minutos después de sacarla (de 30 s a 4 min en los
  registros): mientras tanto la placa espera el adaptador. Del otro lado está
  la radio, que es un host USB: hasta E23, si no leía, la placa se trababa
  ~1 min al prender (ver **Si se traba**).
- **Adaptador:** alimentado desde el pin 1 del conector OBD, que en el DS3
  se corta con la llave, en lugar del 16 (permanente): se le sacó la pata
  16 y se puenteó la 1 al pad. Así no queda consumiendo con el auto
  apagado.
- **Montaje:** lejos del sol directo (el LCD se oscurece con calor). La
  imagen se ve derecha con `PANTALLA_GIRADA = false` en
  `include/config.h`; si se monta al revés, `true` la gira 180° junto con
  el touch (y la fuerza G). Para la fuerza G, de frente: el lado largo de la
  pantalla paralelo al eje de las ruedas; inclinada para atrás no importa.
- Al abrir la puerta arranca la placa, y al poner el contacto el adaptador:
  lleva unos segundos llegar a los datos. Mientras tanto se ve el DS3 de la
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
del ELM, PIDs, fallas, el viaje, los viajes anteriores, las alarmas, la fuerza
G, el registro, el reloj, la marcha y el aviso de cambio)
tiene tests que corren en la PC, con respuestas reales del DS3 y del
adaptador (la marcha, con lecturas de los registros de la microSD):

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
su lugar, que los toques y los deslizamientos hagan lo que corresponde (con
la ECU y sin conexión), que las animaciones se muevan y que el aviso de cambio
tape todo y parpadee.

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

## Licencia

[MIT](LICENSE) © 2026 Joel Gornati. Se puede usar, modificar y redistribuir,
también con fines comerciales, siempre que se mantenga el aviso de copyright.
`lib/axs15231b` conserva su propia licencia (Apache 2.0).
