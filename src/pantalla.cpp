#include "pantalla.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "animaciones.h"
#include "config.h"
#include "dibujo.h"
#include "imagenes/imagenes.h"

// 640x172: barra de ALTO_BARRA y debajo RENGLONES líneas de ALTO_RENGLON.
static const int ANCHO = 640;
static const int ALTO = 172;
static const int ALTO_BARRA = 28;
static const int RENGLONES = 4;
static const int ALTO_RENGLON = (ALTO - ALTO_BARRA) / RENGLONES;
static const int MARGEN = 8;
// Puntos de página: a la derecha de la barra, centrados en su alto
static const int PUNTO = 8;
static const int ENTRE_PUNTOS = 8;
// La marca "SD", a la izquierda de los puntos
static const int ANCHO_MARCA = 26;
static const int ALTO_MARCA = 16;
static const int ANTES_DE_PUNTOS = 12;
// La hora: se pide cada esto
static const uint32_t PERIODO_RELOJ_MS = 500;
// Espera: el auto un poco más arriba que en los cuadros originales, para
// que entre el renglón de abajo, y debajo la barra de mantener apretado
static const int Y_AUTO = 5;
static const int BASE_ESPERA = 160;
static const int Y_REINICIO = 167;
static const int ALTO_REINICIO = 5;

static lv_obj_t *fondoBarra = nullptr;
static lv_obj_t *rellenoBarra = nullptr; // mantener apretado
static lv_obj_t *titulo = nullptr;
static char textoAlarma[48]; // el triángulo y la alarma
static lv_obj_t *puntos[MAX_PAGINAS];
static lv_obj_t *marcaSd = nullptr;
static lv_obj_t *relojBarra = nullptr;
static int limiteReloj = -1; // su borde derecho
static lv_obj_t *relojEspera = nullptr;
static bool (*pedirHora)(char *texto, size_t tam) = nullptr;
// Pantallas de estado: RENGLONES líneas de texto
static lv_obj_t *vistaEstado = nullptr;
static lv_obj_t *textos[RENGLONES];
// Las arman Paginas
static lv_obj_t *vistaPaginas = nullptr;
static lv_obj_t *vistaCompleta = nullptr;
// Espera
static lv_obj_t *vistaEspera = nullptr;
static Entrada entrada;
static lv_obj_t *estadoEspera = nullptr;
static lv_obj_t *pistaEspera = nullptr;
static lv_obj_t *rellenoReinicio = nullptr;
static bool apretada = false; // con el dedo quieto
static uint32_t apretadaDesde = 0;
static lv_point_t dondeApretada = {0, 0};
static bool reinicioPedido = false; // ya se llamó en esta apretada
static bool reintento = false;
static void (*alMantener)() = nullptr;
// El aviso de cambio (E25), arriba de todo
static lv_obj_t *capaCambio = nullptr;
static lv_obj_t *textoCambio = nullptr;
static lv_timer_t *parpadeo = nullptr;
static bool enAzul = false;

// Muestra una vista y oculta las demás. La barra va con las de estado y
// las páginas comunes, y en la completa solo con conBarra (una alarma).
static void ver(lv_obj_t *vista, bool conBarra = false) {
  lv_obj_t *const vistas[] = {vistaEstado, vistaPaginas, vistaCompleta, vistaEspera};
  for (lv_obj_t *v : vistas) {
    mostrar(v, v == vista);
  }
  mostrar(fondoBarra, vista == vistaEstado || vista == vistaPaginas || conBarra);
}

