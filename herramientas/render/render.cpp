// Render de las pantallas en la PC, con el LVGL y el código de pantalla,
// paginas y datos del firmware. PantallaHw es de mentira: la imagen va a un
// búfer y de ahí a PNG (al doble de tamaño). Además revisa que dibujar dos
// veces lo mismo no deje nada para redibujar (en la placa, cada redibujo
// cuesta ~33 ms de envío) y que el toque cambie de página como corresponde.
// Lo arma el Makefile; sale con 1 si alguna revisión falla.

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <vector>

#include <zlib.h>
#ifdef __APPLE__
#include <malloc/malloc.h>
#endif

#include <display/lv_display_private.h>
#include <lvgl.h>

#include "datos.h"
#include "paginas.h"
#include "pantalla.h"
#include "pantalla_hw.h"

static const int ANCHO = 640;
static const int ALTO = 172;
static const int ESCALA = 2;
static const int SEPARACION = 6; // entre pantallas en todas.png

static uint32_t reloj = 100000;
static uint16_t marco[ANCHO * ALTO];
static uint8_t bufer[ANCHO * ALTO * 2];
static std::vector<std::vector<uint16_t>> guardadas;
static int fallas = 0;

// Lo que usa Pantalla
bool PantallaHw::iniciada() const {
  return true;
}

void PantallaHw::dibujarYa() {
  lv_refr_now(nullptr);
}

static uint32_t tick() {
  return reloj;
}

static void volcar(lv_display_t *display, const lv_area_t *area, uint8_t *pixeles) {
  const uint16_t *p = (const uint16_t *)pixeles;
  for (int y = area->y1; y <= area->y2; y++) {
    for (int x = area->x1; x <= area->x2; x++) {
      marco[y * ANCHO + x] = *p++;
    }
  }
  lv_display_flush_ready(display);
}

static void bloquePng(FILE *f, const char *tipo, const uint8_t *datos, uint32_t largo) {
  uint8_t numero[4] = {(uint8_t)(largo >> 24), (uint8_t)(largo >> 16), (uint8_t)(largo >> 8),
                       (uint8_t)largo};
  fwrite(numero, 1, 4, f);
  fwrite(tipo, 1, 4, f);
  fwrite(datos, 1, largo, f);
  uLong crc = crc32(crc32(0, (const Bytef *)tipo, 4), datos, largo);
  uint8_t control[4] = {(uint8_t)(crc >> 24), (uint8_t)(crc >> 16), (uint8_t)(crc >> 8),
                        (uint8_t)crc};
  fwrite(control, 1, 4, f);
}

// RGB565 de LVGL a PNG RGB, cada píxel en un cuadrado de ESCALA
static void escribirPng(const char *ruta, const uint16_t *pixeles, int ancho, int alto) {
  int anchoPng = ancho * ESCALA;
  int altoPng = alto * ESCALA;
  std::vector<uint8_t> crudo;
  crudo.reserve((size_t)altoPng * (anchoPng * 3 + 1));
  for (int y = 0; y < altoPng; y++) {
    crudo.push_back(0); // sin filtro
    for (int x = 0; x < anchoPng; x++) {
      uint16_t c = pixeles[(y / ESCALA) * ancho + x / ESCALA];
      crudo.push_back(((c >> 11) & 0x1F) * 255 / 31);
      crudo.push_back(((c >> 5) & 0x3F) * 255 / 63);
      crudo.push_back((c & 0x1F) * 255 / 31);
    }
  }
  uLongf largo = compressBound(crudo.size());
  std::vector<uint8_t> comprimido(largo);
  compress2(comprimido.data(), &largo, crudo.data(), crudo.size(), 9);

  FILE *f = fopen(ruta, "wb");
  if (f == nullptr) {
    printf("No se pudo escribir %s\n", ruta);
    fallas++;
    return;
  }
  fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
  uint8_t encabezado[13] = {(uint8_t)(anchoPng >> 24),
                            (uint8_t)(anchoPng >> 16),
                            (uint8_t)(anchoPng >> 8),
                            (uint8_t)anchoPng,
                            (uint8_t)(altoPng >> 24),
                            (uint8_t)(altoPng >> 16),
                            (uint8_t)(altoPng >> 8),
                            (uint8_t)altoPng,
                            8,
                            2,
                            0,
                            0,
                            0};
  bloquePng(f, "IHDR", encabezado, sizeof(encabezado));
  bloquePng(f, "IDAT", comprimido.data(), largo);
  bloquePng(f, "IEND", nullptr, 0);
  fclose(f);
}

