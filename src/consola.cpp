#include "consola.h"

#include <Arduino.h>

#include "config.h"

static const char *const AYUDA = "Comandos locales: !reconectar, !olvidar, !descubrir, !log, "
                                 "!info, !pitar, !escanear, !explorar N";

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

bool Consola::reconexionPedida() {
  bool pedida = reconectar;
  reconectar = false;
  return pedida;
}

bool Consola::olvidoPedido() {
  bool pedido = olvidar;
  olvidar = false;
  return pedido;
}

bool Consola::descubrimientoPedido() {
  bool pedido = descubrir;
  descubrir = false;
  return pedido;
}

bool Consola::infoPedida() {
  bool pedida = info;
  info = false;
  return pedida;
}

bool Consola::pitidoPedido() {
  bool pedido = pitido;
  pitido = false;
  return pedido;
}

// El adaptador acepta una sola conexión y, conectado, no se anuncia: para
// explorar hay que soltarlo. Después se reconecta solo.
void Consola::soltarElm() {
  if (elm.conectado()) {
    Serial.println("Consola: se corta la conexion con el ELM para explorar");
    elm.desconectar();
  }
  if (alSoltar != nullptr) {
    alSoltar();
  }
  reconectar = true;
}

void Consola::ejecutar(const char *texto) {
  if (texto[0] != '!') {
    // enviar() ya deja el comando, la respuesta o el rechazo en el log
    elm.enviar(texto, TIMEOUT_CONSOLA_MS);
    return;
  }

  char comando[16];
  int numero = 0;
  int leidos = sscanf(texto, "%15s %d", comando, &numero);

  if (strcasecmp(comando, "!reconectar") == 0) {
    Serial.println("Consola: reconectando");
    reconectar = true;
  } else if (strcasecmp(comando, "!olvidar") == 0) {
    Serial.println("Consola: olvidando el adaptador");
    olvidar = true;
  } else if (strcasecmp(comando, "!descubrir") == 0) {
    descubrir = true;
  } else if (strcasecmp(comando, "!log") == 0) {
    conDetalle = !conDetalle;
    Serial.printf("Consola: detalle de las consultas %s\n", conDetalle ? "SI" : "NO");
  } else if (strcasecmp(comando, "!info") == 0) {
    info = true;
  } else if (strcasecmp(comando, "!pitar") == 0) {
    pitido = true;
  } else if (strcasecmp(comando, "!escanear") == 0) {
    soltarElm();
    explorador.escanear();
  } else if (strcasecmp(comando, "!explorar") == 0 && leidos >= 2) {
    soltarElm();
    explorador.explorar(numero);
  } else {
    Serial.printf("Consola: no conozco %s. %s\n", texto, AYUDA);
  }
}
