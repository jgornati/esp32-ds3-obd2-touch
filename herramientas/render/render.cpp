// Render de las pantallas en la PC, con el LVGL y el código de pantalla,
// paginas, animaciones y datos del firmware. PantallaHw es de mentira: la
// imagen va a un búfer y de ahí a PNG (al doble de tamaño). Además revisa
// que dibujar dos veces lo mismo no deje nada para redibujar (en la placa,
// cada redibujo cuesta ~33 ms de envío), que el toque cambie de página como
// corresponde, que los botones de Info pidan lo suyo sin cambiar de página,
// que la espera reintente y reinicie, que las animaciones se muevan, que
// mantener apretada Viaje la reinicie, que las alarmas se vean y no
// redibujen de más, y que los textos de Fallas, Info y Viaje no se pasen de
// su columna.
//
// El reloj de LVGL es de mentira y no se refresca solo: las animaciones
// avanzan con irA() y la imagen se dibuja al capturarla.
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

#include <alarmas.h>
#include <dtc.h>
#include <respuesta.h>
#include <viaje.h>

#include "animaciones.h"
#include "config.h"
#include "datos.h"
#include "dibujo.h"
#include "imagenes/imagenes.h"
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

// Lleva el reloj a momento y corre los temporizadores de LVGL (las
// animaciones y la lectura del touch) una vez
static void irA(uint32_t momento) {
  reloj = momento;
  lv_timer_handler();
}

