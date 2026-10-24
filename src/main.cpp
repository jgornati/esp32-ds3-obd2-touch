#include <Arduino.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <esp_system.h>

#include "config.h"
#include "consola.h"
#include "explorar.h"

// E0: exploración del adaptador BLE desde el monitor serie. No usa la pantalla.

Explorador explorador;
Consola consola(explorador);

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

  Serial.printf("RAM interna libre: %u KB\n",
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10));
  Serial.printf("Ultimo reinicio: %s\n", motivoReinicio(esp_reset_reason()));
}

void setup() {
  // El panel se inicializa en E1. Con HIGH la retroiluminación igual queda prendida.
  pinMode(PIN_RETROILUMINACION, OUTPUT);
  digitalWrite(PIN_RETROILUMINACION, HIGH);

  Serial.begin(115200);
  uint32_t inicio = millis();
  while (!Serial && millis() - inicio < ESPERA_MONITOR_MS) {
    delay(10);
  }

  Serial.println();
  Serial.println("Monitor OBDII DS3 - E0: exploracion BLE");
  imprimirInfo();
  explorador.iniciar();
  Serial.println("Comandos: !escanear, !explorar N, !usar N [S], !desconectar, !info.");
  Serial.println("Con !usar, las lineas sin '!' van al adaptador.");
}

void loop() {
  consola.atender();
  if (consola.infoPedida()) {
    imprimirInfo();
  }
  explorador.atender();
  delay(1);
}
