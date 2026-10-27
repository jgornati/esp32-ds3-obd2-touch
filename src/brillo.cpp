#include "brillo.h"

#include <Preferences.h>

#include "config.h"

static const char *const ESPACIO = "pantalla";
static const char *const CLAVE = "brillo";

void Brillo::iniciar() {
  Preferences preferencias;
  preferencias.begin(ESPACIO, true);
  actual = preferencias.getUChar(CLAVE, CANT_NIVELES_BRILLO);
  preferencias.end();
  if (actual < 1 || actual > CANT_NIVELES_BRILLO) {
    actual = CANT_NIVELES_BRILLO;
  }
}

uint8_t Brillo::valor() const {
  return BRILLOS[actual - 1];
}

bool Brillo::poner(uint8_t nivel) {
  if (nivel < 1 || nivel > CANT_NIVELES_BRILLO || nivel == actual) {
    return false;
  }
  actual = nivel;
  Preferences preferencias;
  preferencias.begin(ESPACIO, false);
  preferencias.putUChar(CLAVE, actual);
  preferencias.end();
  return true;
}
