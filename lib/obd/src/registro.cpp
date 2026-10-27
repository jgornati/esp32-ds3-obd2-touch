#include "registro.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

void LineaCsv::texto(const char *t) {
  if (!primero) {
    poner(',');
  }
  primero = false;
  if (t == nullptr) {
    return;
  }
  bool comillas = strpbrk(t, ",\"\r\n") != nullptr;
  if (comillas) {
    poner('"');
  }
  for (const char *c = t; *c != '\0'; c++) {
    if (*c == '"') {
      poner('"');
    }
    poner(*c);
  }
  if (comillas) {
    poner('"');
  }
}

// Siempre queda lugar para el '\n' y el '\0' de terminar()
void LineaCsv::poner(char c) {
  if (largo + 3 > capacidad) {
    desbordada = true;
    return;
  }
  salida[largo++] = c;
}

size_t LineaCsv::terminar() {
  if (desbordada || largo + 2 > capacidad) {
    return 0;
  }
  salida[largo++] = '\n';
  salida[largo] = '\0';
  return largo;
}

void Eventos::agregar(const char *texto) {
  if (cortado) {
    return;
  }
  size_t largo = strlen(actuales);
  size_t lugar = sizeof(actuales) - largo;
  int escrito = snprintf(actuales + largo, lugar, "%s%s", largo > 0 ? " | " : "", texto);
  if (escrito >= 0 && (size_t)escrito < lugar) {
    return;
  }
  // Sin partir una letra de varios bytes (UTF-8)
  size_t corte = sizeof(actuales) - 4;
  while (corte > 0 && ((uint8_t)actuales[corte] & 0xC0) == 0x80) {
    corte--;
  }
  strcpy(actuales + corte, "...");
  cortado = true;
}

bool numeroDeViaje(const char *nombre, uint16_t *numero) {
  if (strlen(nombre) != 13 || strncasecmp(nombre, "VIAJE", 5) != 0 ||
      strcasecmp(nombre + 9, ".CSV") != 0) {
    return false;
  }
  uint16_t n = 0;
  for (uint8_t i = 5; i < 9; i++) {
    if (!isdigit((unsigned char)nombre[i])) {
      return false;
    }
    n = n * 10 + (nombre[i] - '0');
  }
  if (n == 0) {
    return false;
  }
  *numero = n;
  return true;
}

void nombreDeViaje(uint16_t numero, char *salida, size_t tam) {
  snprintf(salida, tam, "VIAJE%04u.CSV", (unsigned)numero);
}

void BuferTexto::usar(char *memoria, size_t tam) {
  datos = memoria;
  capacidad = memoria != nullptr ? tam : 0;
  ocupados = 0;
}

bool BuferTexto::agregar(const char *texto, size_t largo) {
  if (largo == 0) {
    return true;
  }
  if (largo > capacidad - ocupados) {
    lineasPerdidas++;
    return false;
  }
  memcpy(datos + ocupados, texto, largo);
  ocupados += largo;
  return true;
}

size_t BuferTexto::tomar(char *destino, size_t tam) {
  size_t cantidad = ocupados < tam ? ocupados : tam;
  if (cantidad == 0) {
    return 0;
  }
  memcpy(destino, datos, cantidad);
  memmove(datos, datos + cantidad, ocupados - cantidad);
  ocupados -= cantidad;
  return cantidad;
}

void textoTarjeta(EstadoTarjeta estado, uint16_t viaje, char *salida, size_t tam) {
  switch (estado) {
  case EstadoTarjeta::BUSCANDO:
    snprintf(salida, tam, "...");
    break;
  case EstadoTarjeta::SIN_TARJETA:
    snprintf(salida, tam, "Sin tarjeta");
    break;
  case EstadoTarjeta::ILEGIBLE:
    snprintf(salida, tam, "No se puede leer");
    break;
  case EstadoTarjeta::LISTA:
    snprintf(salida, tam, "Lista");
    break;
  case EstadoTarjeta::GRABANDO:
    snprintf(salida, tam, "Grabando VIAJE%04u", (unsigned)viaje);
    break;
  case EstadoTarjeta::ERROR:
    snprintf(salida, tam, "Error al escribir");
    break;
  case EstadoTarjeta::LLENA:
    snprintf(salida, tam, "Tarjeta llena");
    break;
  }
}

bool problemaTarjeta(EstadoTarjeta estado) {
  return estado == EstadoTarjeta::ILEGIBLE || estado == EstadoTarjeta::ERROR ||
         estado == EstadoTarjeta::LLENA;
}
