# Monitor OBDII DS3 — pantalla táctil

- `README.md`: qué hace el proyecto y el hardware.
- `PLAN.md`: decisiones, arquitectura, etapas y notas de traspaso.
  Leer los dos antes de empezar. Trabajar solo en la etapa pedida y, al
  terminar, actualizar su estado, **Datos descubiertos** y su sección
  de **Notas de traspaso**.
- `../esp32-ds3-obd2`: la versión anterior. Se porta de ahí; no se
  modifica.

## Comandos

`pio` no está en el PATH:

- Compilar: `~/.platformio/penv/bin/pio run`
- Tests nativos: `~/.platformio/penv/bin/pio test -e native`

Subir y probar en el hardware lo hace el usuario, por el USB-C. Lo que
no se pudo verificar en hardware se deja como pendiente de
verificación; no se da por probado.

## Reglas que no se negocian

- Solo lectura. Nunca borrar fallas (`04`) ni enviar comandos que
  modifiquen el auto. Todo comando pasa por `comandoPermitido()`;
  no agregar otros caminos hacia el ELM ni ampliar la lista blanca sin
  que el usuario lo pida.
- No mostrar datos simulados como reales ni valores vencidos.
- No inicializar microSD, audio, IMU, RTC ni el expansor TCA9554 hasta
  que una etapa lo pida.
- `lib/obd` no depende de Arduino.

## Estilo

Como el proyecto anterior: nombres y comentarios en español, llave de
apertura en la misma línea (`if (x) {`, `} else {`), comentarios cortos
y solo donde aportan. El formato lo fija `.clang-format`: pasar los
archivos tocados por `clang-format` (en VS Code, Format Document).
