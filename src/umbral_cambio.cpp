#include "umbral_cambio.h"

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
}

bool UmbralCambio::poner(uint16_t rpm) {
  if (!umbralCambioValido(reglas, rpm) || rpm == actual) {
    return false;
  }
  actual = rpm;
  Preferences preferencias;
  preferencias.begin(ESPACIO, false);
  preferencias.putUShort(CLAVE, actual);
  preferencias.end();
  return true;
}
