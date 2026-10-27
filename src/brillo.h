#pragma once

#include <stdint.h>

// El brillo de la pantalla en niveles, de 1 a CANT_NIVELES_BRILLO (BRILLOS
// de config.h), guardado en Preferences para el próximo arranque.
class Brillo {
public:
  // Lee el guardado; sin nada guardado, el más alto
  void iniciar();
  uint8_t nivel() const {
    return actual;
  }
  // De 0 a 255, para PantallaHw::ponerBrillo()
  uint8_t valor() const;
  // Lo cambia y lo guarda. false si está fuera de rango o es el mismo.
  bool poner(uint8_t nivel);

private:
  uint8_t actual = 0;
};
