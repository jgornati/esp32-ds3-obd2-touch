#include <Arduino.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <esp_system.h>
#include <lvgl.h>

#include "config.h"
#include "consola.h"
#include "explorar.h"
#include "pantalla_hw.h"

// E1: prueba de pantalla y touch. La consola y la exploración BLE de E0
// siguen andando.

Explorador explorador;
Consola consola(explorador);
PantallaHw pantalla;

// Colores que se piensan usar, sobre fondo negro (para la noche)
static const lv_color_t BLANCO = lv_color_hex(0xFFFFFF);
static const lv_color_t GRIS = lv_color_hex(0x707070);
static const lv_color_t VERDE = lv_color_hex(0x3CC864);
static const lv_color_t AMBAR = lv_color_hex(0xFFB000);
static const lv_color_t ROJO = lv_color_hex(0xFF3B30);
static const lv_color_t CIAN = lv_color_hex(0x40C8E0);
static const lv_color_t FONDO_BARRA = lv_color_hex(0x1A1A1A);

static const int ALTO_BARRA = 28;
static const uint32_t PERIODO_CONTADOR_MS = 100;
static const uint32_t PERIODO_LOG_TOQUE_MS = 150;

static lv_obj_t *textoToque = nullptr;
static lv_obj_t *textoBrillo = nullptr;
static lv_obj_t *contador = nullptr;
static lv_obj_t *marca = nullptr;

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

static void mostrarBrillo() {
  uint8_t nivel = pantalla.brillo();
  lv_label_set_text_fmt(textoBrillo, "Brillo %u", nivel);
  Serial.printf("Brillo: %u (duty %u)\n", nivel, RETROILUMINACION_INVERTIDA ? 255 - nivel : nivel);
}

