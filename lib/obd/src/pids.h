#pragma once

#include <stdint.h>

// Un PID del servicio 01. formula es nullptr si el valor no es un número
// (01, 03, 13): para esos están las funciones de más abajo.
struct Pid {
  uint8_t pid;
  const char *nombre; // corto, para la pantalla
  const char *unidad;
  uint8_t bytes; // bytes de datos que hacen falta
  float (*formula)(const uint8_t *datos);
};

// nullptr si el PID no está en la tabla
const Pid *buscarPid(uint8_t pid);

// Aplica la fórmula. false si el PID no está en la tabla, no es un número
// o faltan bytes.
bool calcularPid(uint8_t pid, const uint8_t *datos, uint8_t largo, float *valor);

// Un PID de la respuesta a un pedido de varios
struct DatoPid {
  uint8_t pid;
  const uint8_t *datos;
  uint8_t largo;
};

// Respuesta a un pedido de varios PIDs (010C0D050F): cada PID viene
// seguido de sus bytes. Los separa según el largo de cada uno en la tabla
// y deja hasta max en salida; devuelve cuántos. Se detiene en un PID que no
// está en la tabla (no se sabe cuántos bytes ocupa) o al que le faltan bytes.
uint8_t separarPids(const uint8_t *datos, uint8_t largo, DatoPid *salida, uint8_t max);

// PID 01: luz de falla (MIL) y cantidad de códigos guardados.
bool luzFalla(const uint8_t *datos, uint8_t largo, bool *encendida, uint8_t *cantidad);

// PID 03: estado del sistema de combustible 1 (primer byte), en texto
// corto. nullptr si no es uno de los valores definidos.
const char *textoLazo(uint8_t estado);

// PID 13: si hay sonda de oxígeno en esa posición. banco 1-2, sensor 1-4.
bool sondaPresente(uint8_t mapa, uint8_t banco, uint8_t sensor);

// PIDs soportados del servicio 01, a partir de las respuestas a 0100,
// 0120, 0140... El 00 se considera siempre soportado.
class PidsSoportados {
public:
  void limpiar();
  // base: 0x00, 0x20, 0x40... false si la base no es válida o faltan bytes
  bool cargar(uint8_t base, const uint8_t *datos, uint8_t largo);
  bool soportado(uint8_t pid) const;
  // Sin contar los que solo anuncian el mapa siguiente (20, 40, 60...)
  uint8_t cantidad() const;

private:
  uint32_t mapas[8] = {};
};
