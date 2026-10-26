#include "descubrimiento.h"

#include <dtc.h>
#include <pids.h>
#include <respuesta.h>

// Todos de lectura y de la lista blanca. No se pide el VIN (0902) para
// que no quede en el log, que se guarda en el repositorio.
static const char *const PASOS[] = {
    "ATI",
    "AT@1",
    "ATRV",
    "ATDPN",
    // Con encabezados se ve qué ECUs contestan y con qué ID
    "ATH1",
    "0100",
    "ATH0",
    // Mapas de PIDs soportados
    "0100",
    "0120",
    "0140",
    "0160",
    "ATDPN",
    // Datos
    "0101",
    "0103",
    "0105",
    "0106",
    "0107",
    "010C",
    "010D",
    "010F",
    "0113",
    "0114",
    "0115",
    "0124",
    "0134",
    "0142",
    "0144",
    // Códigos de falla e información del vehículo
    "03",
    "07",
    "0A",
    "0900",
};

static const uint8_t CANTIDAD_PASOS = sizeof(PASOS) / sizeof(PASOS[0]);
// Velocidad con y sin el sufijo de cantidad de respuestas, y con varios
// PIDs en un pedido (E3: RPM, velocidad, refrigerante y admisión)
static const char *const MEDICIONES[] = {"010C", "010C1", "010C0D050F"};
static const uint8_t CANTIDAD_MEDICIONES = sizeof(MEDICIONES) / sizeof(MEDICIONES[0]);
static const uint8_t REPETICIONES = 20;
static const uint16_t TOTAL = CANTIDAD_PASOS + CANTIDAD_MEDICIONES * REPETICIONES;

// Más largo que el normal: acá importa ver la respuesta, no la velocidad
static const uint32_t TIMEOUT_MS = 3000;

static void imprimirBytes(const uint8_t *datos, uint8_t largo) {
  for (uint8_t i = 0; i < largo; i++) {
    Serial.printf(" %02X", datos[i]);
  }
}

static void explicarPid(uint8_t pid, const Respuesta &r, PidsSoportados &soportados) {
  if (pid % 0x20 == 0) {
    soportados.cargar(pid, r.datos, r.largo);
    Serial.print("   soportados:");
    for (int otro = pid + 1; otro <= pid + 0x20 && otro <= 0xFF; otro++) {
      if (soportados.soportado((uint8_t)otro)) {
        Serial.printf(" %02X", otro);
      }
    }
    Serial.println();
    return;
  }

  if (pid == 0x01) {
    bool encendida;
    uint8_t cantidad;
    if (luzFalla(r.datos, r.largo, &encendida, &cantidad)) {
      Serial.printf("   luz de falla: %s, codigos guardados: %u\n", encendida ? "SI" : "NO",
                    cantidad);
    }
    return;
  }
  if (pid == 0x03 && r.largo >= 2) {
    const char *uno = textoLazo(r.datos[0]);
    const char *dos = textoLazo(r.datos[1]);
    Serial.printf("   lazo: sistema 1 %s, sistema 2 %s\n", uno != nullptr ? uno : "?",
                  dos != nullptr ? dos : "no hay");
    return;
  }
  if (pid == 0x13 && r.largo >= 1) {
    Serial.print("   sondas:");
    for (uint8_t banco = 1; banco <= 2; banco++) {
      for (uint8_t sensor = 1; sensor <= 4; sensor++) {
        if (sondaPresente(r.datos[0], banco, sensor)) {
          Serial.printf(" B%uS%u", banco, sensor);
        }
      }
    }
    Serial.println();
    return;
  }

  const Pid *fila = buscarPid(pid);
  float valor;
  if (fila != nullptr && calcularPid(pid, r.datos, r.largo, &valor)) {
    Serial.printf("   %s: %.2f %s\n", fila->nombre, valor, fila->unidad);
  }
}

