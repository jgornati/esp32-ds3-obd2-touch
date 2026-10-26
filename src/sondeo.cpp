#include "sondeo.h"

#include "config.h"

void Sondeo::reiniciar(uint32_t ahora) {
  for (uint32_t &momento : ultima) {
    momento = ahora - PERIODO_LENTO_MS;
  }
}

void Sondeo::visibles(const Magnitud *lista, uint8_t cantidad) {
  for (bool &v : visible) {
    v = false;
  }
  for (uint8_t i = 0; i < cantidad; i++) {
    visible[lista[i]] = true;
  }
}

uint32_t Sondeo::periodo(Magnitud m) const {
  if (m == VOLT_ELM) {
    return PERIODO_ATRV_MS;
  }
  return visible[m] ? 0 : PERIODO_LENTO_MS;
}

bool Sondeo::siguiente(uint32_t ahora, Magnitud &m) const {
  bool hay = false;
  long mayorAtraso = 0;
  for (uint8_t i = 0; i < CANT_MAGNITUDES; i++) {
    Magnitud candidata = (Magnitud)i;
    if (dato(candidata).estado == EstadoDato::NO_SOPORTADO) {
      continue;
    }
    long atraso = (long)(ahora - ultima[i]) - (long)periodo(candidata);
    if (atraso >= 0 && (!hay || atraso > mayorAtraso)) {
      hay = true;
      mayorAtraso = atraso;
      m = candidata;
    }
  }
  return hay;
}

uint8_t Sondeo::tocan(uint32_t ahora, Magnitud *lista, uint8_t max) const {
  // Ordenadas por atraso, de mayor a menor
  Magnitud orden[CANT_MAGNITUDES];
  long atrasos[CANT_MAGNITUDES];
  uint8_t cantidad = 0;
  for (uint8_t i = 0; i < CANT_MAGNITUDES; i++) {
    Magnitud candidata = (Magnitud)i;
    if (candidata == VOLT_ELM || dato(candidata).estado == EstadoDato::NO_SOPORTADO) {
      continue;
    }
    long atraso = (long)(ahora - ultima[i]) - (long)periodo(candidata);
    if (atraso < 0) {
      continue;
    }
    uint8_t j = cantidad++;
    while (j > 0 && atrasos[j - 1] < atraso) {
      orden[j] = orden[j - 1];
      atrasos[j] = atrasos[j - 1];
      j--;
    }
    orden[j] = candidata;
    atrasos[j] = atraso;
  }
  if (cantidad > max) {
    cantidad = max;
  }
  for (uint8_t i = 0; i < cantidad; i++) {
    lista[i] = orden[i];
  }
  return cantidad;
}

void Sondeo::consultada(Magnitud m, uint32_t ahora) {
  ultima[m] = ahora;
}
