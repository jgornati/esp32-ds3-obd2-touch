#pragma once

#include <stdint.h>

#include "datos.h"

// Decide qué consultar en cada vuelta de LEYENDO, de a una magnitud o
// varias juntas: las visibles todas las vueltas, VOLT_ELM cada
// PERIODO_ATRV_MS y el resto cada PERIODO_LENTO_MS. Las NO_SOPORTADO no se
// consultan.
class Sondeo {
public:
  // Al entrar en LEYENDO: todas quedan para consultar ya.
  void reiniciar(uint32_t ahora);
  void visibles(const Magnitud *lista, uint8_t cantidad);
  // La más atrasada de las que tocan. false si no toca ninguna.
  bool siguiente(uint32_t ahora, Magnitud &m) const;
  // Para pedir varias juntas: las que tocan, de la más atrasada a la menos,
  // sin VOLT_ELM (no es un PID). Deja hasta max en lista y devuelve cuántas.
  uint8_t tocan(uint32_t ahora, Magnitud *lista, uint8_t max) const;
  void consultada(Magnitud m, uint32_t ahora);

private:
  uint32_t periodo(Magnitud m) const;

  uint32_t ultima[CANT_MAGNITUDES] = {};
  bool visible[CANT_MAGNITUDES] = {};
};
