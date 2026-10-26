#pragma once

#include <stddef.h>
#include <stdint.h>

enum class TipoRespuesta {
  DATOS,        // respuesta OBD válida
  TEXTO,        // respuesta AT: "OK", "ELM327 v1.5", "12.4V"
  NO_DATA,      // "NO DATA"
  NO_SOPORTADO, // "?"
  SIN_ECU,      // "UNABLE TO CONNECT"
  ERROR_BUS,    // CAN ERROR, BUS ERROR, BUS INIT: ...ERROR, ERRxx, etc.
  BUFFER_LLENO, // "BUFFER FULL"
  DETENIDO,     // "STOPPED"
  AVISO_ELM,    // LV RESET, ACT ALERT, LP ALERT: conviene reinicializar el ELM
  VACIA,
  INVALIDA
};

constexpr size_t MAX_DATOS = 64;

struct Respuesta {
  TipoRespuesta tipo;
  // Bytes de datos SIN el encabezado (41 0C, 43...). Con varios PIDs
  // (010C0D...), sin el 41: cada PID seguido de sus bytes (ver separarPids()).
  uint8_t datos[MAX_DATOS];
  uint8_t largo;
  uint8_t respuestas; // respuestas válidas; más de 1 si contestaron varias ECUs
};

// Interpreta lo que el ELM respondió (todo lo anterior al '>') a comando.
// Supone encabezados apagados (ATH0); con espacios o sin ellos.
// Si contestan varias ECUs, datos es la primera respuesta válida.
// Si aparece cualquier error, gana el error aunque haya datos.
Respuesta interpretar(const char *cruda, const char *comando);

// Primera línea de texto de una respuesta AT, sin eco, "SEARCHING..." ni \r.
// Se corta si no entra en salida. false si no hay texto.
bool textoRespuesta(const char *cruda, const char *comando, char *salida, size_t tam);

// Respuesta de ATRV: "12.4V" -> 12.4
bool interpretarVoltaje(const char *cruda, float *voltios);

// Texto de ATDPN ("6", o "A6" si lo encontró en automático) -> "CAN 11b 500k".
// nullptr si no es un protocolo conocido (incluido "0", sin protocolo).
const char *nombreProtocolo(const char *dpn);

const char *nombreTipo(TipoRespuesta tipo);
