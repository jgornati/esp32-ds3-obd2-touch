#pragma once

#include <stdint.h>

#include <aviso_cambio.h>

// Las RPM del aviso de cambio (E25), guardadas en Preferences para el próximo
// arranque: las que dejan elegir las reglas, o 0 apagado. Como el brillo, se
// guardan ESPERA_GUARDAR_AJUSTE_MS después del último cambio, con guardar().
class UmbralCambio {
public:
  explicit UmbralCambio(const ReglasAvisoCambio &definidas) : reglas(definidas) {}

  // Lee el guardado; sin nada guardado (o si no vale), CAMBIO_DE_FABRICA_RPM
  void iniciar();
  uint16_t rpm() const {
    return actual;
  }
  // Lo cambia, para guardarlo después. false si no vale o es el mismo.
  bool poner(uint16_t rpm);
  // En cada vuelta de loop(): lo guarda si cambió y ya pasó la espera
  void guardar();

private:
  ReglasAvisoCambio reglas;
  uint16_t actual = 0;
  uint16_t guardado = 0;
  uint32_t cambiadoEn = 0;
};
