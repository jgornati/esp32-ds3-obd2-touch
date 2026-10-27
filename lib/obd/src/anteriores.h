#pragma once

#include <stddef.h>
#include <stdint.h>

#include "reloj.h"
#include "viaje.h"

// Los viajes anteriores (E22): un resumen de cada viaje, que la placa guarda
// en la flash mientras se maneja. Acá lo que no depende de la flash: el
// resumen, sus bytes y la lista que se muestra.

// Los que se guardan: con uno más, se pierde el más viejo
constexpr uint8_t VIAJES_GUARDADOS = 10;

struct ResumenViaje {
  uint32_t numero = 0;   // 1, 2, 3...: el orden en que se hicieron. 0 sin guardar
  bool conFecha = false; // el RTC tenía la hora al empezar
  FechaHora inicio = {}; // con conFecha; sin los segundos
  float km = 0;
  uint32_t enMarchaMs = 0;
  // Como en Viaje: la velocidad máxima dice también si hubo alguna
  // velocidad, y la RPM máxima, alguna RPM
  Extremo velocidadMaxima, rpmMaxima, refrigeranteMaximo, voltajeMinimo, voltajeMaximo;
};

// El viaje como va. inicio: nullptr si no había hora.
ResumenViaje resumir(const Viaje &viaje, uint32_t numero, const FechaHora *inicio);
// La distancia sobre el tiempo en marcha, como Viaje::promedio()
bool promedio(const ResumenViaje &r, float *kmh);

// Los bytes que se guardan, con la versión del formato adelante: si cambia,
// los guardados con otro se descartan en lugar de leerse mal.
constexpr size_t LARGO_RESUMEN = 40;
void empaquetar(const ResumenViaje &r, uint8_t bytes[LARGO_RESUMEN]);
// false si el largo no es LARGO_RESUMEN, es de otra versión o no es un viaje
// válido (sin número, o con una fecha que no existe)
bool desempaquetar(const uint8_t *bytes, size_t largo, ResumenViaje *r);

// Los viajes anteriores, del más nuevo al más viejo
class Anteriores {
public:
  // Uno más nuevo que todos. Si ya hay VIAJES_GUARDADOS, se pierde el más viejo.
  void agregarNuevo(const ResumenViaje &r);
  // Uno más viejo que todos (al leerlos de la flash). Si no entra, nada.
  void agregarViejo(const ResumenViaje &r);
  uint8_t cantidad() const {
    return total;
  }
  // 0 el más nuevo
  const ResumenViaje &viaje(uint8_t i) const {
    return viajes[i];
  }

private:
  ResumenViaje viajes[VIAJES_GUARDADOS];
  uint8_t total = 0;
};

// "Lun 27/10 08:12"
void textoInicio(const FechaHora &f, char *salida, size_t tam);
