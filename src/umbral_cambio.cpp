#include "umbral_cambio.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

static const char *const ESPACIO = "cambio";
static const char *const CLAVE = "rpm";

void UmbralCambio::iniciar() {
  Preferences preferencias;
  preferencias.begin(ESPACIO, true);
  actual = preferencias.getUShort(CLAVE, CAMBIO_DE_FABRICA_RPM);
  preferencias.end();
  if (!umbralCambioValido(reglas, actual)) {
    actual = CAMBIO_DE_FABRICA_RPM;
  }
  guardado = actual;
}

bool UmbralCambio::poner(uint16_t rpm) {
  if (!umbralCambioValido(reglas, rpm) || rpm == actual) {
    return false;
  }
  actual = rpm;
  cambiadoEn = millis();
  return true;
}

void UmbralCambio::guardar() {
  if (actual == guardado || millis() - cambiadoEn < ESPERA_GUARDAR_AJUSTE_MS) {
    return;
  }
  Preferences preferencias;
  preferencias.begin(ESPACIO, false);
  preferencias.putUShort(CLAVE, actual);
  preferencias.end();
  guardado = actual;
}