static int cantidadDePaginas(Paginas &paginas) {
  const char *primera = paginas.titulo();
  int cantidad = 0;
  do {
    paginas.cambiar(1);
    cantidad++;
  } while (strcmp(paginas.titulo(), primera) != 0);
  return cantidad;
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

static const ReglasViaje REGLAS_VIAJE = {VIGENCIA_MS, RPM_EN_MARCHA, ESPERA_VOLTAJE_VIAJE_MS};

// Un viaje de minutos a kmh, con una lectura por segundo desde desde, y los
// picos en el medio. Los voltajes cuentan después de los primeros 5 s.
static Viaje viajeDe(uint32_t minutos, float kmh, float velocidadMaxima, float rpmMaxima,
                     float refrigeranteMaximo, float voltajeMinimo, float voltajeMaximo) {
  Viaje viaje(REGLAS_VIAJE);
  uint32_t segundos = minutos * 60;
  uint32_t desde = 1000;
  for (uint32_t s = 0; s <= segundos; s++) {
    uint32_t ahora = desde + s * 1000;
    bool pico = s == segundos / 2;
    viaje.velocidad(pico ? velocidadMaxima : kmh, ahora);
    viaje.rpm(pico ? rpmMaxima : 2000, ahora);
    viaje.refrigerante(pico ? refrigeranteMaximo : refrigeranteMaximo - 4);
    viaje.voltaje(s == 10 ? voltajeMinimo : (pico ? voltajeMaximo : 14.0f), ahora);
  }
  return viaje;
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

// Entre x1 y x2, y entre y1 e y2, no se dibujó nada
static bool zonaVacia(int x1, int x2, int y1, int y2) {
  for (int y = y1; y <= y2; y++) {
    for (int x = x1; x <= x2; x++) {
      if (marco[y * ANCHO + x] != 0) {
        return false;
      }
    }
  }
  return true;
}

// Entre x1 y x2, debajo de la barra, no se dibujó nada
static bool franjaVacia(int x1, int x2) {
  return zonaVacia(x1, x2, 28, ALTO - 1);
}

// En Viaje, en las dos filas de casillas (sin la línea que las separa), entre
// x1 y x2 no se dibujó nada
static bool franjaVaciaEnCasillas(int x1, int x2) {
  return zonaVacia(x1, x2, 28, 28 + 70) && zonaVacia(x1, x2, 28 + 72, ALTO - 1);
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

// Mantiene apretada la espera ms más, como mantener() pero con los
// temporizadores de LVGL, que miden cuánto lleva
static void mantenerEspera(uint32_t ms) {
  for (uint32_t pasado = 0; pasado < ms; pasado += 100) {
    irA(reloj + 100);
  }
}

static int reinicios = 0;

static void contarReinicio() {
  reinicios++;
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
  // Se dibuja solo al capturar: así se ve qué quedó para redibujar
  lv_timer_pause(lv_display_get_refr_timer(display));

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
  // El de la maqueta de E10: 23.7 km en 38 min
  Viaje viaje = viajeDe(38, 37.4f, 118, 5120, 94, 13.6f, 14.3f);
  AlarmaALaVista alarma = {nullptr, 0};
  size_t antes = memoriaEnUso();
  paginas.iniciar();
  const int cantidad = cantidadDePaginas(paginas);
  for (int i = 0; i < cantidad; i++) {
    paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
    lv_refr_now(nullptr);
    paginas.cambiar(1);
  }
  size_t usada = memoriaEnUso() - antes;
  if (usada > 0) {
    printf("Paginas: %zu bytes de malloc en la PC (con punteros de 64 bits)\n", usada);
  }

  // Espera: hasta los 5 s, negra con el estado abajo
  uint32_t encendida = reloj; // la de Pantalla::iniciar()
  pantalla.alMantenerApretada(contarReinicio);
  pantalla.espera("Conectando con el adaptador...");
  guardar(directorio, "0-espera-conectando");
  revisar(tocar(indev, paginas, 500, 100) == 0 && paginas.tomarAccion(reloj) == Accion::NINGUNA &&
              pantalla.tomarReintento(),
          "espera: tocar pide reintentar, sin cambiar de pagina");
  revisar(!pantalla.tomarReintento(), "espera: el reintento se pide una sola vez");

  // A los 5 s entra el auto; se guardan algunos cuadros de los originales
  irA(encendida + ESPERA_ENTRADA_MS);
  revisar(capturar() == guardadas.back(), "espera: en el cuadro 0 el auto todavia no se ve");
  for (uint32_t cuadro : {6, 13, 20, 30}) {
    irA(encendida + ESPERA_ENTRADA_MS + cuadro * entrada_ms_cuadro);
    char nombre[32];
    snprintf(nombre, sizeof(nombre), "0-espera-entrando-%02u", (unsigned)cuadro);
    guardar(directorio, nombre);
  }
  irA(encendida + ESPERA_ENTRADA_MS + entrada_cantidad * entrada_ms_cuadro);
  guardar(directorio, "0-espera-quieto");
  irA(reloj + 1000);
  pantalla.espera("Conectando con el adaptador...");
  revisar(display->inv_p == 0, "espera: con el auto quieto, no se redibuja nada");
  pantalla.espera("Esperando la ECU");
  guardar(directorio, "0-espera-sin-ecu");
  pantalla.espera("Sin respuesta de la ECU", true);
  guardar(directorio, "0-espera-ecu-perdida");
  revisar(franjaVacia(633, ANCHO - 1), "espera: nada pasa del borde derecho");

  // Mantener apretado: la barra crece, a los 3 s reinicia una vez
  apoyar(indev, 300, 80);
  mantenerEspera(MANTENER_REINICIO_MS / 2);
  guardar(directorio, "0-espera-mantener");
  levantar(indev);
  revisar(reinicios == 0 && pantalla.tomarReintento(),
          "espera: soltarla antes de 3 s reintenta, sin reiniciar");
  irA(reloj + 100);
  revisar(capturar() == guardadas[guardadas.size() - 2], "espera: suelta, la barra desaparece");
  apoyar(indev, 300, 80);
  mantenerEspera(MANTENER_REINICIO_MS - 100);
  revisar(reinicios == 0, "espera: antes de 3 s no reinicia");
  mantenerEspera(200);
  revisar(reinicios == 1, "espera: a los 3 s reinicia");
  mantenerEspera(2000);
  levantar(indev);
  revisar(reinicios == 1 && !pantalla.tomarReintento() && paginas.tomarToque() == 0,
          "espera: reinicia una sola vez, y al soltar no reintenta");

  // Si a los 5 s se ven los datos, el auto no entra: después aparece quieto
  Entrada sinVer;
  sinVer.crear(lv_obj_create(nullptr), reloj, 0);
  sinVer.mover(reloj, false);
  revisar(sinVer.quieto(), "espera: si no se veia a los 5 s, el auto aparece quieto");

  for (int i = 1; i <= cantidad; i++) {
    paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
    char nombre[32];
    snprintf(nombre, sizeof(nombre), "%d-%s", i, paginas.titulo());
    for (char *c = nombre; *c != '\0'; c++) {
      *c = *c == '/' ? '-' : (char)tolower(*c);
    }
    guardar(directorio, nombre);
    paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
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
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  guardar(directorio, "7-motor-extremos");
  irA(paginas, "CARGA");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  guardar(directorio, "8-carga-extremos");
  irA(paginas, "MEZCLA");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  guardar(directorio, "9-mezcla-extremos");

  // Vencidos: "--" en gris, sin barras
  reloj += 5000;
  irA(paginas, "MOTOR");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  guardar(directorio, "a-motor-vencido");
  irA(paginas, "MEZCLA");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
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
    paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
    guardar(directorio, caso.nombre);
    paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
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
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  guardar(directorio, "l-fallas-grupo-1");
  revisar(franjaVacia(633, ANCHO - 1) && franjaVacia(209, 219),
          "l-fallas-grupo-1: nada se pasa de su columna");
  reloj += PERIODO_GRUPO_FALLAS_MS - 1;
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  revisar(display->inv_p == 0, "el grupo no cambia antes de tiempo");
  reloj += 1;
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  revisar(display->inv_p > 0, "a los 3 s pasa al otro grupo");
  guardar(directorio, "m-fallas-grupo-2");
  revisar(guardadas[guardadas.size() - 1] != guardadas[guardadas.size() - 2],
          "los dos grupos se ven distintos");
  revisar(tocar(indev, paginas, 500, 100) == 1, "en Fallas, tocar a la derecha: siguiente");

  // Info: cada botón pide lo suyo sin cambiar de página. Los centros, con
  // la barra de 28 px: Reconectar (528, 66) y Olvidar (528, 134).
  irA(paginas, "INFO");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  std::vector<uint16_t> suelta = capturar();
  revisar(franjaVacia(417, 423), "Info: los datos no llegan a los botones");

  apoyar(indev, 528, 66);
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  guardar(directorio, "n-info-reconectar-apretado");
  levantar(indev);
  revisar(paginas.tomarToque() == 0 && paginas.tomarAccion(reloj) == Accion::RECONECTAR,
          "Reconectar: al soltarlo pide reconectar, sin cambiar de pagina");
  revisar(paginas.tomarAccion(reloj) == Accion::NINGUNA, "Reconectar: lo pide una sola vez");

  apoyar(indev, 528, 134);
  bool antesDeTiempo = mantener(indev, paginas, MANTENER_OLVIDAR_MS / 2);
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  guardar(directorio, "o-info-olvidar-a-la-mitad");
  antesDeTiempo = mantener(indev, paginas, MANTENER_OLVIDAR_MS / 2 - 100) || antesDeTiempo;
  revisar(!antesDeTiempo, "Olvidar: antes de 3 s no pide nada");
  revisar(mantener(indev, paginas, 100), "Olvidar: a los 3 s pide olvidar");
  revisar(!mantener(indev, paginas, 1000), "Olvidar: si se sigue apretando, no lo vuelve a pedir");
  levantar(indev);
  revisar(paginas.tomarToque() == 0 && paginas.tomarAccion(reloj) == Accion::NINGUNA,
          "Olvidar: al soltarlo no cambia de pagina ni pide nada");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
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

  // DS3: el paseo, a pantalla completa. Los momentos de los cuadros 0, 5 y
  // 30 de los originales (la vuelta empieza con el reloj en 0).
  irA(paginas, "DS3");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  uint32_t vuelta = reloj - reloj % paseo.duracion_ms + paseo.duracion_ms;
  for (uint32_t cuadro : {0, 5, 30}) {
    irA(vuelta + cuadro * paseo.ms_cuadro);
    char nombre[32];
    snprintf(nombre, sizeof(nombre), "q-ds3-%02u", (unsigned)cuadro);
    guardar(directorio, nombre);
  }
  revisar(guardadas[guardadas.size() - 1] != guardadas[guardadas.size() - 2], "DS3: se mueve");
  irA(vuelta + paseo.duracion_ms);
  revisar(capturar() == guardadas[guardadas.size() - 3], "DS3: a los 3 s vuelve a empezar");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  revisar(display->inv_p == 0, "DS3: dibujar de nuevo lo mismo no redibuja nada");
  revisar(tocar(indev, paginas, 100, 100) == -1, "en DS3, tocar a la izquierda: anterior");
  paginas.cambiar(1);
  revisar(tocar(indev, paginas, 500, 10) == 1, "en DS3, tocar arriba a la derecha: siguiente");
  paginas.cambiar(-1);
  irA(reloj + 1000);
  revisar(display->inv_p == 0, "DS3: oculta, no se mueve");

  // Viaje: al poner el contacto, uno largo y uno sin velocidad. Ningún texto
  // pasa de su casilla.
  struct CasoViaje {
    const char *nombre;
    Viaje viaje;
  };
  Viaje arranque(REGLAS_VIAJE);
  arranque.rpm(0, 1000);
  arranque.velocidad(0, 1000);
  arranque.refrigerante(21);
  const CasoViaje casosViaje[] = {
      {"r-viaje-arranque", arranque},
      {"s-viaje-extremos", viajeDe(754, 98.3f, 188, 6500, 112, 11.9f, 15.2f)},
  };
  irA(paginas, "VIAJE");
  for (const CasoViaje &caso : casosViaje) {
    viaje = caso.viaje;
    paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
    guardar(directorio, caso.nombre);
    paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
    char que[96];
    snprintf(que, sizeof(que), "%s: dibujar de nuevo lo mismo no redibuja nada", caso.nombre);
    revisar(display->inv_p == 0, que);
    snprintf(que, sizeof(que), "%s: nada pasa de su casilla", caso.nombre);
    revisar(franjaVaciaEnCasillas(150, 158) && franjaVaciaEnCasillas(310, 318) &&
                franjaVaciaEnCasillas(470, 478) && franjaVaciaEnCasillas(633, ANCHO - 1),
            que);
  }
  // Sin la velocidad (0D): N/D lo que sale de ella
  PidsSoportados sinVelocidad;
  const uint8_t mapa00[] = {0xBE, 0x16, 0xA8, 0x11};
  sinVelocidad.cargar(0x00, mapa00, 4);
  reiniciarDatos(sinVelocidad);
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  guardar(directorio, "t-viaje-sin-velocidad");
  datosDeLaMaqueta();

  // Mantener apretada Viaje: la barra se llena de rojo y a los 3 s la
  // reinicia, una vez; al soltarla no cambia de página. Un toque, sí.
  viaje = viajeDe(38, 37.4f, 118, 5120, 94, 13.6f, 14.3f);
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  std::vector<uint16_t> sinApretar = capturar();
  revisar(tocar(indev, paginas, 500, 100) == 1 && paginas.tomarAccion(reloj) == Accion::NINGUNA,
          "en Viaje, tocar a la derecha: siguiente");
  revisar(tocar(indev, paginas, 100, 10) == -1,
          "en Viaje, tocar la barra a la izquierda: anterior");
  apoyar(indev, 500, 100);
  pidio = mantener(indev, paginas, AVISO_MANTENER_MS - 100);
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  revisar(capturar() == sinApretar, "Viaje: apretada menos de 0,5 s, la barra no cambia");
  pidio = mantener(indev, paginas, MANTENER_VIAJE_MS / 2 - AVISO_MANTENER_MS + 100) || pidio;
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  guardar(directorio, "u-viaje-mantener");
  levantar(indev);
  revisar(!pidio && paginas.tomarToque() == 0 && paginas.tomarAccion(reloj) == Accion::NINGUNA,
          "Viaje: soltarla antes de 3 s no reinicia ni cambia de pagina");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  revisar(capturar() == sinApretar, "Viaje: suelta, la barra desaparece");

  apoyar(indev, 100, 100);
  pidio = mantener(indev, paginas, MANTENER_VIAJE_MS - 100);
  revisar(!pidio, "Viaje: antes de 3 s no pide reiniciar");
  Accion pedida = Accion::NINGUNA;
  for (uint32_t pasado = 0; pasado < 200; pasado += 100) {
    reloj += 100;
    lv_indev_read(indev);
    Accion a = paginas.tomarAccion(reloj);
    pedida = a != Accion::NINGUNA ? a : pedida;
  }
  revisar(pedida == Accion::REINICIAR_VIAJE, "Viaje: a los 3 s pide reiniciar el viaje");
  viaje.reiniciar(); // lo que hace main.cpp
  revisar(!mantener(indev, paginas, 1000), "Viaje: si se sigue apretando, no lo vuelve a pedir");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  guardar(directorio, "v-viaje-reiniciado");
  levantar(indev);
  revisar(paginas.tomarToque() == 0 && paginas.tomarAccion(reloj) == Accion::NINGUNA,
          "Viaje: al soltarla despues de reiniciar no cambia de pagina");

  // En las otras páginas, un toque largo sigue cambiando de página
  irA(paginas, "MOTOR");
  datosDeLaMaqueta();
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  std::vector<uint16_t> motor = capturar();
  apoyar(indev, 500, 100);
  pidio = mantener(indev, paginas, MANTENER_VIAJE_MS / 2);
  datosDeLaMaqueta();
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  revisar(capturar() == motor, "en Motor, mantenerla no llena la barra");
  pidio = mantener(indev, paginas, MANTENER_VIAJE_MS) || pidio;
  levantar(indev);
  revisar(!pidio && paginas.tomarToque() == 1,
          "en Motor, mantener y soltar: siguiente, sin reiniciar el viaje");

  // Alarmas: la barra en rojo con el texto y el valor en rojo en su página.
  // El texto es el de lib/obd, como en main.cpp.
  char textoBarra[48];
  struct CasoAlarma {
    const char *nombre;
    const char *pagina;
    TipoAlarma tipo;
    float valor;
    Magnitud magnitud;
    const char *despues; // "  1/2" con varias
    uint16_t otras;      // las magnitudes de las demás alarmas
  };
  const CasoAlarma casosAlarma[] = {
      {"w-alarma-refrigerante", "MOTOR", REFRIGERANTE_ALTO, 106, REFRIGERANTE, "", 0},
      {"x-alarma-zona-roja", "MOTOR", RPM_ALTA, 6250, RPM, "", 0},
      {"y-alarma-dos", "AIRE/VOLT", VOLTAJE_ALTO, 15.4f, VOLT_ELM, "  1/2", 1 << RPM},
      {"z-alarma-ds3", "DS3", REFRIGERANTE_ALTO, 106, REFRIGERANTE, "", 0},
  };
  datosDeLaMaqueta();
  irA(paginas, "MOTOR");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  std::vector<uint16_t> motorSinAlarma = capturar();
  for (const CasoAlarma &caso : casosAlarma) {
    datosDeLaMaqueta();
    actualizarDato(caso.magnitud, caso.valor, reloj);
    textoAlarma(caso.tipo, caso.valor, textoBarra, sizeof(textoBarra));
    strlcat(textoBarra, caso.despues, sizeof(textoBarra));
    alarma = {textoBarra, (uint16_t)((1 << caso.magnitud) | caso.otras)};
    irA(paginas, caso.pagina);
    paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
    if (strcmp(caso.pagina, "DS3") == 0) {
      irA(reloj + 100);
    }
    guardar(directorio, caso.nombre);
    paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
    char que[96];
    snprintf(que, sizeof(que), "%s: dibujar de nuevo lo mismo no redibuja nada", caso.nombre);
    revisar(display->inv_p == 0, que);
    snprintf(que, sizeof(que), "%s: la barra es roja", caso.nombre);
    revisar(marco[5 * ANCHO + 630] == lv_color_to_u16(ROJO), que);
    snprintf(que, sizeof(que), "%s: tocar la barra roja sigue cambiando de pagina", caso.nombre);
    revisar(tocar(indev, paginas, 600, 10) == 1 && tocar(indev, paginas, 20, 10) == -1, que);
  }
  // Sin alarma, todo vuelve a como estaba; en DS3, sin la barra
  alarma = {nullptr, 0};
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  capturar();
  revisar(marco[5 * ANCHO + 630] != lv_color_to_u16(ROJO), "DS3: sin alarma, la barra no se ve");
  datosDeLaMaqueta();
  irA(paginas, "MOTOR");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  revisar(capturar() == motorSinAlarma, "Motor: sin alarma, como antes");

  // Viaje mantenida con una alarma: se ve lo de mantener
  irA(paginas, "VIAJE");
  alarma = {"REFRIGERANTE 106 °C", 1 << REFRIGERANTE};
  apoyar(indev, 300, 100);
  mantener(indev, paginas, MANTENER_VIAJE_MS / 2);
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  guardar(directorio, "z-alarma-viaje-mantenida");
  revisar(marco[5 * ANCHO + 630] != lv_color_to_u16(ROJO),
          "Viaje mantenida con una alarma: se ve lo de mantener");
  levantar(indev);
  paginas.tomarToque();
  alarma = {nullptr, 0};

  // Toques con las páginas a la vista
  irA(paginas, "MOTOR");
  paginas.dibujar(reloj, info, fallas, reintento, viaje, alarma);
  revisar(tocar(indev, paginas, 100, 100) == -1, "tocar a la izquierda: anterior");
  revisar(tocar(indev, paginas, 500, 100) == 1, "tocar a la derecha: siguiente");
  revisar(tocar(indev, paginas, 20, 10) == -1, "tocar la barra a la izquierda: anterior");
  revisar(tocar(indev, paginas, 600, 10) == 1, "tocar la barra a la derecha: siguiente");
  revisar(paginas.tomarToque() == 0, "sin tocar no hay pedido");

  guardarTodas(directorio);
  printf("PNG en %s\n", directorio);
  return errores > 0 ? 1 : 0;
}
