#pragma once

#include <stddef.h>

#include "elm327.h"
#include "explorar.h"

// Consola por el USB. Una línea que empieza con '!' es un comando local;
// "+" y "-" cambian el brillo; cualquier otra se le envía al ELM (pasa por
// la lista blanca).
class Consola {
public:
  Consola(Elm327 &elm, Explorador &explorador) : elm(elm), explorador(explorador) {}

  // Llamar en cada vuelta de loop(). Bloquea mientras dura el comando.
  void atender();

  // true una sola vez después de un !reconectar (o de explorar, que corta
  // la conexión con el ELM)
  bool reconexionPedida();
  // true una sola vez después de un !olvidar
  bool olvidoPedido();
  // true una sola vez después de un !descubrir
  bool descubrimientoPedido();
  // true una sola vez después de un !info
  bool infoPedida();
  // +1 o -1 una sola vez después de una línea "+" o "-"; si no, 0
  int ajusteBrillo();
  // !log lo cambia: con true se ve cada consulta periódica, no solo las fallas
  bool detalle() const {
    return conDetalle;
  }
  // funcion se llama después de soltar el ELM para !escanear o !explorar,
  // que bloquean varios segundos (para sacar los datos de la pantalla).
  void antesDeExplorar(void (*funcion)()) {
    alSoltar = funcion;
  }

private:
  void ejecutar(const char *linea);
  void soltarElm();

  Elm327 &elm;
  Explorador &explorador;
  char linea[64];
  size_t largo = 0;
  bool descartando = false;
  bool reconectar = false;
  bool olvidar = false;
  bool descubrir = false;
  bool info = false;
  int brillo = 0;
  bool conDetalle = false;
  void (*alSoltar)() = nullptr;
};
