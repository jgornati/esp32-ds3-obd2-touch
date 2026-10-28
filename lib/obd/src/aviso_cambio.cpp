#include "aviso_cambio.h"

#include <stdio.h>

void AvisoCambio::ponerUmbral(uint16_t rpm) {
  umbralRpm = rpm;
  listo = true;
  enCurso = false;
}

bool AvisoCambio::revisar(bool hay, float rpm, uint8_t marcha, uint32_t momento) {
  if (umbralRpm == 0 || !hay) {
    enCurso = false;
    return false;
  }
  if (rpm <= umbralRpm - reglas.rearme) {
    listo = true;
  }
  bool avisaria = rpm >= umbralRpm && marcha >= 1 && marcha < reglas.marchas;
  if (!avisaria) {
    enCurso = false;
    return false;
  }
  if (!listo) {
    return false;
  }
  listo = false;
  enCurso = true;
  inicio = momento;
  return true;
}

bool AvisoCambio::activo(uint32_t ahora) const {
  return enCurso && ahora - inicio < reglas.duracionMs;
}

uint16_t otroUmbralCambio(const ReglasAvisoCambio &reglas, uint16_t actual, int8_t paso) {
  if (paso == 0) {
    return actual;
  }
  if (actual < reglas.minimo) {
    return paso > 0 ? reglas.minimo : 0;
  }
  if (paso < 0) {
    return actual - reglas.paso >= reglas.minimo ? actual - reglas.paso : 0;
  }
  return actual + reglas.paso <= reglas.maximo ? actual + reglas.paso : actual;
}

bool umbralCambioValido(const ReglasAvisoCambio &reglas, uint16_t rpm) {
  return rpm == 0 ||
         (rpm >= reglas.minimo && rpm <= reglas.maximo && (rpm - reglas.minimo) % reglas.paso == 0);
}

void textoUmbralCambio(uint16_t rpm, char *salida, size_t tam) {
  if (rpm == 0) {
    snprintf(salida, tam, "Apagado");
  } else {
    snprintf(salida, tam, "%u rpm", (unsigned)rpm);
  }
}
