#include "marcha.h"

#include <math.h>

uint8_t marchaDe(const ReglasMarcha &reglas, float rpm, float kmh) {
  if (kmh < reglas.velocidadMinima || rpm <= 0) {
    return 0;
  }
  float relacion = kmh / rpm * 1000;
  for (uint8_t i = 0; i < reglas.cantidad; i++) {
    float tolerancia = i == 0 ? reglas.toleranciaPrimera : reglas.tolerancia;
    if (fabsf(relacion / reglas.relaciones[i] - 1) <= tolerancia) {
      return i + 1;
    }
  }
  return 0;
}

void Marcha::lectura(float rpm, float kmh, uint32_t ahora) {
  if (!actual(ahora)) {
    mostrada = 0;
    candidata = 0;
    iguales = 0;
  }
  hay = true;
  ultima = ahora;
  uint8_t esta = marchaDe(reglas, rpm, kmh);
  if (esta == candidata) {
    if (iguales < UINT8_MAX) {
      iguales++;
    }
  } else {
    candidata = esta;
    iguales = 1;
  }
  if (iguales >= reglas.lecturasIguales) {
    mostrada = candidata;
  }
}

bool Marcha::actual(uint32_t ahora) const {
  return hay && ahora - ultima <= reglas.vigenciaMs;
}
