#pragma once

#include <reloj.h>

// El RTC PCF85063 de la placa (E14), en el I2C de la placa (iniciarPlaca()
// antes). Se lee al arrancar y pone la hora del sistema (time()): de ahí la
// toman la barra, el registro y la FAT. Guarda la hora local, en 24 h. Con la
// batería puesta sigue andando con la placa apagada.
class Rtc {
public:
  // false si no contesta. Si contesta pero perdió la hora, queda sin hora
  // hasta poner(). Lo que pasó sale en el monitor serie.
  bool iniciar();
  // Escribe f en el RTC y en la hora del sistema. false si no contesta.
  bool poner(const FechaHora &f);
  // La hora, si el RTC tenía una válida al arrancar o se puso. Sale de la
  // hora del sistema: se puede llamar desde otra tarea.
  bool ahora(FechaHora *f) const;
  // Para !hora y !info
  void imprimir() const;
};