// Con total > 0, un punto por página pegado al margen derecho, numero en
// blanco, antes la marca de la microSD y antes la hora. Con mantenida > 0, el
// relleno rojo hasta esa fracción del ancho. Si no, con una alarma, la barra
// en rojo con la alarma en lugar del texto.
static void barra(const char *texto, lv_color_t color, uint8_t numero = 0, uint8_t total = 0,
                  float mantenida = 0, const char *alarma = nullptr,
                  MarcaSd sd = MarcaSd::NINGUNA) {
  bool conAlarma = alarma != nullptr && mantenida <= 0;
  if (conAlarma) {
    snprintf(textoAlarma, sizeof(textoAlarma), LV_SYMBOL_WARNING " %s", alarma);
    texto = textoAlarma;
    color = BLANCO;
  }
  ponerFondo(fondoBarra, conAlarma ? ROJO : FONDO_BARRA);
  ponerTexto(titulo, texto);
  ponerColor(titulo, color);
  int32_t lleno = lroundf(ANCHO * fminf(1, mantenida));
  mostrar(rellenoBarra, lleno > 0);
  if (lleno > 0) {
    ponerAncho(rellenoBarra, lleno);
  }
  int x = ANCHO - MARGEN - total * (PUNTO + ENTRE_PUNTOS) + ENTRE_PUNTOS;
  for (uint8_t i = 0; i < MAX_PAGINAS; i++) {
    bool visible = i < total;
    mostrar(puntos[i], visible);
    if (visible) {
      ponerPosicion(puntos[i], x + i * (PUNTO + ENTRE_PUNTOS), (ALTO_BARRA - PUNTO) / 2);
      ponerFondo(puntos[i], i == numero ? BLANCO : PUNTO_APAGADO);
    }
  }
  bool conMarca = total > 0 && sd != MarcaSd::NINGUNA;
  int xMarca = x - ANTES_DE_PUNTOS - ANCHO_MARCA;
  mostrar(marcaSd, conMarca);
  if (conMarca) {
    ponerPosicion(marcaSd, xMarca, (ALTO_BARRA - ALTO_MARCA) / 2);
    ponerFondo(marcaSd, sd == MarcaSd::GRABANDO ? ETIQUETA : AMBAR);
  }
  // La hora, pegada a lo que tenga a la derecha. Alineada a la derecha: si
  // cambia el ancho del texto, el borde queda.
  int limite = conMarca    ? xMarca - ANTES_DE_PUNTOS
               : total > 0 ? x - ANTES_DE_PUNTOS
                           : ANCHO - MARGEN;
  if (limite != limiteReloj) {
    limiteReloj = limite;
    lv_obj_align(relojBarra, LV_ALIGN_RIGHT_MID, limite - ANCHO, 0);
  }
}

static void alActualizarHora(lv_timer_t *) {
  char texto[8];
  bool valida = pedirHora != nullptr && pedirHora(texto, sizeof(texto));
  if (!valida) {
    strcpy(texto, "--:--");
  }
  ponerTexto(relojBarra, texto);
  ponerColor(relojBarra, valida ? BLANCO : GRIS);
  ponerTexto(relojEspera, texto);
  ponerColor(relojEspera, valida ? ETIQUETA : GRIS);
}

static void armarEstado(const char *texto, lv_color_t color,
                        const char *const (&lineas)[RENGLONES]) {
  barra(texto, color);
  ver(vistaEstado);
  for (int i = 0; i < RENGLONES; i++) {
    ponerTexto(textos[i], lineas[i]);
  }
}

// Tocar la espera es un pedido de reintento al soltar, salvo que ya se haya
// pedido reiniciar. Si el dedo se va de la pantalla o se corre (para cambiar
// de página), no cuenta, y mantenerla tampoco.
static void alApretarEspera(lv_event_t *e) {
  lv_event_code_t codigo = lv_event_get_code(e);
  if (codigo == LV_EVENT_PRESSED) {
    apretada = true;
    apretadaDesde = lv_tick_get();
    lv_indev_get_point(lv_indev_active(), &dondeApretada);
    reinicioPedido = false;
  } else if (codigo == LV_EVENT_PRESSING) {
    lv_point_t punto;
    lv_indev_get_point(lv_indev_active(), &punto);
    if (LV_ABS(punto.x - dondeApretada.x) > QUIETO_PX ||
        LV_ABS(punto.y - dondeApretada.y) > QUIETO_PX) {
      apretada = false;
    }
  } else if (codigo == LV_EVENT_RELEASED) {
    reintento = reintento || (apretada && !reinicioPedido);
    apretada = false;
  } else if (codigo == LV_EVENT_PRESS_LOST) {
    apretada = false;
  }
}

// El auto, el texto de la derecha y lo apretado. También pide reiniciar: con
// la otra tarea atendiendo la pantalla, anda aunque loop() esté trabado.
static void alAnimarEspera(lv_timer_t *) {
  uint32_t ahora = lv_tick_get();
  bool visible = !lv_obj_has_flag(vistaEspera, LV_OBJ_FLAG_HIDDEN);
  entrada.mover(ahora, visible);

  uint32_t tiempo = apretada && visible ? ahora - apretadaDesde : 0;
  if (tiempo >= MANTENER_REINICIO_MS && !reinicioPedido) {
    reinicioPedido = true;
    if (alMantener != nullptr) {
      alMantener();
    }
  }
  const char *pista = entrada.quieto() ? "Toca para reintentar" : "";
  if (reinicioPedido && apretada) {
    pista = "Reiniciando...";
  } else if (tiempo >= AVISO_MANTENER_MS) {
    pista = "Mantener para reiniciar";
  }
  ponerTexto(pistaEspera, pista);
  int32_t lleno = tiempo >= AVISO_MANTENER_MS
                      ? lroundf(ANCHO * fminf(1, (float)tiempo / MANTENER_REINICIO_MS))
                      : 0;
  mostrar(rellenoReinicio, lleno > 0);
  if (lleno > 0) {
    ponerAncho(rellenoReinicio, lleno);
  }
}

