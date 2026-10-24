# axs15231b

Driver `esp_lcd` del controlador AXS15231B (pantalla y touch), de
Espressif Systems, con licencia Apache-2.0 (ver `LICENSE`).

Copiado sin cambios del ejemplo `Examples/Arduino/10_LVGL_V9_Test/src/`
del repositorio `waveshareteam/ESP32-S3-Touch-LCD-3.49` (commit
`def6edd0b6`):

- `src/axs15231b/`: el panel. Es lo que se usa.
- `src/touch/`: `esp_lcd_touch`, que el driver incluye. El touch se lee
  directo por I2C en `src/pantalla_hw.cpp`, como en el ejemplo.
