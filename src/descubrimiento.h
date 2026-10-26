#pragma once

#include <stdint.h>

#include "elm327.h"

// !descubrir (E4 del anterior, E3b acá): una serie fija de consultas de
// lectura para saber qué da la ECU. Deja en el monitor serie lo crudo, los
// ms y la interpretación. Es la misma serie del anterior, para comparar,
// más una medición de un pedido de varios PIDs.
// Hay que llamarla con la ECU ya encontrada (estado LEYENDO), para que el
// ELM no tenga que buscar el protocolo.
// progreso, si no es nullptr, se llama antes de cada comando.
// false si se perdió la conexión en el medio.
bool descubrir(Elm327 &elm, void (*progreso)(uint16_t hechos, uint16_t total));