static void ponerAzul(bool azul) {
  enAzul = azul;
  ponerFondo(capaCambio, azul ? AZUL : lv_color_black());
  ponerColor(textoCambio, azul ? lv_color_black() : AZUL);
}

static void alParpadear(lv_timer_t *) {
  ponerAzul(!enAzul);
}

static lv_obj_t *textoEspera(lv_color_t color, lv_align_t alineacion, int32_t x) {
  lv_obj_t *e = etiqueta(vistaEspera, &lv_font_montserrat_20, color);
  lv_obj_align(e, alineacion, x, yDeBase(&lv_font_montserrat_20, BASE_ESPERA));
  return e;
}

static void armarEspera(lv_obj_t *raiz) {
  vistaEspera = caja(raiz, 0, 0, ANCHO, ALTO);
  lv_obj_add_flag(vistaEspera, LV_OBJ_FLAG_CLICKABLE);
  static const lv_event_code_t EVENTOS[] = {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED,
                                            LV_EVENT_PRESS_LOST};
  for (lv_event_code_t codigo : EVENTOS) {
    lv_obj_add_event_cb(vistaEspera, alApretarEspera, codigo, nullptr);
  }
  entrada.crear(vistaEspera, lv_tick_get() + ESPERA_ENTRADA_MS, Y_AUTO);
  estadoEspera = textoEspera(ETIQUETA, LV_ALIGN_TOP_LEFT, MARGEN);
  pistaEspera = textoEspera(ETIQUETA, LV_ALIGN_TOP_RIGHT, -MARGEN);
  relojEspera = etiqueta(vistaEspera, &lv_font_montserrat_14, ETIQUETA);
  lv_obj_align(relojEspera, LV_ALIGN_TOP_RIGHT, -MARGEN, 4);
  rellenoReinicio = rectangulo(vistaEspera, 0, Y_REINICIO, 0, ALTO_REINICIO, ROJO);
  lv_obj_add_flag(rellenoReinicio, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(vistaEspera, LV_OBJ_FLAG_HIDDEN);
  lv_timer_create(alAnimarEspera, entrada_ms_cuadro, nullptr);
}

void Pantalla::iniciar() {
  if (!hw.iniciada()) {
    return;
  }
  lv_obj_t *raiz = lv_screen_active();
  lv_obj_remove_flag(raiz, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(raiz, lv_color_black(), 0);

  fondoBarra = rectangulo(raiz, 0, 0, ANCHO, ALTO_BARRA, FONDO_BARRA);
  // Antes que el título y los puntos, para quedar abajo
  rellenoBarra = rectangulo(fondoBarra, 0, 0, 0, ALTO_BARRA, ROJO_OSCURO);
  lv_obj_add_flag(rellenoBarra, LV_OBJ_FLAG_HIDDEN);
  titulo = etiqueta(fondoBarra, &lv_font_montserrat_20, BLANCO);
  lv_obj_align(titulo, LV_ALIGN_LEFT_MID, MARGEN, 0);
  for (lv_obj_t *&punto : puntos) {
    punto =
        rectangulo(fondoBarra, 0, (ALTO_BARRA - PUNTO) / 2, PUNTO, PUNTO, PUNTO_APAGADO, PUNTO / 2);
    lv_obj_add_flag(punto, LV_OBJ_FLAG_HIDDEN);
  }
  marcaSd = rectangulo(fondoBarra, 0, 0, ANCHO_MARCA, ALTO_MARCA, ETIQUETA, 3);
  lv_obj_t *sd = etiqueta(marcaSd, &lv_font_montserrat_14, lv_color_black());
  lv_label_set_text(sd, "SD");
  lv_obj_center(sd);
  lv_obj_add_flag(marcaSd, LV_OBJ_FLAG_HIDDEN);
  relojBarra = etiqueta(fondoBarra, &lv_font_montserrat_20, BLANCO);

  vistaEstado = caja(raiz, 0, ALTO_BARRA, ANCHO, ALTO - ALTO_BARRA);
  for (int i = 0; i < RENGLONES; i++) {
    textos[i] = etiqueta(vistaEstado, &lv_font_montserrat_20, BLANCO);
    lv_obj_align(textos[i], LV_ALIGN_TOP_LEFT, MARGEN,
                 i * ALTO_RENGLON + (ALTO_RENGLON - lv_font_montserrat_20.line_height) / 2);
  }

  vistaPaginas = caja(raiz, 0, ALTO_BARRA, ANCHO, ALTO - ALTO_BARRA);
  vistaCompleta = caja(raiz, 0, 0, ANCHO, ALTO);
  armarEspera(raiz);
  // Arriba de todo: con una alarma, también sobre la página completa
  lv_obj_move_to_index(fondoBarra, -1);
  // Y más arriba, el aviso de cambio
  capaCambio = rectangulo(raiz, 0, 0, ANCHO, ALTO, AZUL);
  textoCambio = etiqueta(capaCambio, &lv_font_montserrat_48, lv_color_black());
  lv_label_set_text(textoCambio, "CAMBIO");
  lv_obj_center(textoCambio);
  lv_obj_add_flag(capaCambio, LV_OBJ_FLAG_HIDDEN);
  parpadeo = lv_timer_create(alParpadear, PARPADEO_CAMBIO_MS, nullptr);
  lv_timer_pause(parpadeo);
  lv_timer_create(alActualizarHora, PERIODO_RELOJ_MS, nullptr);
  alActualizarHora(nullptr);
  lista = true;
  ver(vistaEspera);
}

void Pantalla::alPedirHora(bool (*funcion)(char *texto, size_t tam)) {
  pedirHora = funcion;
  if (lista) {
    alActualizarHora(nullptr);
  }
}

void Pantalla::espera(const char *estado, bool problema) {
  if (!lista) {
    return;
  }
  ver(vistaEspera);
  ponerTexto(estadoEspera, estado);
  ponerColor(estadoEspera, problema ? AMBAR : ETIQUETA);
}

void Pantalla::avisoCambio(bool activo) {
  if (!lista || activo == !lv_obj_has_flag(capaCambio, LV_OBJ_FLAG_HIDDEN)) {
    return;
  }
  mostrar(capaCambio, activo);
  if (activo) {
    ponerAzul(true);
    lv_timer_reset(parpadeo);
    lv_timer_resume(parpadeo);
  } else {
    lv_timer_pause(parpadeo);
  }
}

bool Pantalla::tomarReintento() {
  bool pedido = reintento;
  reintento = false;
  return pedido;
}

void Pantalla::alMantenerApretada(void (*funcion)()) {
  alMantener = funcion;
}

void Pantalla::alimentacionBaja() {
  if (!lista) {
    return;
  }
  const char *const lineas[RENGLONES] = {"Se reinicio por falta de corriente (brownout)",
                                         "Revisar la alimentacion de 5 V", "", ""};
  armarEstado("ALIMENTACION", ROJO, lineas);
}

void Pantalla::descubriendo(uint16_t hechos, uint16_t total) {
  if (!lista) {
    return;
  }
  char paso[24];
  snprintf(paso, sizeof(paso), "Paso %u/%u", hechos + 1, total);
  const char *const lineas[RENGLONES] = {"Consultando la ECU...", "", "", paso};
  armarEstado("DESCUBRIENDO", CIAN, lineas);
}

void Pantalla::pagina(const char *texto, uint8_t numero, uint8_t total, float mantenida,
                      const char *alarma, MarcaSd sd) {
  if (!lista) {
    return;
  }
  barra(texto, mantenida > 0 ? ROJO_CLARO : BLANCO, numero,
        total < MAX_PAGINAS ? total : MAX_PAGINAS, mantenida, alarma, sd);
  ver(vistaPaginas);
}

lv_obj_t *Pantalla::areaPaginas() const {
  return vistaPaginas;
}

void Pantalla::paginaCompleta(uint8_t numero, uint8_t total, const char *alarma, MarcaSd sd) {
  if (!lista) {
    return;
  }
  if (alarma != nullptr) {
    barra("", BLANCO, numero, total < MAX_PAGINAS ? total : MAX_PAGINAS, 0, alarma, sd);
  }
  ver(vistaCompleta, alarma != nullptr);
}

lv_obj_t *Pantalla::areaCompleta() const {
  return vistaCompleta;
}

lv_obj_t *Pantalla::areaEspera() const {
  return vistaEspera;
}
