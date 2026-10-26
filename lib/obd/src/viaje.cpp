#include "viaje.h"

#include <stdio.h>

static void subir(Extremo &e, float valor) {
  if (!e.hay || valor > e.valor) {
    e.hay = true;
    e.valor = valor;
  }
}

static void bajar(Extremo &e, float valor) {
  if (!e.hay || valor < e.valor) {
    e.hay = true;
    e.valor = valor;
  }
}

void Viaje::reiniciar() {
  *this = Viaje(reglas);
}

void Viaje::velocidad(float kmh, uint32_t ahora) {
  uint32_t lapso = ahora - momentoVelocidad;
  if (hayVelocidad && lapso <= reglas.huecoMaximoMs) {
    // Trapecio: el promedio de las dos, en km/h, por ms son metros / 3600
    metros += (double)(ultimaVelocidad + kmh) / 2 * lapso / 3600;
  }
  hayVelocidad = true;
  ultimaVelocidad = kmh;
  momentoVelocidad = ahora;
  subir(maxVelocidad, kmh);
}

void Viaje::rpm(float valor, uint32_t ahora) {
  bool enMarcha = valor >= reglas.rpmEnMarcha;
  uint32_t lapso = ahora - momentoRpm;
  bool seguia = hayRpm && ultimaRpm >= reglas.rpmEnMarcha && lapso <= reglas.huecoMaximoMs;
  if (enMarcha && seguia) {
    enMarchaMs += lapso;
  } else if (enMarcha) {
    // Arrancó, o volvió después de un hueco: el voltaje espera de nuevo
    enMarchaDesde = ahora;
  }
  hayRpm = true;
  ultimaRpm = valor;
  momentoRpm = ahora;
  subir(maxRpm, valor);
}

void Viaje::refrigerante(float grados) {
  subir(maxRefrigerante, grados);
}

void Viaje::voltaje(float volts, uint32_t ahora) {
  bool enMarcha =
      hayRpm && ultimaRpm >= reglas.rpmEnMarcha && ahora - momentoRpm <= reglas.huecoMaximoMs;
  if (!enMarcha || ahora - enMarchaDesde < reglas.esperaVoltajeMs) {
    return;
  }
  bajar(minVoltaje, volts);
  subir(maxVoltaje, volts);
}

double Viaje::distanciaKm() const {
  return metros / 1000;
}

uint32_t Viaje::tiempoEnMarchaMs() const {
  return enMarchaMs;
}

bool Viaje::promedio(float *kmh) const {
  if (enMarchaMs == 0) {
    return false;
  }
  *kmh = (float)(metros * 3600 / enMarchaMs);
  return true;
}

void textoDistancia(double km, char *salida, size_t tam) {
  // 99.96 con un decimal sería "100.0"
  snprintf(salida, tam, km < 99.95 ? "%.1f" : "%.0f", km);
}

const char *textoTiempo(uint32_t ms, char *salida, size_t tam) {
  unsigned long minutos = ms / 60000;
  if (minutos < 60) {
    snprintf(salida, tam, "%lu", minutos);
    return "min";
  }
  snprintf(salida, tam, "%lu:%02lu", minutos / 60, minutos % 60);
  return "h";
}
