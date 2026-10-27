#pragma once

#include <stddef.h>
#include <stdint.h>

#include <pids.h>

enum class EstadoDato { ESPERANDO, VALIDO, NO_SOPORTADO, SIN_CONEXION };

struct Dato {
  EstadoDato estado = EstadoDato::ESPERANDO;
  float valor = 0;
  uint32_t actualizadoMs = 0;
};

// Lo que se muestra. VOLT_ELM sale de ATRV; el resto, de un PID del servicio 01.
enum Magnitud : uint8_t {
  RPM,
  REFRIGERANTE,
  VELOCIDAD,
  ADMISION,
  VOLT_ELM,
  VOLT_ECU,
  AJ_CORTO,
  AJ_LARGO,
  LAZO,
  LAMBDA,
  SONDA_POST,
  CARGA,
  AVANCE,
  ACELERADOR,
  CANT_MAGNITUDES
};

// PID de la magnitud; 0 para VOLT_ELM.
uint8_t pidDe(Magnitud m);
const char *etiquetaDe(Magnitud m);
// "C", "km/h", "%"; "" sin unidad (RPM, lazo, lambda)
const char *unidadDe(Magnitud m);

const Dato &dato(Magnitud m);

// Al entrar en LEYENDO: las que la ECU no soporta quedan NO_SOPORTADO y el
// resto ESPERANDO. VOLT_ELM no se toca (no depende de la ECU).
void reiniciarDatos(const PidsSoportados &soportados);
// Al perder el Bluetooth, todas.
void marcarSinConexion();
// Al conectar: VOLT_ELM pasa a ESPERANDO hasta el primer ATRV.
void marcarConectado();
void actualizarDato(Magnitud m, float valor, uint32_t ahora);

// Calcula el valor de la magnitud a partir de los datos de su PID.
bool valorDesdePid(Magnitud m, const uint8_t *datos, uint8_t largo, float *valor);

// El valor, si es VALIDO y no está vencido. false si no hay un valor actual.
bool valorActual(Magnitud m, uint32_t ahora, float *valor);

// Texto según las convenciones de pantalla: valor y unidad, "N/D", "..." o
// "--" (vencido o sin conexión). pantalla: con "°C" y "°" (UTF-8, para LVGL)
// en vez de "C" y "grad".
void textoDato(Magnitud m, uint32_t ahora, bool pantalla, char *salida, size_t tam);

// Para las páginas, que ponen la unidad aparte: el número sin la unidad (el
// estado del lazo en texto), o "N/D", "..." o "--". true si es un valor actual.
bool textoValor(Magnitud m, uint32_t ahora, char *salida, size_t tam);
