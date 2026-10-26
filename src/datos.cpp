#include "datos.h"

#include <stdio.h>
#include <string.h>

#include "config.h"

struct Definicion {
  uint8_t pid; // 0: ATRV
  const char *etiqueta;
  const char *unidad;
  uint8_t decimales;
  bool conSigno; // "+1.6" en vez de "1.6"
};

// En el orden de Magnitud
static const Definicion DEFINICIONES[] = {
    {0x0C, "RPM", "", 0, false},        {0x05, "Refrig", "C", 0, false},
    {0x0D, "Vel", "km/h", 0, false},    {0x0F, "Admision", "C", 0, false},
    {0x00, "Volt ELM", "V", 1, false},  {0x42, "Volt ECU", "V", 1, false},
    {0x06, "Aj corto", "%", 1, true},   {0x07, "Aj largo", "%", 1, true},
    {0x03, "Lazo", "", 0, false},       {0x34, "Lambda", "", 3, false},
    {0x15, "O2 post", "V", 2, false},   {0x04, "Carga", "%", 0, false},
    {0x0E, "Avance", "grad", 0, false}, {0x11, "Acelerador", "%", 0, false},
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

bool valorActual(Magnitud m, uint32_t ahora, float *valor) {
  const Dato &d = datos[m];
  if (d.estado != EstadoDato::VALIDO || ahora - d.actualizadoMs > VIGENCIA_MS) {
    return false;
  }
  *valor = d.valor;
  return true;
}

// "N/D", "...", "--" (vencido o sin conexión), o nullptr con un valor actual
static const char *textoSinValor(Magnitud m, uint32_t ahora, float *valor) {
  switch (datos[m].estado) {
  case EstadoDato::NO_SOPORTADO:
    return "N/D";
  case EstadoDato::ESPERANDO:
    return "...";
  case EstadoDato::SIN_CONEXION:
    return "--";
  case EstadoDato::VALIDO:
    break;
  }
  return valorActual(m, ahora, valor) ? nullptr : "--";
}

// El número con sus decimales, o el estado del lazo
static void textoNumero(Magnitud m, float valor, char *salida, size_t tam) {
  if (m == LAZO) {
    uint8_t estado = (uint8_t)valor;
    const char *texto = textoLazo(estado);
    strlcpy(salida, estado == 0 ? "Apagado" : (texto != nullptr ? texto : "?"), tam);
    return;
  }
  const Definicion &def = DEFINICIONES[m];
  snprintf(salida, tam, def.conSigno ? "%+.*f" : "%.*f", def.decimales, valor);
}

void textoDato(Magnitud m, uint32_t ahora, bool pantalla, char *salida, size_t tam) {
  float valor;
  const char *sinValor = textoSinValor(m, ahora, &valor);
  if (sinValor != nullptr) {
    strlcpy(salida, sinValor, tam);
    return;
  }

  char numero[16];
  textoNumero(m, valor, numero, sizeof(numero));
  const char *unidad = DEFINICIONES[m].unidad;
  const char *separador = "";
  if (pantalla && strcmp(unidad, "C") == 0) {
    unidad = "°C";
  } else if (pantalla && strcmp(unidad, "grad") == 0) {
    unidad = "°";
  } else if (strcmp(unidad, "km/h") == 0 || strcmp(unidad, "grad") == 0) {
    separador = " ";
  }
  snprintf(salida, tam, "%s%s%s", numero, separador, unidad);
}

bool textoValor(Magnitud m, uint32_t ahora, char *salida, size_t tam) {
  float valor;
  const char *sinValor = textoSinValor(m, ahora, &valor);
  if (sinValor != nullptr) {
    strlcpy(salida, sinValor, tam);
    return false;
  }
  textoNumero(m, valor, salida, tam);
  return true;
}
