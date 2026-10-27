#pragma once

#include <stdint.h>

#include <anteriores.h>
#include <reloj.h>
#include <viaje.h>

// Los viajes en la flash (E22), con Preferences: cada uno en una clave de
// VIAJES_GUARDADOS (la del número, módulo VIAJES_GUARDADOS) y el número del
// último. Así el viaje de ahora se sobrescribe en su lugar, sin mover los
// demás, y uno que no se terminó de escribir no se confunde con otro. Desde
// loop().
class ViajesGuardados {
public:
  // Lee los de los encendidos anteriores; lo que no se puede leer se saltea
  void iniciar();
  // El viaje de ahora empieza (al arrancar y al reiniciarlo). inicio:
  // nullptr si el reloj no tiene la hora.
  void empezar(const FechaHora *inicio);
  // En cada vuelta. Con MINIMO_VIAJE_GUARDADO_MS en marcha lo guarda, y
  // después cada PERIODO_GUARDAR_VIAJE_MS si cambió.
  void actualizar(const Viaje &viaje, uint32_t ahora);
  // Al reiniciar el viaje a mano: si llegó a guardarse, lo guarda como está
  // y pasa a los anteriores
  void terminar(const Viaje &viaje);
  // Los de antes del viaje de ahora, del más nuevo al más viejo
  const Anteriores &anteriores() const {
    return lista;
  }
  // Para !viajes
  void imprimir() const;

private:
  bool guardar(const Viaje &viaje);

  Anteriores lista;
  uint32_t ultimo = 0; // el número del último guardado; 0 ninguno
  // El de ahora: su número (0 hasta guardarlo), su inicio y lo último escrito
  uint32_t numero = 0;
  bool conInicio = false;
  FechaHora inicio = {};
  uint8_t escritos[LARGO_RESUMEN] = {};
  uint32_t proximo = 0;
  uint32_t errores = 0;
};
