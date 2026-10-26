#pragma once

#include <stdint.h>

// Hardware de la pantalla: panel AXS15231B por QSPI, touch por I2C,
// retroiluminación con PWM y el puerto de LVGL. LVGL dibuja en horizontal
// (640x172) y la imagen se gira por software al mandarla al panel (172x640).
// No hay tarea de LVGL: atender() llama a lv_timer_handler().
class PantallaHw {
public:
  // Panel, touch y LVGL. Deja la pantalla en negro con BRILLO_INICIAL.
  bool iniciar();
  bool iniciada() const;
  // Llamar seguido desde loop() y mientras se espera al ELM
  void atender();
  // Dibuja y manda ya lo que cambió, sin esperar el período de LVGL. Para
  // antes de algo que bloquea sin llamar a atender().
  void dibujarYa();

  // 0-255. En 0 queda lo más oscura que permita el hardware.
  void ponerBrillo(uint8_t nivel);
  uint8_t brillo() const;

  // Dibuja y manda la pantalla entera. Devuelve los ms del refresco
  // completo; envio, los de mandarla al panel.
  uint32_t medirRefresco(uint32_t &envio);

  // Última lectura del touch, sin girar (para calibrar). false sin dedo.
  bool toqueCrudo(uint16_t &x, uint16_t &y) const;
};
