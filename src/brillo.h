#pragma once

#include <stdint.h>

// El brillo de la pantalla en niveles, de 1 a CANT_NIVELES_BRILLO (BRILLOS
// de config.h), guardado en Preferences para el próximo arranque. Se guarda
// ESPERA_GUARDAR_AJUSTE_MS después del último cambio (E25), con guardar().
class Brillo {
public:
  // Lee el guardado; sin nada guardado, el más alto
  void iniciar();
  uint8_t nivel() const {
    return actual;
  }
  // De 0 a 255, para PantallaHw::ponerBrillo()
  uint8_t valor() const;
  // Lo cambia, para guardarlo después. false si está fuera de rango o es el mismo.
  bool poner(uint8_t nivel);
  // En cada vuelta de loop(): lo guarda si cambió y ya pasó la espera
  void guardar();

private:
  uint8_t actual = 0;
  uint8_t guardado = 0;
  uint32_t cambiadoEn = 0;
};