static void alTocar(lv_event_t *e) {
  static uint32_t ultimoLog = 0;
  lv_event_code_t codigo = lv_event_get_code(e);
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  uint16_t crudoX;
  uint16_t crudoY;
  pantalla.toqueCrudo(crudoX, crudoY);

  if (codigo == LV_EVENT_PRESSED || codigo == LV_EVENT_PRESSING) {
    lv_obj_remove_flag(marca, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_pos(marca, p.x - lv_obj_get_width(marca) / 2, p.y - lv_obj_get_height(marca) / 2);
    lv_label_set_text_fmt(textoToque, "Toque x=%d y=%d", (int)p.x, (int)p.y);
    if (codigo == LV_EVENT_PRESSED || millis() - ultimoLog >= PERIODO_LOG_TOQUE_MS) {
      Serial.printf("Touch: x=%d y=%d (crudo %u, %u)\n", (int)p.x, (int)p.y, crudoX, crudoY);
      ultimoLog = millis();
    }
  } else if (codigo == LV_EVENT_RELEASED) {
    Serial.printf("Touch: suelto en x=%d y=%d\n", (int)p.x, (int)p.y);
  }
}

static void sinEstilo(lv_obj_t *obj) {
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *etiqueta(lv_obj_t *padre, const lv_font_t *fuente, lv_color_t color,
                          const char *texto) {
  lv_obj_t *e = lv_label_create(padre);
  lv_obj_set_style_text_font(e, fuente, 0);
  lv_obj_set_style_text_color(e, color, 0);
  lv_label_set_text(e, texto);
  return e;
}

static void crearPrueba() {
  lv_obj_t *pantallaLv = lv_screen_active();
  lv_obj_remove_flag(pantallaLv, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(pantallaLv, lv_color_black(), 0);
  lv_obj_add_event_cb(pantallaLv, alTocar, LV_EVENT_ALL, nullptr);

  // Barra de estado
  lv_obj_t *barra = lv_obj_create(pantallaLv);
  sinEstilo(barra);
  lv_obj_set_size(barra, 640, ALTO_BARRA);
  lv_obj_set_style_bg_color(barra, FONDO_BARRA, 0);
  lv_obj_set_style_bg_opa(barra, LV_OPA_COVER, 0);
  lv_obj_t *titulo = etiqueta(barra, &lv_font_montserrat_20, BLANCO, "PRUEBA E1  21,5 °C");
  lv_obj_align(titulo, LV_ALIGN_LEFT_MID, 8, 0);
  textoToque = etiqueta(barra, &lv_font_montserrat_20, CIAN, "Toque --");
  lv_obj_align(textoToque, LV_ALIGN_CENTER, 40, 0);
  textoBrillo = etiqueta(barra, &lv_font_montserrat_20, AMBAR, "");
  lv_obj_align(textoBrillo, LV_ALIGN_RIGHT_MID, -8, 0);
  mostrarBrillo();

  // Números grandes: un contador que cambia todo el tiempo, para ver si parpadea
  contador = etiqueta(pantallaLv, &fuente_numeros_110, BLANCO, "0");
  lv_obj_set_pos(contador, 8, ALTO_BARRA + 10);
  lv_obj_t *nota = etiqueta(pantallaLv, &lv_font_montserrat_14, GRIS, "contador de prueba");
  lv_obj_align(nota, LV_ALIGN_BOTTOM_LEFT, 8, -2);
  lv_obj_t *grados = etiqueta(pantallaLv, &fuente_numeros_110, VERDE, "92°");
  lv_obj_set_pos(grados, 300, ALTO_BARRA + 10);

  // Muestras de color
  struct Muestra {
    lv_color_t color;
    const char *nombre;
  };
  const Muestra muestras[] = {{BLANCO, "BLANCO"}, {GRIS, "GRIS"}, {VERDE, "VERDE"},
                              {AMBAR, "AMBAR"},   {ROJO, "ROJO"}, {CIAN, "CIAN"}};
  int y = ALTO_BARRA;
  for (const Muestra &m : muestras) {
    lv_obj_t *cuadro = lv_obj_create(pantallaLv);
    sinEstilo(cuadro);
    lv_obj_set_size(cuadro, 104, 24);
    lv_obj_set_pos(cuadro, 536, y);
    lv_obj_set_style_bg_color(cuadro, m.color, 0);
    lv_obj_set_style_bg_opa(cuadro, LV_OPA_COVER, 0);
    lv_obj_t *nombre = etiqueta(cuadro, &lv_font_montserrat_14, lv_color_black(), m.nombre);
    lv_obj_center(nombre);
    y += 24;
  }

  // Marca donde se toca
  marca = lv_obj_create(pantallaLv);
  sinEstilo(marca);
  lv_obj_set_size(marca, 30, 30);
  lv_obj_set_style_radius(marca, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(marca, AMBAR, 0);
  lv_obj_set_style_border_width(marca, 3, 0);
  lv_obj_add_flag(marca, LV_OBJ_FLAG_HIDDEN);
}

static void actualizarContador() {
  static uint32_t ultimo = 0;
  static int valor = 0;
  if (contador == nullptr || millis() - ultimo < PERIODO_CONTADOR_MS) {
    return;
  }
  ultimo = millis();
  valor = (valor + 37) % 10000;
  lv_label_set_text_fmt(contador, "%d", valor);
}

void setup() {
  Serial.begin(115200);
  uint32_t inicio = millis();
  while (!Serial && millis() - inicio < ESPERA_MONITOR_MS) {
    delay(10);
  }
  Serial.println();
  Serial.println("Monitor OBDII DS3 - E1: prueba de pantalla y touch");

  // La pantalla antes que el BLE: necesita RAM interna con DMA
  if (pantalla.iniciar()) {
    crearPrueba();
  } else {
    Serial.println("Pantalla: no se pudo iniciar");
  }
  explorador.iniciar();
  imprimirInfo();
  Serial.println("Comandos: + y - (brillo), !info, !escanear, !explorar N, !usar N [S], "
                 "!desconectar.");
}

void loop() {
  consola.atender();
  int ajuste = consola.ajusteBrillo();
  if (ajuste != 0 && pantalla.iniciada()) {
    int nivel = constrain(pantalla.brillo() + ajuste * PASO_BRILLO, 0, 255);
    pantalla.ponerBrillo(nivel);
    mostrarBrillo();
  }
  if (consola.infoPedida()) {
    imprimirInfo();
  }
  explorador.atender();
  actualizarContador();
  pantalla.atender();
  delay(1);
}
