// Render de las pantallas en la PC, con el LVGL y el código de pantalla,
// paginas y datos del firmware. PantallaHw es de mentira: la imagen va a un
// búfer y de ahí a PNG (al doble de tamaño). Además revisa que dibujar dos
// veces lo mismo no deje nada para redibujar (en la placa, cada redibujo
// cuesta ~33 ms de envío), que el toque cambie de página como corresponde,
// que los botones de Info pidan lo suyo sin cambiar de página y que los
// textos de Fallas e Info no se pasen de su columna.
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

#include <dtc.h>
#include <respuesta.h>

#include "config.h"
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
static int errores = 0;

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
    errores++;
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

// Dibuja la pantalla entera y devuelve una copia
static std::vector<uint16_t> capturar() {
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(nullptr);
  return std::vector<uint16_t>(marco, marco + ANCHO * ALTO);
}

static void guardar(const char *directorio, const char *nombre) {
  guardadas.push_back(capturar());
  char ruta[512];
  snprintf(ruta, sizeof(ruta), "%s/%s.png", directorio, nombre);
  escribirPng(ruta, marco, ANCHO, ALTO);
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
    errores++;
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

// Una lectura de la página Fallas con las respuestas crudas de 01011, 03, 07 y 0A
static LecturaFallas lecturaFallas(const char *luz, const char *guardados, const char *pendientes,
                                   const char *permanentes) {
  LecturaFallas l;
  l.leida = true;
  Respuesta r = interpretar(luz, "01011");
  l.luzLeida = r.tipo == TipoRespuesta::DATOS && luzFalla(r.datos, r.largo, &l.luz, &l.cantidadLuz);
  l.guardados = interpretarCodigos(guardados, "03", true);
  l.pendientes = interpretarCodigos(pendientes, "07", true);
  l.permanentes = interpretarCodigos(permanentes, "0A", true);
  return l;
}

// Entre x1 y x2, debajo de la barra, no se dibujó nada
static bool franjaVacia(int x1, int x2) {
  for (int y = 28; y < ALTO; y++) {
    for (int x = x1; x <= x2; x++) {
      if (marco[y * ANCHO + x] != 0) {
        return false;
      }
    }
  }
  return true;
}

// Touch de mentira: un toque es apoyar y levantar el dedo en x, y
static lv_point_t dedo;
static bool apoyado = false;

static void leerDedo(lv_indev_t *, lv_indev_data_t *datos) {
  datos->point = dedo;
  datos->state = apoyado ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void apoyar(lv_indev_t *indev, int32_t x, int32_t y) {
  dedo = {x, y};
  apoyado = true;
  lv_indev_read(indev);
}

static void levantar(lv_indev_t *indev) {
  apoyado = false;
  lv_indev_read(indev);
}

static int8_t tocar(lv_indev_t *indev, Paginas &paginas, int32_t x, int32_t y) {
  apoyar(indev, x, y);
  reloj += 80;
  levantar(indev);
  reloj += 80;
  return paginas.tomarToque();
}

// Mantiene el dedo apoyado ms más, leyendo el touch cada 100 ms como la
// placa. true si en ese rato se pidió alguna acción.
static bool mantener(lv_indev_t *indev, Paginas &paginas, uint32_t ms) {
  bool pidio = false;
  for (uint32_t pasado = 0; pasado < ms; pasado += 100) {
    reloj += 100;
    lv_indev_read(indev);
    pidio = paginas.tomarAccion(reloj) != Accion::NINGUNA || pidio;
  }
  return pidio;
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
  DatosInfo info = {"v2.1", "6", 17, 6, "81:23:45:67:89:BA", -58};
  // La de la maqueta, con la luz encendida y 2 códigos según 0101
  LecturaFallas fallas =
      lecturaFallas("410182076100", "43 02 04 20 01 71", "47 01 03 00", "NO DATA");
  uint32_t reintento = 0;
  size_t antes = memoriaEnUso();
  paginas.iniciar();
  for (uint8_t i = 0; i < 6; i++) {
    paginas.dibujar(reloj, info, fallas, reintento);
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
  pantalla.sinAdaptador(5);
  guardar(directorio, "0-sin-adaptador");
  pantalla.sinAdaptador(0);
  guardar(directorio, "0-sin-adaptador-buscando");
  revisar(tocar(indev, paginas, 500, 100) == 0 && paginas.tomarAccion(reloj) == Accion::NINGUNA,
          "con SIN ADAPTADOR a la vista, tocar no hace nada");

  for (uint8_t i = 1; i <= 6; i++) {
    paginas.dibujar(reloj, info, fallas, reintento);
    char nombre[32];
    snprintf(nombre, sizeof(nombre), "%u-%s", i, paginas.titulo());
    for (char *c = nombre; *c != '\0'; c++) {
      *c = *c == '/' ? '-' : (char)tolower(*c);
    }
    guardar(directorio, nombre);
    paginas.dibujar(reloj, info, fallas, reintento);
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
  paginas.dibujar(reloj, info, fallas, reintento);
  guardar(directorio, "7-motor-extremos");
  irA(paginas, "CARGA");
  paginas.dibujar(reloj, info, fallas, reintento);
  guardar(directorio, "8-carga-extremos");
  irA(paginas, "MEZCLA");
  paginas.dibujar(reloj, info, fallas, reintento);
  guardar(directorio, "9-mezcla-extremos");

  // Vencidos: "--" en gris, sin barras
  reloj += 5000;
  irA(paginas, "MOTOR");
  paginas.dibujar(reloj, info, fallas, reintento);
  guardar(directorio, "a-motor-vencido");
  irA(paginas, "MEZCLA");
  paginas.dibujar(reloj, info, fallas, reintento);
  guardar(directorio, "b-mezcla-vencido");

  // Fallas: cada caso con su nombre, la lectura y si los textos van en
  // columnas (no al centro)
  struct CasoFallas {
    const char *nombre;
    LecturaFallas lectura;
    bool columnas;
  };
  const CasoFallas casos[] = {
      {"c-fallas-leyendo", LecturaFallas(), false},
      // Las respuestas del DS3 (E3)
      {"d-fallas-sin-fallas", lecturaFallas("410100076100", "4300", "4700", "NO DATA"), false},
      {"e-fallas-sin-fallas-luz-sin-leer", lecturaFallas("NO DATA", "4300", "4700", "NO DATA"),
       false},
      {"f-fallas-luz-sin-codigos", lecturaFallas("410180076100", "4300", "4700", "NO DATA"), true},
      // La descripción más larga con "(pendiente)", y uno sin descripción
      {"g-fallas-pendientes", lecturaFallas("410100076100", "4300", "47 02 01 20 12 34", "NO DATA"),
       true},
      // P0420 guardado y pendiente: va una vez
      {"h-fallas-repetido",
       lecturaFallas("410181076100", "43 01 04 20", "47 02 04 20 01 71", "NO DATA"), true},
      // 07 no contestó: lo de 03 y "Pendientes: error"
      {"i-fallas-error", lecturaFallas("410181076100", "43 01 04 20", "NO DATA", "NO DATA"), true},
      {"j-fallas-error-todo", lecturaFallas("NO DATA", "NO DATA", "CAN ERROR", "NO DATA"), true},
      // Con 0A: tres cantidades, y una descripción que no entra con "(permanente)"
      {"k-fallas-permanentes",
       lecturaFallas("410181076100", "43 01 04 20", "47 01 03 00", "4A 02 01 71 01 36"), true},
  };
  irA(paginas, "FALLAS");
  reintento = 4;
  for (const CasoFallas &caso : casos) {
    fallas = caso.lectura;
    paginas.dibujar(reloj, info, fallas, reintento);
    guardar(directorio, caso.nombre);
    paginas.dibujar(reloj, info, fallas, reintento);
    char que[96];
    snprintf(que, sizeof(que), "%s: dibujar de nuevo lo mismo no redibuja nada", caso.nombre);
    revisar(display->inv_p == 0, que);
    snprintf(que, sizeof(que), "%s: nada pasa del borde derecho", caso.nombre);
    revisar(franjaVacia(633, ANCHO - 1), que);
    if (caso.columnas) {
      snprintf(que, sizeof(que), "%s: nada pasa de la columna izquierda", caso.nombre);
      revisar(franjaVacia(209, 219), que);
    }
  }

  // 7 códigos, uno repetido: 6 renglones en dos grupos que se alternan
  fallas = lecturaFallas("410185076100", "00C\r0: 43 05 04 20 01 71 \r1: 03 02 12 34 01 33 00 \r\r",
                         "47 02 01 33 03 00", "NO DATA");
  reloj += 2 * PERIODO_GRUPO_FALLAS_MS - reloj % (2 * PERIODO_GRUPO_FALLAS_MS);
  paginas.dibujar(reloj, info, fallas, reintento);
  guardar(directorio, "l-fallas-grupo-1");
  revisar(franjaVacia(633, ANCHO - 1) && franjaVacia(209, 219),
          "l-fallas-grupo-1: nada se pasa de su columna");
  reloj += PERIODO_GRUPO_FALLAS_MS - 1;
  paginas.dibujar(reloj, info, fallas, reintento);
  revisar(display->inv_p == 0, "el grupo no cambia antes de tiempo");
  reloj += 1;
  paginas.dibujar(reloj, info, fallas, reintento);
  revisar(display->inv_p > 0, "a los 3 s pasa al otro grupo");
  guardar(directorio, "m-fallas-grupo-2");
  revisar(guardadas[guardadas.size() - 1] != guardadas[guardadas.size() - 2],
          "los dos grupos se ven distintos");
  revisar(tocar(indev, paginas, 500, 100) == 1, "en Fallas, tocar a la derecha: siguiente");

  // Info: cada botón pide lo suyo sin cambiar de página. Los centros, con
  // la barra de 28 px: Reconectar (528, 66) y Olvidar (528, 134).
  irA(paginas, "INFO");
  paginas.dibujar(reloj, info, fallas, reintento);
  std::vector<uint16_t> suelta = capturar();
  revisar(franjaVacia(417, 423), "Info: los datos no llegan a los botones");

  apoyar(indev, 528, 66);
  paginas.dibujar(reloj, info, fallas, reintento);
  guardar(directorio, "n-info-reconectar-apretado");
  levantar(indev);
  revisar(paginas.tomarToque() == 0 && paginas.tomarAccion(reloj) == Accion::RECONECTAR,
          "Reconectar: al soltarlo pide reconectar, sin cambiar de pagina");
  revisar(paginas.tomarAccion(reloj) == Accion::NINGUNA, "Reconectar: lo pide una sola vez");

  apoyar(indev, 528, 134);
  bool antesDeTiempo = mantener(indev, paginas, MANTENER_OLVIDAR_MS / 2);
  paginas.dibujar(reloj, info, fallas, reintento);
  guardar(directorio, "o-info-olvidar-a-la-mitad");
  antesDeTiempo = mantener(indev, paginas, MANTENER_OLVIDAR_MS / 2 - 100) || antesDeTiempo;
  revisar(!antesDeTiempo, "Olvidar: antes de 3 s no pide nada");
  revisar(mantener(indev, paginas, 100), "Olvidar: a los 3 s pide olvidar");
  revisar(!mantener(indev, paginas, 1000), "Olvidar: si se sigue apretando, no lo vuelve a pedir");
  levantar(indev);
  revisar(paginas.tomarToque() == 0 && paginas.tomarAccion(reloj) == Accion::NINGUNA,
          "Olvidar: al soltarlo no cambia de pagina ni pide nada");
  paginas.dibujar(reloj, info, fallas, reintento);
  revisar(capturar() == suelta, "Olvidar: suelto, el relleno desaparece");

  apoyar(indev, 528, 134);
  mantener(indev, paginas, 500);
  dedo = {300, 100};
  bool pidio = mantener(indev, paginas, MANTENER_OLVIDAR_MS);
  levantar(indev);
  revisar(!pidio && paginas.tomarToque() == 0,
          "Olvidar: si el dedo se va del boton, no pide nada ni cambia de pagina");

  // Fuera de los botones, Info cambia de página como las demás
  revisar(tocar(indev, paginas, 100, 100) == -1, "en Info, tocar los datos: anterior");
  revisar(tocar(indev, paginas, 380, 60) == 1,
          "en Info, tocar a la derecha de los datos: siguiente");
  revisar(tocar(indev, paginas, 528, 100) == 1, "en Info, tocar entre los botones: siguiente");

  // Toques con las páginas a la vista
  revisar(tocar(indev, paginas, 100, 100) == -1, "tocar a la izquierda: anterior");
  revisar(tocar(indev, paginas, 500, 100) == 1, "tocar a la derecha: siguiente");
  revisar(tocar(indev, paginas, 20, 10) == -1, "tocar la barra a la izquierda: anterior");
  revisar(tocar(indev, paginas, 600, 10) == 1, "tocar la barra a la derecha: siguiente");
  revisar(paginas.tomarToque() == 0, "sin tocar no hay pedido");

  guardarTodas(directorio);
  printf("PNG en %s\n", directorio);
  return errores > 0 ? 1 : 0;
}
