#pragma once

#include <stdint.h>

// El parlante (E11): pitidos generados acá, por el códec ES8311 y el
// amplificador NS4150B. Usa el I2C de la placa, el I2S y el pin 7 del
// TCA9554 (NS_MODE). Los demás pines del expansor no se tocan: el 1 prende
// la luz, el 6 sostiene la placa con la batería y, en la V2, el 5 es el
// reset de la pantalla.
class Sonido {
public:
  // false si algo no respondió: queda sin sonido, y pitar() no hace nada.
  // Lo que falló sale en el monitor serie.
  bool iniciar();
  bool iniciado() const;
  // No bloquea: los genera una tarea, con PITIDO_MS, SILENCIO_PITIDO_MS,
  // FRECUENCIA_PITIDO_HZ y VOLUMEN_PITIDO de config.h. Un pedido mientras
  // pita se hace después.
  void pitar(uint8_t veces);
};
