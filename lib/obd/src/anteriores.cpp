#include "anteriores.h"

#include <stdio.h>
#include <string.h>

// Sube si cambia el formato de los bytes
static const uint8_t VERSION_RESUMEN = 1;

ResumenViaje resumir(const Viaje &viaje, uint32_t numero, const FechaHora *inicio) {
  ResumenViaje r;
  r.numero = numero;
  r.conFecha = inicio != nullptr;
  if (inicio != nullptr) {
    r.inicio = *inicio;
    r.inicio.segundo = 0;
  }
  r.km = (float)viaje.distanciaKm();
  r.enMarchaMs = viaje.tiempoEnMarchaMs();
  r.velocidadMaxima = viaje.velocidadMaxima();
  r.rpmMaxima = viaje.rpmMaxima();
  r.refrigeranteMaximo = viaje.refrigeranteMaximo();
  r.voltajeMinimo = viaje.voltajeMinimo();
  r.voltajeMaximo = viaje.voltajeMaximo();
  return r;
}

bool promedio(const ResumenViaje &r, float *kmh) {
  if (r.enMarchaMs == 0) {
    return false;
  }
  *kmh = (float)((double)r.km * 3600000 / r.enMarchaMs);
  return true;
}

// Los números, con el byte menos significativo primero

static uint8_t *poner32(uint8_t *p, uint32_t valor) {
  for (int i = 0; i < 4; i++) {
    *p++ = (uint8_t)(valor >> (8 * i));
  }
  return p;
}

static uint8_t *ponerFloat(uint8_t *p, float valor) {
  uint32_t bits;
  memcpy(&bits, &valor, sizeof(bits));
  return poner32(p, bits);
}

static const uint8_t *sacar32(const uint8_t *p, uint32_t *valor) {
  *valor = 0;
  for (int i = 0; i < 4; i++) {
    *valor |= (uint32_t)*p++ << (8 * i);
  }
  return p;
}

static const uint8_t *sacarFloat(const uint8_t *p, float *valor) {
  uint32_t bits;
  p = sacar32(p, &bits);
  memcpy(valor, &bits, sizeof(bits));
  return p;
}

// Los extremos, en el orden de los bytes
static Extremo ResumenViaje::*const EXTREMOS[] = {
    &ResumenViaje::velocidadMaxima, &ResumenViaje::rpmMaxima, &ResumenViaje::refrigeranteMaximo,
    &ResumenViaje::voltajeMinimo, &ResumenViaje::voltajeMaximo};
static const size_t CANT_EXTREMOS = sizeof(EXTREMOS) / sizeof(EXTREMOS[0]);

// Versión, número, banderas (la fecha y qué extremos hay), la fecha (año,
// mes, día, hora y minuto), km, tiempo en marcha y los extremos
void empaquetar(const ResumenViaje &r, uint8_t bytes[LARGO_RESUMEN]) {
  uint8_t *p = bytes;
  *p++ = VERSION_RESUMEN;
  p = poner32(p, r.numero);
  uint8_t banderas = r.conFecha ? 1 : 0;
  for (size_t i = 0; i < CANT_EXTREMOS; i++) {
    banderas |= (r.*EXTREMOS[i]).hay ? 2 << i : 0;
  }
  *p++ = banderas;
  FechaHora f = r.conFecha ? r.inicio : FechaHora{};
  *p++ = (uint8_t)f.anio;
  *p++ = (uint8_t)(f.anio >> 8);
  *p++ = f.mes;
  *p++ = f.dia;
  *p++ = f.hora;
  *p++ = f.minuto;
  p = ponerFloat(p, r.km);
  p = poner32(p, r.enMarchaMs);
  for (Extremo ResumenViaje::*e : EXTREMOS) {
    p = ponerFloat(p, (r.*e).valor);
  }
}

bool desempaquetar(const uint8_t *bytes, size_t largo, ResumenViaje *r) {
  if (largo != LARGO_RESUMEN || bytes[0] != VERSION_RESUMEN) {
    return false;
  }
  ResumenViaje leido;
  const uint8_t *p = sacar32(bytes + 1, &leido.numero);
  uint8_t banderas = *p++;
  leido.conFecha = banderas & 1;
  FechaHora f = {};
  f.anio = (uint16_t)(p[0] | p[1] << 8);
  f.mes = p[2];
  f.dia = p[3];
  f.hora = p[4];
  f.minuto = p[5];
  p += 6;
  if (leido.numero == 0 || (leido.conFecha && !fechaValida(f))) {
    return false;
  }
  if (leido.conFecha) {
    leido.inicio = f;
  }
  p = sacarFloat(p, &leido.km);
  p = sacar32(p, &leido.enMarchaMs);
  for (size_t i = 0; i < CANT_EXTREMOS; i++) {
    Extremo &e = leido.*EXTREMOS[i];
    e.hay = banderas & (2 << i);
    p = sacarFloat(p, &e.valor);
  }
  *r = leido;
  return true;
}

void Anteriores::agregarNuevo(const ResumenViaje &r) {
  if (total < VIAJES_GUARDADOS) {
    total++;
  }
  for (uint8_t i = total - 1; i > 0; i--) {
    viajes[i] = viajes[i - 1];
  }
  viajes[0] = r;
}

void Anteriores::agregarViejo(const ResumenViaje &r) {
  if (total < VIAJES_GUARDADOS) {
    viajes[total++] = r;
  }
}

void textoInicio(const FechaHora &f, char *salida, size_t tam) {
  static const char *const DIAS[] = {"Dom", "Lun", "Mar", "Mie", "Jue", "Vie", "Sab"};
  snprintf(salida, tam, "%s %02u/%02u %02u:%02u", DIAS[diaDeLaSemana(f.anio, f.mes, f.dia)],
           (unsigned)f.dia, (unsigned)f.mes, (unsigned)f.hora, (unsigned)f.minuto);
}
