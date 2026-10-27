#pragma once

#include <stdint.h>

#include <fuerza_g.h>

// El IMU de la placa (E12): solo el acelerómetro del QMI8658, en el I2C de
// la placa (iniciarPlaca() antes). Una tarea lo lee cada PERIODO_IMU_MS,
// aparte de loop() y del ELM, y le pasa cada muestra a FuerzaG. Lo demás se
// llama desde loop(): toma la cuenta con un cerrojo.
class Imu {
public:
  // false si no contesta: queda sin fuerza G. Lo que falló sale en el
  // monitor serie.
  bool iniciar();
  bool iniciado() const;
  LecturaG lectura(uint32_t ahora);
  // La ECU da 0 km/h: con la placa quieta, recalibra
  void parado(bool siONo);
  void reiniciarMaximos();
  // Para !imu y !info
  bool cruda(Vector *a);
  uint32_t calibraciones();
  uint32_t lecturasFallidas() const;
};
