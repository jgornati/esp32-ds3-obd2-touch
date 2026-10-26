#include "boton.h"

#include <Arduino.h>

#include "config.h"

void Boton::iniciar() {
  pinMode(PIN_BOTON, INPUT_PULLUP);
}

void Boton::actualizar() {
  bool ahora = digitalRead(PIN_BOTON) == LOW;
  if (ahora != lectura) {
    lectura = ahora;
    cambioMs = millis();
    return;
  }
  if (lectura != apretado && millis() - cambioMs >= ANTIRREBOTE_MS) {
    apretado = lectura;
    if (apretado) {
      pendiente = true;
    }
  }
}

bool Boton::pulsado() {
  bool pulso = pendiente;
  pendiente = false;
  return pulso;
}
