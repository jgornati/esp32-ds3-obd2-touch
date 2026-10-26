#include "datos.h"

#include <stdio.h>
#include <string.h>

#include "config.h"

struct Definicion {
  uint8_t pid; // 0: ATRV
  const char *etiqueta;
  const char *unidad;
  uint8_t decimales;
};

// En el orden de Magnitud
static const Definicion DEFINICIONES[] = {
    {0x0C, "RPM", "", 0},       {0x05, "Refrig", "C", 0},   {0x0D, "Vel", "km/h", 0},
    {0x0F, "Admision", "C", 0}, {0x00, "Volt ELM", "V", 1}, {0x42, "Volt ECU", "V", 1},
    {0x06, "Aj corto", "%", 1}, {0x07, "Aj largo", "%", 1}, {0x03, "Lazo", "", 0},
    {0x34, "Lambda", "", 3},    {0x15, "O2 post", "V", 2},
};
static_assert(sizeof(DEFINICIONES) / sizeof(DEFINICIONES[0]) == CANT_MAGNITUDES,
              "DEFINICIONES tiene que seguir el orden de Magnitud");

static Dato datos[CANT_MAGNITUDES];

uint8_t pidDe(Magnitud m) {
  return DEFINICIONES[m].pid;
}

const char *etiquetaDe(Magnitud m) {
  return DEFINICIONES[m].etiqueta;
}

const Dato &dato(Magnitud m) {
  return datos[m];
}

void reiniciarDatos(const PidsSoportados &soportados) {
  for (uint8_t i = 0; i < CANT_MAGNITUDES; i++) {
    if (i == VOLT_ELM) {
      continue;
    }
    datos[i] = Dato();
    if (!soportados.soportado(DEFINICIONES[i].pid)) {
      datos[i].estado = EstadoDato::NO_SOPORTADO;
    }
  }
}

void marcarSinConexion() {
  for (Dato &d : datos) {
    d = Dato();
    d.estado = EstadoDato::SIN_CONEXION;
  }
}

void marcarConectado() {
  datos[VOLT_ELM] = Dato();
}

void actualizarDato(Magnitud m, float valor, uint32_t ahora) {
  datos[m].estado = EstadoDato::VALIDO;
  datos[m].valor = valor;
  datos[m].actualizadoMs = ahora;
}

bool valorDesdePid(Magnitud m, const uint8_t *datosPid, uint8_t largo, float *valor) {
  // El lazo es un estado, no un número: se guarda el byte del sistema 1
  if (m == LAZO) {
    if (largo < 1) {
      return false;
    }
    *valor = datosPid[0];
    return true;
  }
  return calcularPid(DEFINICIONES[m].pid, datosPid, largo, valor);
}

// textoLazo() abreviado, para que entre en la línea junto a "Lazo"
static const char *lazoCorto(uint8_t estado) {
  switch (estado) {
  case 0x01:
    return "Ab. frio";
  case 0x02:
    return "Cerrado";
  case 0x04:
    return "Ab. carga";
  case 0x08:
    return "Ab. falla";
  case 0x10:
    return "Ce. falla";
  }
  return nullptr;
}

bool valorActual(Magnitud m, uint32_t ahora, float *valor) {
  const Dato &d = datos[m];
  if (d.estado != EstadoDato::VALIDO || ahora - d.actualizadoMs > VIGENCIA_MS) {
    return false;
  }
  *valor = d.valor;
  return true;
}

void textoDato(Magnitud m, uint32_t ahora, bool pantalla, char *salida, size_t tam) {
  const Dato &d = datos[m];
  switch (d.estado) {
  case EstadoDato::NO_SOPORTADO:
    strlcpy(salida, "N/D", tam);
    return;
  case EstadoDato::ESPERANDO:
    strlcpy(salida, "...", tam);
    return;
  case EstadoDato::SIN_CONEXION:
    strlcpy(salida, "--", tam);
    return;
  case EstadoDato::VALIDO:
    break;
  }
  float valor;
  if (!valorActual(m, ahora, &valor)) {
    strlcpy(salida, "--", tam); // vencido
    return;
  }

  if (m == LAZO) {
    uint8_t estado = (uint8_t)valor;
    const char *texto = pantalla ? lazoCorto(estado) : textoLazo(estado);
    strlcpy(salida, estado == 0 ? "Apagado" : (texto != nullptr ? texto : "?"), tam);
    return;
  }

  const Definicion &def = DEFINICIONES[m];
  const char *unidad = def.unidad;
  if (pantalla && strcmp(unidad, "C") == 0) {
    unidad = "°C";
  }
  snprintf(salida, tam, "%.*f%s%s", def.decimales, valor, strcmp(unidad, "km/h") == 0 ? " " : "",
           unidad);
}
