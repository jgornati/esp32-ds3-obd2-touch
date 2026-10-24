# Monitor OBDII para Citroën DS3 — pantalla táctil

Monitor de datos del motor con una placa Waveshare
ESP32-S3-Touch-LCD-3.49 que se conecta por BLE a un adaptador ELM327 y
muestra los datos en su pantalla táctil.

Es la segunda versión de `../esp32-ds3-obd2` (ESP32-CAM, ELM327 por
Bluetooth clásico y pantalla Nokia 5110), con la misma lógica OBD.

## Objetivo

Leer y mostrar, igual que la versión anterior:

- RPM, temperatura del refrigerante, velocidad y temperatura del aire de
  admisión.
- Voltaje medido por el ELM327.
- Mezcla aire/combustible: lazo, ajustes de combustible, lambda y sonda
  post-catalizador.
- Códigos de falla del DS3 VTI 1.6 16v (solo OBD genérico).

## Componentes

- Waveshare ESP32-S3-Touch-LCD-3.49: ESP32-S3R8 (8 MB de PSRAM, 16 MB de
  flash), pantalla IPS de 172×640 con touch capacitivo.
- Adaptador OBD BLE compatible con iPhone. Modelo, nombre y UUIDs: ver
  **Datos descubiertos** en `PLAN.md` (E0).
- Cable USB-C para programar y para el monitor serie.
- Alimentación: 5 V desde un USB del auto.

## Entorno de desarrollo

- Visual Studio Code con PlatformIO.
- `espressif32 @ 8.0.0` (Arduino-ESP32 3.3.x), placa
  `esp32-s3-devkitc-1` con la configuración de `platformio.ini`.
- Monitor serie: 115200 baudios, por el USB-C.

## Tests

La lógica que no depende del hardware (`lib/obd`) tiene tests que corren
en la PC. Desde la carpeta del proyecto (`pio` no está en el PATH):

```text
~/.platformio/penv/bin/pio test -e native
```

## Alcance

Solo lectura y visualización. No borra fallas ni envía comandos que
modifiquen la configuración o el funcionamiento del vehículo. No muestra
datos simulados como reales.
