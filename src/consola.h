#pragma once

#include <stddef.h>

#include "explorar.h"

// Consola por el USB. Una línea que empieza con '!' es un comando local;
// cualquier otra se le envía al adaptador (pasa por la lista blanca).
class Consola {
public:
  explicit Consola(Explorador &explorador) : explorador(explorador) {}

  // Llamar en cada vuelta de loop(). Bloquea mientras dura el comando.
  void atender();

  // true una sola vez después de un !info
  bool infoPedida();
  // +1 o -1 una sola vez después de una línea "+" o "-"; si no, 0
  int ajusteBrillo();

private:
  void ejecutar(const char *linea);

  Explorador &explorador;
  char linea[64];
  size_t largo = 0;
  bool descartando = false;
  bool info = false;
  int brillo = 0;
};
