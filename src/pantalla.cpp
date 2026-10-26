#include "pantalla.h"

#include <stdio.h>

#include "config.h"
#include "dibujo.h"

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

static const LineaDato VACIA = {"", "", true};

static lv_obj_t *titulo = nullptr;
static lv_obj_t *derecha = nullptr;
static lv_obj_t *puntos[MAX_PAGINAS];
// Pantallas de estado: RENGLONES líneas de texto y valor
static lv_obj_t *vistaEstado = nullptr;
static lv_obj_t *textos[RENGLONES];
static lv_obj_t *valores[RENGLONES];
// Las arma Paginas
static lv_obj_t *vistaPaginas = nullptr;

// Arriba del renglón, con la letra centrada en su alto
static int yRenglon(int numero) {
  return numero * ALTO_RENGLON + (ALTO_RENGLON - lv_font_montserrat_20.line_height) / 2;
}

static void renglon(lv_obj_t *padre, int numero, int x, lv_color_t colorTexto, lv_obj_t **texto,
                    lv_obj_t **valor) {
  *texto = etiqueta(padre, &lv_font_montserrat_20, colorTexto);
  lv_obj_align(*texto, LV_ALIGN_TOP_LEFT, x, yRenglon(numero));
  *valor = etiqueta(padre, &lv_font_montserrat_20, BLANCO);
  lv_obj_align(*valor, LV_ALIGN_TOP_RIGHT, -MARGEN, yRenglon(numero));
}

// Con total > 0, un punto por página pegado al margen derecho, numero en blanco
static void barra(const char *texto, lv_color_t color, const char *textoDerecha, uint8_t numero = 0,
                  uint8_t total = 0) {
  ponerTexto(titulo, texto);
  ponerColor(titulo, color);
  ponerTexto(derecha, textoDerecha);
  int x = ANCHO - MARGEN - total * (PUNTO + ENTRE_PUNTOS) + ENTRE_PUNTOS;
  for (uint8_t i = 0; i < MAX_PAGINAS; i++) {
    bool visible = i < total;
    mostrar(puntos[i], visible);
    if (visible) {
      ponerPosicion(puntos[i], x + i * (PUNTO + ENTRE_PUNTOS), (ALTO_BARRA - PUNTO) / 2);
      ponerFondo(puntos[i], i == numero ? BLANCO : PUNTO_APAGADO);
    }
  }
}

static void ponerLinea(lv_obj_t *texto, lv_obj_t *valor, const LineaDato &linea) {
  ponerTexto(texto, linea.etiqueta);
  ponerTexto(valor, linea.valor);
  ponerColor(valor, linea.actual ? BLANCO : GRIS);
}

static void armarEstado(const char *texto, lv_color_t color, const LineaDato (&lineas)[RENGLONES]) {
  barra(texto, color, "");
  mostrar(vistaPaginas, false);
  mostrar(vistaEstado, true);
  for (int i = 0; i < RENGLONES; i++) {
    ponerLinea(textos[i], valores[i], lineas[i]);
  }
}

// "Reintento en 5 s"; vacío con 0
static void textoReintento(uint32_t segundos, char *salida, size_t tam) {
  if (segundos == 0) {
    salida[0] = '\0';
    return;
  }
  snprintf(salida, tam, "Reintento en %lu s", (unsigned long)segundos);
}

