#include <Arduino.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <esp_system.h>
#include <lvgl.h>
#include <respuesta.h>

#include "config.h"
#include "consola.h"
#include "elm327.h"
#include "explorar.h"
#include "pantalla_hw.h"

// E2: banco del transporte BLE, como E1 del anterior. Conecta, inicializa el
// ELM (ATZ, ATE0, ATI, ATRV), lee ATRV cada PERIODO_ATRV_MS y reconecta solo.
// La pantalla muestra solo el estado de la conexión: las pantallas son de E3.

Elm327 elm;
Explorador explorador;
Consola consola(elm, explorador);
PantallaHw pantalla;

static lv_obj_t *textoEstado = nullptr;

static bool estabaConectado = false;
static bool elmIniciado = false;
static uint32_t proximoIntento = 0;
static uint32_t proximaInicializacion = 0;
static uint32_t proximoAtrv = 0;

static const char *motivoReinicio(esp_reset_reason_t motivo) {
  switch (motivo) {
  case ESP_RST_POWERON:
    return "encendido";
  case ESP_RST_EXT:
    return "pin externo";
  case ESP_RST_SW:
    return "por software";
  case ESP_RST_PANIC:
    return "excepcion";
  case ESP_RST_INT_WDT:
    return "watchdog de interrupciones";
  case ESP_RST_TASK_WDT:
    return "watchdog de tareas";
  case ESP_RST_WDT:
    return "otro watchdog";
  case ESP_RST_DEEPSLEEP:
    return "salida de deep sleep";
  case ESP_RST_BROWNOUT:
    return "brownout (baja tension)";
  case ESP_RST_USB:
    return "USB";
  case ESP_RST_JTAG:
    return "JTAG";
  case ESP_RST_PWR_GLITCH:
    return "falla de alimentacion";
  case ESP_RST_CPU_LOCKUP:
    return "CPU bloqueada";
  default:
    return "desconocido";
  }
}

static void imprimirInfo() {
  Serial.printf("Chip: %s rev %u, %u nucleos, %lu MHz\n", ESP.getChipModel(),
                (unsigned)ESP.getChipRevision(), (unsigned)ESP.getChipCores(),
                (unsigned long)ESP.getCpuFreqMHz());

  // El tamaño del chip, no el del heap (que es un poco menor)
  size_t psram = esp_psram_get_size();
  if (psram > 0) {
    Serial.printf("PSRAM: %u MB (libres %u KB)\n", (unsigned)(psram >> 20),
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10));
  } else {
    Serial.println("PSRAM: NO");
  }

  // La física la lee del chip; la configurada es la de platformio.ini
  uint32_t fisica = 0;
  esp_flash_get_physical_size(nullptr, &fisica);
  Serial.printf("Flash: %lu MB (configurada %lu MB, %lu MHz)\n", (unsigned long)(fisica >> 20),
                (unsigned long)(ESP.getFlashChipSize() >> 20),
                (unsigned long)(ESP.getFlashChipSpeed() / 1000000));

  Serial.printf("RAM interna libre: %u KB (bloque mayor %u KB)\n",
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10),
                (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >> 10));
  Serial.printf("Ultimo reinicio: %s\n", motivoReinicio(esp_reset_reason()));

  if (pantalla.iniciada()) {
    uint32_t envio;
    uint32_t total = pantalla.medirRefresco(envio);
    Serial.printf("Pantalla: refresco completo %lu ms (envio al panel %lu ms), brillo %u\n",
                  (unsigned long)total, (unsigned long)envio, pantalla.brillo());
  }
}

static void mostrarEstado(const char *texto) {
  if (textoEstado != nullptr) {
    lv_label_set_text(textoEstado, texto);
    pantalla.atender();
  }
}

static void crearPantalla() {
  lv_obj_t *pantallaLv = lv_screen_active();
  lv_obj_remove_flag(pantallaLv, LV_OBJ_FLAG_SCROLLABLE);
  textoEstado = lv_label_create(pantallaLv);
  lv_obj_set_style_text_color(textoEstado, lv_color_hex(0x707070), 0);
  lv_obj_center(textoEstado);
}

static void alAvisar(FaseConexion fase) {
  mostrarEstado(fase == FaseConexion::POR_DIRECCION ? "E2: conectando"
                                                    : "E2: buscando el adaptador");
}

