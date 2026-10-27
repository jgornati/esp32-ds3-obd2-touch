#include "guardados.h"

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "config.h"

static const char *const ESPACIO = "viajes";
static const char *const CLAVE_ULTIMO = "ultimo";

// "v0" a "v9": el lugar de cada número
static void clave(uint32_t numero, char *texto, size_t tam) {
  snprintf(texto, tam, "v%u", (unsigned)(numero % VIAJES_GUARDADOS));
}

void ViajesGuardados::iniciar() {
  Preferences preferencias;
  // Sin nada guardado todavía, el espacio no existe y no abre
  if (!preferencias.begin(ESPACIO, true)) {
    return;
  }
  ultimo = preferencias.getUInt(CLAVE_ULTIMO, 0);
  for (uint32_t n = ultimo; n > 0 && ultimo - n < VIAJES_GUARDADOS; n--) {
    char k[8];
    clave(n, k, sizeof(k));
    uint8_t bytes[LARGO_RESUMEN];
    size_t largo = preferencias.isKey(k) ? preferencias.getBytes(k, bytes, sizeof(bytes)) : 0;
    ResumenViaje r;
    // Si el de n no se llegó a escribir, en su lugar está uno de antes
    if (desempaquetar(bytes, largo, &r) && r.numero == n) {
      lista.agregarViejo(r);
    }
  }
  preferencias.end();
}

void ViajesGuardados::empezar(const FechaHora *desde) {
  numero = 0;
  conInicio = desde != nullptr;
  if (conInicio) {
    inicio = *desde;
  }
}

void ViajesGuardados::actualizar(const Viaje &viaje, uint32_t ahora) {
  if (viaje.tiempoEnMarchaMs() < MINIMO_VIAJE_GUARDADO_MS || (int32_t)(ahora - proximo) < 0) {
    return;
  }
  proximo = ahora + PERIODO_GUARDAR_VIAJE_MS;
  guardar(viaje);
}

void ViajesGuardados::terminar(const Viaje &viaje) {
  if (viaje.tiempoEnMarchaMs() < MINIMO_VIAJE_GUARDADO_MS) {
    return;
  }
  guardar(viaje);
  if (numero != 0) {
    lista.agregarNuevo(resumir(viaje, numero, conInicio ? &inicio : nullptr));
  }
}

// La primera vez le da el número siguiente al último; después lo escribe
// solo si cambió. false si no se pudo.
bool ViajesGuardados::guardar(const Viaje &viaje) {
  uint32_t n = numero != 0 ? numero : ultimo + 1;
  uint8_t bytes[LARGO_RESUMEN];
  empaquetar(resumir(viaje, n, conInicio ? &inicio : nullptr), bytes);
  if (numero != 0 && memcmp(bytes, escritos, sizeof(bytes)) == 0) {
    return true;
  }
  uint32_t desde = millis();
  Preferences preferencias;
  bool bien = preferencias.begin(ESPACIO, false);
  // Primero el número: si se corta antes de escribir el viaje, en su lugar
  // queda uno con otro número, que al leerlo se descarta
  if (bien && numero == 0) {
    bien = preferencias.putUInt(CLAVE_ULTIMO, n) == sizeof(uint32_t);
    if (bien) {
      ultimo = n;
      numero = n;
    }
  }
  if (bien) {
    char k[8];
    clave(n, k, sizeof(k));
    bien = preferencias.putBytes(k, bytes, sizeof(bytes)) == sizeof(bytes);
  }
  preferencias.end();
  if (!bien) {
    errores++;
    Serial.printf("Viajes: no se pudo guardar el %lu\n", (unsigned long)n);
    return false;
  }
  memcpy(escritos, bytes, sizeof(bytes));
  Serial.printf("Viajes: guardado el %lu (%.2f km, %lu s en marcha) en %lu ms\n", (unsigned long)n,
                viaje.distanciaKm(), (unsigned long)(viaje.tiempoEnMarchaMs() / 1000),
                (unsigned long)(millis() - desde));
  return true;
}

// ", vel. max 118", o nada si no lo tuvo
static void imprimirExtremo(const char *nombre, const Extremo &e, int decimales) {
  if (e.hay) {
    Serial.printf(", %s %.*f", nombre, decimales, e.valor);
  }
}

void ViajesGuardados::imprimir() const {
  Serial.printf("Viajes: %u anteriores; el de ahora ", lista.cantidad());
  if (numero != 0) {
    Serial.printf("es el %lu", (unsigned long)numero);
  } else {
    Serial.printf("todavia no se guardo (se guarda con %lu s en marcha)",
                  (unsigned long)(MINIMO_VIAJE_GUARDADO_MS / 1000));
  }
  Serial.printf("; %lu errores al guardar\n", (unsigned long)errores);
  for (uint8_t i = 0; i < lista.cantidad(); i++) {
    const ResumenViaje &r = lista.viaje(i);
    char fecha[24] = "sin fecha";
    if (r.conFecha) {
      textoInicio(r.inicio, fecha, sizeof(fecha));
    }
    Serial.printf("  %lu: %s, %.1f km, %lu min en marcha", (unsigned long)r.numero, fecha, r.km,
                  (unsigned long)(r.enMarchaMs / 60000));
    float kmh;
    if (r.velocidadMaxima.hay && promedio(r, &kmh)) {
      Serial.printf(", promedio %.0f km/h", kmh);
    }
    imprimirExtremo("vel. max", r.velocidadMaxima, 0);
    imprimirExtremo("RPM max", r.rpmMaxima, 0);
    imprimirExtremo("refrig. max", r.refrigeranteMaximo, 0);
    imprimirExtremo("volt. min", r.voltajeMinimo, 1);
    imprimirExtremo("volt. max", r.voltajeMaximo, 1);
    Serial.println();
  }
}
