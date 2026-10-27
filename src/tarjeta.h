#pragma once

#include <stddef.h>
#include <stdint.h>

#include <registro.h>

// La microSD (E13) y el registro del viaje. Una tarea monta la tarjeta al
// prender y escribe cada PERIODO_REGISTRO_MS lo que loop() dejó en un
// búfer: loop() no espera a la tarjeta, que puede tardar cientos de ms. Lo
// demás se llama desde loop(): toma el estado con un cerrojo.
class Tarjeta {
public:
  // Crea la tarea, que busca la tarjeta. No bloquea.
  void iniciar();
  // Pide el archivo del viaje: el número siguiente al más alto de la
  // tarjeta. encabezado es su primera línea, con su '\n'.
  void abrir(const char *encabezado, size_t largo);
  // Una línea entera, con su '\n'. Sin tarjeta, no hace nada; si el búfer
  // está lleno, se pierde.
  void agregar(const char *linea, size_t largo);
  EstadoTarjeta estado();
  // El número del archivo; 0 sin archivo
  uint16_t viaje();
  // Para !sd y !info
  void imprimir();
};