static void alEsperar() {
  pantalla.atender();
}

static void leerVoltaje(bool registrar) {
  ResultadoComando r = elm.enviar("ATRV", TIMEOUT_COMANDO_MS, registrar);
  float voltios;
  if (registrar && r.estado == EstadoTransporte::OK &&
      interpretarVoltaje(r.cruda.c_str(), &voltios)) {
    Serial.printf("ELM: %.1f V\n", voltios);
  }
}

// ATZ reinicia el ELM, que vuelve con el eco prendido
static bool inicializarElm() {
  if (elm.enviar("ATZ", TIMEOUT_ATZ_MS).estado != EstadoTransporte::OK) {
    return false;
  }
  elm.enviar("ATE0", TIMEOUT_COMANDO_MS);
  ResultadoComando r = elm.enviar("ATI", TIMEOUT_COMANDO_MS);
  char version[24];
  if (r.estado == EstadoTransporte::OK &&
      textoRespuesta(r.cruda.c_str(), "ATI", version, sizeof(version))) {
    Serial.printf("ELM: version %s\n", version);
  }
  leerVoltaje(true);
  return true;
}

void setup() {
  Serial.begin(115200);
  uint32_t inicio = millis();
  while (!Serial && millis() - inicio < ESPERA_MONITOR_MS) {
    delay(10);
  }
  Serial.println();
  Serial.println("Monitor OBDII DS3 - E2: banco del transporte BLE");

  // La pantalla antes que el BLE: necesita RAM interna con DMA
  if (pantalla.iniciar()) {
    crearPantalla();
  } else {
    Serial.println("Pantalla: no se pudo iniciar");
  }
  elm.iniciar();
  elm.mientrasEspera(alEsperar);
  imprimirInfo();
  Serial.println("Escribi un comando para el ELM, !reconectar, !olvidar, !log, !info, "
                 "!escanear o !explorar N.");
}

void loop() {
  consola.atender();
  if (consola.olvidoPedido()) {
    elm.olvidar();
    proximoIntento = millis();
  }
  if (consola.reconexionPedida()) {
    elm.desconectar();
    proximoIntento = millis();
  }
  int ajuste = consola.ajusteBrillo();
  if (ajuste != 0 && pantalla.iniciada()) {
    pantalla.ponerBrillo(constrain(pantalla.brillo() + ajuste * PASO_BRILLO, 0, 255));
    Serial.printf("Brillo: %u\n", pantalla.brillo());
  }
  if (consola.infoPedida()) {
    imprimirInfo();
  }

  if (!elm.conectado()) {
    if (estabaConectado) {
      Serial.println("ELM: sin conexion");
      estabaConectado = false;
      elmIniciado = false;
    }
    if ((int32_t)(millis() - proximoIntento) >= 0) {
      uint32_t inicio = millis();
      if (elm.conectar(alAvisar)) {
        Serial.printf("ELM: conectado en %lu ms\n", (unsigned long)(millis() - inicio));
        mostrarEstado("E2: conectado");
        estabaConectado = true;
        proximaInicializacion = millis();
      } else {
        Serial.printf("ELM: no conecto (%lu ms); reintenta en %lu s\n",
                      (unsigned long)(millis() - inicio), (unsigned long)(REINTENTO_BT_MS / 1000));
        mostrarEstado("E2: sin conexion");
        proximoIntento = millis() + REINTENTO_BT_MS;
      }
    }
  } else if (!elmIniciado) {
    if ((int32_t)(millis() - proximaInicializacion) >= 0) {
      elmIniciado = inicializarElm();
      if (!elmIniciado) {
        Serial.printf("ELM: no contesta; reintenta en %lu s\n",
                      (unsigned long)(REINTENTO_ELM_MS / 1000));
        proximaInicializacion = millis() + REINTENTO_ELM_MS;
      }
      proximoAtrv = millis() + PERIODO_ATRV_MS;
    }
  } else if ((int32_t)(millis() - proximoAtrv) >= 0) {
    // Callado salvo que falle o que !log pida el detalle
    leerVoltaje(consola.detalle());
    proximoAtrv = millis() + PERIODO_ATRV_MS;
  }

  pantalla.atender();
  delay(1);
}
