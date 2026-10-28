#pragma once

#include <stdint.h>

#include <aviso_cambio.h>

// Las RPM del aviso de cambio (E25), guardadas en Preferences para el próximo
// arranque: las que dejan elegir las reglas, o 0 apagado.
class UmbralCambio {
public:
  explicit UmbralCambio(const ReglasAvisoCambio &definidas) : reglas(definidas) {}

  // Lee el guardado; sin nada guardado (o si no vale), CAMBIO_DE_FABRICA_RPM
  void iniciar();
  uint16_t rpm() const {
    return actual;
  }
  // Lo cambia y lo guarda. false si no vale o es el mismo.
  bool poner(uint16_t rpm);

private:
  ReglasAvisoCambio reglas;
  uint16_t actual = 0;
};
