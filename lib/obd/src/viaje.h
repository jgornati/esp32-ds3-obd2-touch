#pragma once

#include <stddef.h>
#include <stdint.h>

// El viaje en curso (E10): distancia, tiempo con el motor en marcha y los
// máximos, a partir de cada lectura válida de la ECU y del ELM. Los momentos
// son los de millis(). Vive en la RAM: empieza al poner el contacto, porque
// la placa arranca con la llave.

struct ReglasViaje {
  uint32_t huecoMaximoMs;   // entre dos lecturas: un hueco más largo no suma distancia ni tiempo
  float rpmEnMarcha;        // desde esta RPM el motor está en marcha
  uint32_t esperaVoltajeMs; // el voltaje cuenta después de este rato en marcha
};

// Un máximo o un mínimo. hay: ya tuvo alguna lectura.
struct Extremo {
  bool hay = false;
  float valor = 0;
};

class Viaje {
public:
  explicit Viaje(const ReglasViaje &definidas) : reglas(definidas) {}

  void reiniciar();
  // La distancia sale de integrar la velocidad entre dos lecturas seguidas
  void velocidad(float kmh, uint32_t ahora);
  // El tiempo en marcha suma lo que pasa entre dos lecturas en marcha seguidas
  void rpm(float valor, uint32_t ahora);
  void refrigerante(float grados);
  // Cuenta solo con el motor en marcha desde hace esperaVoltajeMs, y con la
  // última RPM de hace menos de huecoMaximoMs
  void voltaje(float volts, uint32_t ahora);

  double distanciaKm() const;
  uint32_t tiempoEnMarchaMs() const;
  // La distancia sobre el tiempo en marcha. false sin tiempo en marcha.
  bool promedio(float *kmh) const;
  // hay es false hasta la primera lectura. La velocidad máxima dice también
  // si ya hubo alguna velocidad, y la RPM máxima, alguna RPM.
  const Extremo &velocidadMaxima() const {
    return maxVelocidad;
  }
  const Extremo &rpmMaxima() const {
    return maxRpm;
  }
  const Extremo &refrigeranteMaximo() const {
    return maxRefrigerante;
  }
  const Extremo &voltajeMinimo() const {
    return minVoltaje;
  }
  const Extremo &voltajeMaximo() const {
    return maxVoltaje;
  }

private:
  ReglasViaje reglas;
  double metros = 0;
  uint32_t enMarchaMs = 0;
  // La última lectura de cada una
  bool hayVelocidad = false;
  float ultimaVelocidad = 0;
  uint32_t momentoVelocidad = 0;
  bool hayRpm = false;
  float ultimaRpm = 0;
  uint32_t momentoRpm = 0;
  uint32_t enMarchaDesde = 0;
  Extremo maxVelocidad, maxRpm, maxRefrigerante, minVoltaje, maxVoltaje;
};

// Los km con un decimal hasta 99.9 ("23.7") y sin decimales desde 100 ("123")
void textoDistancia(double km, char *salida, size_t tam);
// Hasta la hora, los minutos ("38"); desde ahí, horas y minutos ("1:07").
// Devuelve la unidad: "min" o "h".
const char *textoTiempo(uint32_t ms, char *salida, size_t tam);