static void explicarDtcs(const Respuesta &r) {
  ListaDtc lista;
  if (!interpretarDtcs(r.datos, r.largo, &lista)) {
    Serial.println("   codigos: no se pudo leer la lista");
    return;
  }
  Serial.printf("   codigos: %u", lista.cantidad);
  for (uint8_t i = 0; i < lista.cantidad; i++) {
    char texto[6];
    textoDtc(lista.codigos[i], texto);
    Serial.printf(" %s", texto);
  }
  Serial.println();
}

static void explicar(const char *comando, const ResultadoComando &r, bool encabezados,
                     PidsSoportados &soportados) {
  if (strncmp(comando, "AT", 2) == 0) {
    char texto[40];
    if (r.estado == EstadoTransporte::OK &&
        textoRespuesta(r.cruda.c_str(), comando, texto, sizeof(texto))) {
      Serial.printf("   = \"%s\"\n", texto);
    }
    return;
  }

  Respuesta respuesta = interpretar(r.cruda.c_str(), comando);
  if (encabezados) {
    Serial.printf("   = %s (con encabezados se analiza en crudo)\n", nombreTipo(respuesta.tipo));
    return;
  }
  Serial.printf("   = %s", nombreTipo(respuesta.tipo));
  if (respuesta.tipo != TipoRespuesta::DATOS) {
    Serial.println();
    return;
  }
  Serial.printf(" x%u:", respuesta.respuestas);
  imprimirBytes(respuesta.datos, respuesta.largo);
  Serial.println();

  if (strncmp(comando, "01", 2) == 0) {
    uint8_t pid = (uint8_t)strtol(comando + 2, nullptr, 16);
    explicarPid(pid, respuesta, soportados);
  } else if (strcmp(comando, "03") == 0 || strcmp(comando, "07") == 0 ||
             strcmp(comando, "0A") == 0) {
    explicarDtcs(respuesta);
  }
}

bool descubrir(Elm327 &elm, void (*progreso)(uint16_t hechos, uint16_t total)) {
  uint32_t inicio = millis();
  uint16_t hechos = 0;
  bool encabezados = false;
  PidsSoportados soportados;

  Serial.println("=== DESCUBRIR: inicio");
  for (const char *comando : PASOS) {
    if (progreso != nullptr) {
      progreso(hechos, TOTAL);
    }
    hechos++;
    ResultadoComando r = elm.enviar(comando, TIMEOUT_MS);
    if (r.estado == EstadoTransporte::DESCONECTADO) {
      Serial.println("=== DESCUBRIR: se perdio la conexion, queda incompleto");
      return false;
    }
    if (strcmp(comando, "ATH1") == 0) {
      encabezados = true;
    } else if (strcmp(comando, "ATH0") == 0) {
      encabezados = false;
    }
    explicar(comando, r, encabezados, soportados);
  }

  for (const char *comando : MEDICIONES) {
    uint32_t total = 0;
    uint8_t conDatos = 0;
    for (uint8_t i = 0; i < REPETICIONES; i++) {
      if (progreso != nullptr) {
        progreso(hechos, TOTAL);
      }
      hechos++;
      ResultadoComando r = elm.enviar(comando, TIMEOUT_MS);
      if (r.estado == EstadoTransporte::DESCONECTADO) {
        Serial.println("=== DESCUBRIR: se perdio la conexion, queda incompleto");
        return false;
      }
      total += r.ms;
      if (r.estado == EstadoTransporte::OK &&
          interpretar(r.cruda.c_str(), comando).tipo == TipoRespuesta::DATOS) {
        conDatos++;
      }
    }
    Serial.printf("=== %s: %u de %u con datos, promedio %lu ms\n", comando, conDatos, REPETICIONES,
                  (unsigned long)(total / REPETICIONES));
  }

  Serial.printf("=== DESCUBRIR: fin en %lu ms, %u PIDs soportados en 01\n",
                (unsigned long)(millis() - inicio), soportados.cantidad());
  return true;
}
