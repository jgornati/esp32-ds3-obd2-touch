#pragma once

#include <stdint.h>

// Botón de cambio de página (BOOT): PIN_BOTON contra GND, con INPUT_PULLUP.
// Una pulsación cuenta cuando la lectura queda estable ANTIRREBOTE_MS.
// Mantenerlo apretado no repite. Portado del anterior.
class Boton {
public:
  void iniciar();
  // En cada vuelta de loop() y mientras se espera al ELM, para no perder
  // pulsaciones durante una consulta.
  void actualizar();
  // true una sola vez por pulsación
  bool pulsado();

private:
  bool apretado = false; // estado ya filtrado
  bool lectura = false;  // última lectura cruda
  uint32_t cambioMs = 0; // cuándo cambió la lectura cruda
  bool pendiente = false;
};
