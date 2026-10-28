#include "consola.h"

#include <Arduino.h>

#include "config.h"

static const char *const AYUDA =
    "Comandos locales: !reconectar, !olvidar, !descubrir, !log, "
    "!info, !pitar, !pitar cambio, !brillo N, !cambio N, !luz V, !imu, !sd, !viajes, !hora, "
    "!trabar, !escanear, !explorar N";

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

bool Consola::pitidoCambioPedido() {
  bool pedido = pitidoCambio;
  pitidoCambio = false;
  return pedido;
}

bool Consola::trabaPedida() {
  bool pedida = trabar;
  trabar = false;
  return pedida;
}

bool Consola::imuPedido() {
  bool pedido = imu;
  imu = false;
  return pedido;
}

bool Consola::sdPedido() {
  bool pedido = sd;
  sd = false;
  return pedido;
}

bool Consola::viajesPedido() {
  bool pedido = viajes;
  viajes = false;
  return pedido;
}

const char *Consola::horaPedida() {
  if (!hora) {
    return nullptr;
  }
  hora = false;
  return textoHora;
}

bool Consola::luzPedida(uint8_t *valor) {
  if (!luz) {
    return false;
  }
  luz = false;
  *valor = valorLuz;
  return true;
}

uint8_t Consola::brilloPedido() {
  uint8_t pedido = brillo;
  brillo = 0;
  return pedido;
}

bool Consola::cambioPedido(int32_t *rpm) {
  if (!cambio) {
    return false;
  }
  cambio = false;
  *rpm = rpmCambio;
  return true;
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

void Consola::terminarExploracion() {
  if (alTerminar != nullptr) {
    alTerminar();
  }
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
  char palabra[16] = "";
  sscanf(texto, "%*s %15s", palabra);

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
  } else if (strcasecmp(comando, "!pitar") == 0 && strcasecmp(palabra, "cambio") == 0) {
    pitidoCambio = true;
  } else if (strcasecmp(comando, "!pitar") == 0 && palabra[0] == '\0') {
    pitido = true;
  } else if (strcasecmp(comando, "!trabar") == 0) {
    trabar = true;
  } else if (strcasecmp(comando, "!imu") == 0) {
    imu = true;
  } else if (strcasecmp(comando, "!sd") == 0) {
    sd = true;
  } else if (strcasecmp(comando, "!viajes") == 0) {
    viajes = true;
  } else if (strcasecmp(comando, "!hora") == 0) {
    // El resto de la línea, sin los espacios del principio
    const char *resto = texto + strlen("!hora");
    while (*resto == ' ') {
      resto++;
    }
    strlcpy(textoHora, resto, sizeof(textoHora));
    hora = true;
  } else if (strcasecmp(comando, "!brillo") == 0 && leidos >= 2 && numero > 0) {
    brillo = numero > UINT8_MAX ? UINT8_MAX : numero;
  } else if (strcasecmp(comando, "!cambio") == 0 && (leidos >= 2 || palabra[0] == '\0')) {
    // Sin número, lo muestra; si no, main.cpp revisa que sea válido
    rpmCambio = leidos >= 2 ? (numero < 0 ? INT32_MAX : numero) : -1;
    cambio = true;
  } else if (strcasecmp(comando, "!luz") == 0 && leidos >= 2 && numero >= 0 && numero <= 255) {
    valorLuz = numero;
    luz = true;
  } else if (strcasecmp(comando, "!escanear") == 0) {
    soltarElm();
    explorador.escanear();
    terminarExploracion();
  } else if (strcasecmp(comando, "!explorar") == 0 && leidos >= 2) {
    soltarElm();
    explorador.explorar(numero);
    terminarExploracion();
  } else {
    Serial.printf("Consola: no conozco %s. %s\n", texto, AYUDA);
  }
}
