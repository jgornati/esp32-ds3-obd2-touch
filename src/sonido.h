#pragma once

#include <stdint.h>

// El parlante (E11): pitidos generados acá, por el códec ES8311 y el
// amplificador NS4150B. Usa el I2C de la placa (iniciarPlaca() antes), el I2S
// y el pin 7 del TCA9554 (NS_MODE).
class Sonido {
public:
  // false si algo no respondió: queda sin sonido, y los pitidos no hacen nada.
  // Lo que falló sale en el monitor serie.
  bool iniciar();
  bool iniciado() const;
  // No bloquean: los genera una tarea. Un pedido mientras pita se hace
  // después; el del cambio, antes que los de una alarma.
  // Los de una alarma: PITIDOS_ALARMA de PITIDO_MS, separados por
  // SILENCIO_PITIDO_MS, a FRECUENCIA_PITIDO_HZ
  void pitarAlarma();
  // El del aviso de cambio (E25): uno de PITIDO_CAMBIO_MS a FRECUENCIA_CAMBIO_HZ
  void pitarCambio();
};