static void guardar(const char *directorio, const char *nombre) {
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(nullptr);
  char ruta[512];
  snprintf(ruta, sizeof(ruta), "%s/%s.png", directorio, nombre);
  escribirPng(ruta, marco, ANCHO, ALTO);
  guardadas.emplace_back(marco, marco + ANCHO * ALTO);
}

// Todas una debajo de otra, separadas por una franja gris
static void guardarTodas(const char *directorio) {
  int cantidad = guardadas.size();
  int alto = cantidad * ALTO + (cantidad - 1) * SEPARACION;
  std::vector<uint16_t> hoja((size_t)ANCHO * alto, 0x632C);
  for (int i = 0; i < cantidad; i++) {
    memcpy(&hoja[(size_t)i * (ALTO + SEPARACION) * ANCHO], guardadas[i].data(),
           guardadas[i].size() * 2);
  }
  char ruta[512];
  snprintf(ruta, sizeof(ruta), "%s/todas.png", directorio);
  escribirPng(ruta, hoja.data(), ANCHO, alto);
}

static void revisar(bool bien, const char *que) {
  printf("%s %s\n", bien ? "OK   " : "FALLA", que);
  if (!bien) {
    fallas++;
  }
}

static void irA(Paginas &paginas, const char *titulo) {
  for (int i = 0; i < MAX_PAGINAS && strcmp(paginas.titulo(), titulo) != 0; i++) {
    paginas.cambiar(1);
  }
}

// Los PIDs del DS3 (E3) y los valores de la maqueta de E4
static void datosDeLaMaqueta() {
  PidsSoportados soportados;
  const uint8_t mapa00[] = {0xBE, 0x1E, 0xA8, 0x11};
  const uint8_t mapa20[] = {0x80, 0x00, 0x10, 0x01};
  const uint8_t mapa40[] = {0x02, 0x00, 0x00, 0x00};
  soportados.cargar(0x00, mapa00, 4);
  soportados.cargar(0x20, mapa20, 4);
  soportados.cargar(0x40, mapa40, 4);
  reiniciarDatos(soportados);
  actualizarDato(RPM, 2140, reloj);
  actualizarDato(REFRIGERANTE, 89, reloj);
  actualizarDato(VELOCIDAD, 62, reloj);
  actualizarDato(ADMISION, 31, reloj);
  actualizarDato(VOLT_ELM, 14.1f, reloj);
  actualizarDato(CARGA, 23, reloj);
  actualizarDato(AVANCE, 12, reloj);
  actualizarDato(ACELERADOR, 14, reloj);
  actualizarDato(LAZO, 2, reloj);
  actualizarDato(AJ_CORTO, 1.5625f, reloj);
  actualizarDato(AJ_LARGO, -0.78f, reloj);
  actualizarDato(LAMBDA, 0.995f, reloj);
  actualizarDato(SONDA_POST, 0.46f, reloj);
}

// Touch de mentira: un toque es apoyar y levantar el dedo en x, y
static lv_point_t dedo;
static bool apoyado = false;

