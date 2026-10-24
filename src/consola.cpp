#include "consola.h"

#include <Arduino.h>

#include "config.h"

static const char *const AYUDA =
    "Comandos locales: !escanear, !explorar N, !usar N [S], !desconectar, !info; + y - para el "
    "brillo";

void Consola::atender() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (descartando) {
        Serial.println("Consola: linea demasiado larga, descartada");
      } else if (largo > 0) {
        linea[largo] = '\0';
        ejecutar(linea);
      }
      largo = 0;
      descartando = false;
    } else if (largo < sizeof(linea) - 1) {
      linea[largo++] = c;
    } else {
      descartando = true;
    }
  }
}

bool Consola::infoPedida() {
  bool pedida = info;
  info = false;
  return pedida;
}

int Consola::ajusteBrillo() {
  int ajuste = brillo;
  brillo = 0;
  return ajuste;
}

void Consola::ejecutar(const char *linea) {
  if (strcmp(linea, "+") == 0 || strcmp(linea, "-") == 0) {
    brillo = linea[0] == '+' ? 1 : -1;
    return;
  }
  if (linea[0] != '!') {
    // enviar() ya deja el comando, la respuesta o el rechazo en el log
    explorador.enviar(linea, TIMEOUT_CONSOLA_MS);
    return;
  }

  char comando[16];
  int numero = 0;
  int servicio = 0;
  int leidos = sscanf(linea, "%15s %d %d", comando, &numero, &servicio);

  if (strcasecmp(comando, "!escanear") == 0) {
    explorador.escanear();
  } else if (strcasecmp(comando, "!explorar") == 0 && leidos >= 2) {
    explorador.explorar(numero);
  } else if (strcasecmp(comando, "!usar") == 0 && leidos >= 2) {
    explorador.usar(numero, leidos >= 3 ? servicio : 0);
  } else if (strcasecmp(comando, "!desconectar") == 0) {
    explorador.desconectar();
  } else if (strcasecmp(comando, "!info") == 0) {
    info = true;
  } else {
    Serial.printf("Consola: no conozco %s. %s\n", linea, AYUDA);
  }
}
