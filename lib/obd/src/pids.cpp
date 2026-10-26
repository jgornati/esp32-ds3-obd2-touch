#include "pids.h"

#include <stddef.h>

// Fórmulas de SAE J1979. A es datos[0], B es datos[1].

static float porcentaje(const uint8_t *d) {
  return d[0] * 100.0f / 255.0f;
}

static float temperatura(const uint8_t *d) {
  return d[0] - 40.0f;
}

static float ajusteCombustible(const uint8_t *d) {
  return (d[0] - 128) * 100.0f / 128.0f;
}

static float directo(const uint8_t *d) {
  return d[0];
}

static float rpm(const uint8_t *d) {
  return (d[0] * 256 + d[1]) / 4.0f;
}

static float flujoAire(const uint8_t *d) {
  return (d[0] * 256 + d[1]) / 100.0f;
}

static float tensionSonda(const uint8_t *d) {
  return d[0] / 200.0f;
}

// Relación de equivalencia (lambda)
static float lambda(const uint8_t *d) {
  return (d[0] * 256 + d[1]) * 2.0f / 65536.0f;
}

static float tensionModulo(const uint8_t *d) {
  return (d[0] * 256 + d[1]) / 1000.0f;
}

static const Pid TABLA[] = {
    {0x01, "Fallas", "", 4, nullptr}, // ver luzFalla()
    {0x03, "Lazo", "", 2, nullptr},   // ver textoLazo()
    {0x04, "Carga", "%", 1, porcentaje},
    {0x05, "Refrig", "C", 1, temperatura},
    {0x06, "Aj corto", "%", 1, ajusteCombustible},
    {0x07, "Aj largo", "%", 1, ajusteCombustible},
    {0x0B, "MAP", "kPa", 1, directo},
    {0x0C, "RPM", "rpm", 2, rpm},
    {0x0D, "Vel", "km/h", 1, directo},
    {0x0F, "Admision", "C", 1, temperatura},
    {0x10, "MAF", "g/s", 2, flujoAire},
    {0x11, "Acelerad", "%", 1, porcentaje},
    {0x13, "Sondas", "", 1, nullptr}, // ver sondaPresente()

    // Sondas de banda angosta: tensión (A). B es el ajuste asociado.
    {0x14, "O2 B1S1", "V", 2, tensionSonda},
    {0x15, "O2 B1S2", "V", 2, tensionSonda},
    {0x16, "O2 B1S3", "V", 2, tensionSonda},
    {0x17, "O2 B1S4", "V", 2, tensionSonda},
    {0x18, "O2 B2S1", "V", 2, tensionSonda},
    {0x19, "O2 B2S2", "V", 2, tensionSonda},
    {0x1A, "O2 B2S3", "V", 2, tensionSonda},
    {0x1B, "O2 B2S4", "V", 2, tensionSonda},

    // Sondas de banda ancha: lambda (A, B). C y D son la tensión.
    {0x24, "Lam B1S1", "", 4, lambda},
    {0x25, "Lam B1S2", "", 4, lambda},
    {0x26, "Lam B1S3", "", 4, lambda},
    {0x27, "Lam B1S4", "", 4, lambda},
    {0x28, "Lam B2S1", "", 4, lambda},
    {0x29, "Lam B2S2", "", 4, lambda},
    {0x2A, "Lam B2S3", "", 4, lambda},
    {0x2B, "Lam B2S4", "", 4, lambda},

    // Sondas de banda ancha: lambda (A, B). C y D son la corriente.
    {0x34, "Lam B1S1", "", 4, lambda},
    {0x35, "Lam B1S2", "", 4, lambda},
    {0x36, "Lam B1S3", "", 4, lambda},
    {0x37, "Lam B1S4", "", 4, lambda},
    {0x38, "Lam B2S1", "", 4, lambda},
    {0x39, "Lam B2S2", "", 4, lambda},
    {0x3A, "Lam B2S3", "", 4, lambda},
    {0x3B, "Lam B2S4", "", 4, lambda},

    {0x42, "V ECU", "V", 2, tensionModulo},
    {0x44, "Lam obj", "", 2, lambda},
};

const Pid *buscarPid(uint8_t pid) {
  for (const Pid &fila : TABLA) {
    if (fila.pid == pid) {
      return &fila;
    }
  }
  return nullptr;
}

bool calcularPid(uint8_t pid, const uint8_t *datos, uint8_t largo, float *valor) {
  const Pid *fila = buscarPid(pid);
  if (fila == nullptr || fila->formula == nullptr || datos == nullptr || valor == nullptr ||
      largo < fila->bytes) {
    return false;
  }
  *valor = fila->formula(datos);
  return true;
}

uint8_t separarPids(const uint8_t *datos, uint8_t largo, DatoPid *salida, uint8_t max) {
  if (datos == nullptr || salida == nullptr) {
    return 0;
  }
  uint8_t cantidad = 0;
  uint8_t i = 0;
  while (i < largo && cantidad < max) {
    const Pid *fila = buscarPid(datos[i]);
    if (fila == nullptr || largo - i - 1 < fila->bytes) {
      break;
    }
    salida[cantidad++] = {datos[i], datos + i + 1, fila->bytes};
    i += 1 + fila->bytes;
  }
  return cantidad;
}

bool luzFalla(const uint8_t *datos, uint8_t largo, bool *encendida, uint8_t *cantidad) {
  if (datos == nullptr || largo < 4) {
    return false;
  }
  if (encendida != nullptr) {
    *encendida = (datos[0] & 0x80) != 0;
  }
  if (cantidad != nullptr) {
    *cantidad = datos[0] & 0x7F;
  }
  return true;
}

const char *textoLazo(uint8_t estado) {
  switch (estado) {
  case 0x01:
    return "Abierto frio";
  case 0x02:
    return "Cerrado";
  case 0x04:
    return "Abierto carga"; // por carga o por corte en desaceleración
  case 0x08:
    return "Abierto falla";
  case 0x10:
    return "Cerrado falla"; // falla en alguna sonda
  }
  return nullptr;
}

bool sondaPresente(uint8_t mapa, uint8_t banco, uint8_t sensor) {
  if (banco < 1 || banco > 2 || sensor < 1 || sensor > 4) {
    return false;
  }
  return (mapa >> ((banco - 1) * 4 + (sensor - 1))) & 1;
}

void PidsSoportados::limpiar() {
  for (uint32_t &mapa : mapas) {
    mapa = 0;
  }
}

bool PidsSoportados::cargar(uint8_t base, const uint8_t *datos, uint8_t largo) {
  if (base % 0x20 != 0 || datos == nullptr || largo < 4) {
    return false;
  }
  mapas[base / 0x20] =
      (uint32_t)datos[0] << 24 | (uint32_t)datos[1] << 16 | (uint32_t)datos[2] << 8 | datos[3];
  return true;
}

// En cada mapa, el bit más alto es base + 1 y el más bajo base + 0x20.
bool PidsSoportados::soportado(uint8_t pid) const {
  if (pid == 0) {
    return true;
  }
  uint8_t indice = (pid - 1) / 0x20;
  uint8_t bit = 31 - (pid - 1) % 0x20;
  return (mapas[indice] >> bit) & 1;
}

uint8_t PidsSoportados::cantidad() const {
  uint8_t total = 0;
  for (uint32_t mapa : mapas) {
    for (uint8_t bit = 1; bit < 32; bit++) {
      total += (mapa >> bit) & 1;
    }
  }
  return total;
}