static void leerDedo(lv_indev_t *, lv_indev_data_t *datos) {
  datos->point = dedo;
  datos->state = apoyado ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static int8_t tocar(lv_indev_t *indev, Paginas &paginas, int32_t x, int32_t y) {
  dedo = {x, y};
  apoyado = true;
  lv_indev_read(indev);
  reloj += 80;
  apoyado = false;
  lv_indev_read(indev);
  reloj += 80;
  return paginas.tomarToque();
}

static size_t memoriaEnUso() {
#ifdef __APPLE__
  malloc_statistics_t estadisticas;
  malloc_zone_statistics(nullptr, &estadisticas);
  return estadisticas.size_in_use;
#else
  return 0;
#endif
}

int main(int argc, char **argv) {
  const char *directorio = argc > 1 ? argv[1] : ".";
  lv_init();
  lv_tick_set_cb(tick);
  lv_display_t *display = lv_display_create(ANCHO, ALTO);
  lv_display_set_flush_cb(display, volcar);
  lv_display_set_buffers(display, bufer, nullptr, sizeof(bufer), LV_DISPLAY_RENDER_MODE_FULL);
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, leerDedo);

  PantallaHw hw;
  Pantalla pantalla(hw);
  pantalla.iniciar();
  Paginas paginas(pantalla);
  // Lo que piden las páginas armadas y dibujadas una vez
  datosDeLaMaqueta();
  InfoConexion info = {"v2.1", "6", 17, 6};
  size_t antes = memoriaEnUso();
  paginas.iniciar();
  for (uint8_t i = 0; i < 6; i++) {
    paginas.dibujar(reloj, info);
    lv_refr_now(nullptr);
    paginas.cambiar(1);
  }
  size_t usada = memoriaEnUso() - antes;
  if (usada > 0) {
    printf("Paginas: %zu bytes de malloc en la PC (con punteros de 64 bits)\n", usada);
  }

  pantalla.sinEcu("v2.1", {"", "12.0V", true}, "No responde", 7);
  guardar(directorio, "0-sin-ecu");
  revisar(tocar(indev, paginas, 500, 100) == 0,
          "con SIN ECU a la vista, tocar no cambia de pagina");

  for (uint8_t i = 1; i <= 6; i++) {
    paginas.dibujar(reloj, info);
    char nombre[32];
    snprintf(nombre, sizeof(nombre), "%u-%s", i, paginas.titulo());
    for (char *c = nombre; *c != '\0'; c++) {
      *c = *c == '/' ? '-' : (char)tolower(*c);
    }
    guardar(directorio, nombre);
    paginas.dibujar(reloj, info);
    char que[64];
    snprintf(que, sizeof(que), "%s: dibujar de nuevo lo mismo no redibuja nada", paginas.titulo());
    revisar(display->inv_p == 0, que);
    paginas.cambiar(1);
  }

  // Valores extremos
  actualizarDato(RPM, 6500, reloj);
  actualizarDato(REFRIGERANTE, -12, reloj);
  actualizarDato(VELOCIDAD, 188, reloj);
  actualizarDato(AVANCE, -6.5f, reloj);
  actualizarDato(CARGA, 100, reloj);
  actualizarDato(ACELERADOR, 0, reloj);
  actualizarDato(LAZO, 4, reloj);
  actualizarDato(AJ_CORTO, -12.5f, reloj);
  actualizarDato(LAMBDA, 0.91f, reloj);
  irA(paginas, "MOTOR");
  paginas.dibujar(reloj, info);
  guardar(directorio, "7-motor-extremos");
  irA(paginas, "CARGA");
  paginas.dibujar(reloj, info);
  guardar(directorio, "8-carga-extremos");
  irA(paginas, "MEZCLA");
  paginas.dibujar(reloj, info);
  guardar(directorio, "9-mezcla-extremos");

  // Vencidos: "--" en gris, sin barras
  reloj += 5000;
  irA(paginas, "MOTOR");
  paginas.dibujar(reloj, info);
  guardar(directorio, "a-motor-vencido");
  irA(paginas, "MEZCLA");
  paginas.dibujar(reloj, info);
  guardar(directorio, "b-mezcla-vencido");

  // Toques con las páginas a la vista
  revisar(tocar(indev, paginas, 100, 100) == -1, "tocar a la izquierda: anterior");
  revisar(tocar(indev, paginas, 500, 100) == 1, "tocar a la derecha: siguiente");
  revisar(tocar(indev, paginas, 20, 10) == -1, "tocar la barra a la izquierda: anterior");
  revisar(tocar(indev, paginas, 600, 10) == 1, "tocar la barra a la derecha: siguiente");
  revisar(paginas.tomarToque() == 0, "sin tocar no hay pedido");

  guardarTodas(directorio);
  printf("PNG en %s\n", directorio);
  return fallas > 0 ? 1 : 0;
}