void Pantalla::iniciar() {
  if (!hw.iniciada()) {
    return;
  }
  lv_obj_t *raiz = lv_screen_active();
  lv_obj_remove_flag(raiz, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(raiz, lv_color_black(), 0);

  lv_obj_t *fondoBarra = rectangulo(raiz, 0, 0, ANCHO, ALTO_BARRA, FONDO_BARRA);
  titulo = etiqueta(fondoBarra, &lv_font_montserrat_20, BLANCO);
  lv_obj_align(titulo, LV_ALIGN_LEFT_MID, MARGEN, 0);
  derecha = etiqueta(fondoBarra, &lv_font_montserrat_20, GRIS);
  lv_obj_align(derecha, LV_ALIGN_RIGHT_MID, -MARGEN, 0);
  for (lv_obj_t *&punto : puntos) {
    punto =
        rectangulo(fondoBarra, 0, (ALTO_BARRA - PUNTO) / 2, PUNTO, PUNTO, PUNTO_APAGADO, PUNTO / 2);
    lv_obj_add_flag(punto, LV_OBJ_FLAG_HIDDEN);
  }

  vistaEstado = caja(raiz, 0, ALTO_BARRA, ANCHO, ALTO - ALTO_BARRA);
  for (int i = 0; i < RENGLONES; i++) {
    renglon(vistaEstado, i, MARGEN, BLANCO, &textos[i], &valores[i]);
  }

  vistaPaginas = caja(raiz, 0, ALTO_BARRA, ANCHO, ALTO - ALTO_BARRA);
  lv_obj_add_flag(vistaPaginas, LV_OBJ_FLAG_HIDDEN);
  lista = true;
}

void Pantalla::inicio() {
  if (!lista) {
    return;
  }
  armarEstado("DS3 OBDII", BLANCO, {{"Iniciando...", "", true}, VACIA, VACIA, VACIA});
}

void Pantalla::alimentacionBaja() {
  if (!lista) {
    return;
  }
  armarEstado("ALIMENTACION", ROJO,
              {{"Se reinicio por falta de corriente (brownout)", "", true},
               {"Revisar la alimentacion de 5 V", "", true},
               VACIA,
               VACIA});
}

void Pantalla::conectando(bool buscando) {
  if (!lista) {
    return;
  }
  char texto[48];
  char hasta[16];
  if (buscando) {
    snprintf(texto, sizeof(texto), "Buscando el adaptador %s...", BLE_NOMBRE_REMOTO);
    snprintf(hasta, sizeof(hasta), "(hasta %lu s)", (unsigned long)BUSQUEDA_ELM_S);
    armarEstado("BUSCANDO ADAPTADOR", CIAN, {{texto, "", true}, VACIA, VACIA, {hasta, "", true}});
  } else {
    snprintf(texto, sizeof(texto), "Conectando con %s", BLE_NOMBRE_REMOTO);
    snprintf(hasta, sizeof(hasta), "(hasta %lu s)", (unsigned long)(TIMEOUT_CONEXION_MS / 1000));
    armarEstado(
        "CONECTANDO", CIAN,
        {{texto, "", true}, {"por la direccion guardada...", "", true}, VACIA, {hasta, "", true}});
  }
  hw.dibujarYa();
}

void Pantalla::sinAdaptador(uint32_t segundos) {
  if (!lista) {
    return;
  }
  char abajo[24] = "Buscando...";
  if (segundos > 0) {
    textoReintento(segundos, abajo, sizeof(abajo));
  }
  armarEstado("SIN ADAPTADOR", ETIQUETA,
              {{"Se prende con el contacto", "", true}, VACIA, VACIA, {abajo, "", true}});
}

void Pantalla::iniciandoElm(const char *comando) {
  if (!lista) {
    return;
  }
  char texto[24];
  snprintf(texto, sizeof(texto), "> %s", comando);
  armarEstado("INICIANDO ELM", CIAN,
              {{"Configurando el ELM327", "", true}, VACIA, VACIA, {texto, "", true}});
}

void Pantalla::elmNoResponde(uint32_t segundos) {
  if (!lista) {
    return;
  }
  char reintento[24];
  textoReintento(segundos, reintento, sizeof(reintento));
  armarEstado("INICIANDO ELM", AMBAR,
              {{"El ELM no responde", "", true}, VACIA, VACIA, {reintento, "", true}});
}

void Pantalla::buscandoEcu(const char *version) {
  if (!lista) {
    return;
  }
  char elm[20];
  snprintf(elm, sizeof(elm), "ELM %s", version);
  char hasta[16];
  snprintf(hasta, sizeof(hasta), "(hasta %lu s)", (unsigned long)(TIMEOUT_BUSCAR_ECU_MS / 1000));
  armarEstado(
      "BUSCANDO ECU", CIAN,
      {{elm, "", true}, {"Buscando la ECU del auto...", "", true}, VACIA, {hasta, "", true}});
}

void Pantalla::sinEcu(const char *version, const LineaDato &voltaje, const char *motivo,
                      uint32_t segundos) {
  if (!lista) {
    return;
  }
  char elm[20];
  snprintf(elm, sizeof(elm), "ELM %s", version);
  char reintento[24];
  textoReintento(segundos, reintento, sizeof(reintento));
  armarEstado(
      "SIN ECU", AMBAR,
      {{elm, voltaje.valor, voltaje.actual}, {motivo, "", true}, VACIA, {reintento, "", true}});
}

void Pantalla::descubriendo(uint16_t hechos, uint16_t total) {
  if (!lista) {
    return;
  }
  char paso[24];
  snprintf(paso, sizeof(paso), "Paso %u/%u", hechos + 1, total);
  armarEstado("DESCUBRIENDO", CIAN,
              {{"Consultando la ECU...", "", true}, VACIA, VACIA, {paso, "", true}});
}

void Pantalla::pagina(const char *texto, uint8_t numero, uint8_t total) {
  if (!lista) {
    return;
  }
  barra(texto, BLANCO, "", numero, total < MAX_PAGINAS ? total : MAX_PAGINAS);
  mostrar(vistaEstado, false);
  mostrar(vistaPaginas, true);
}

lv_obj_t *Pantalla::areaPaginas() const {
  return vistaPaginas;
}
