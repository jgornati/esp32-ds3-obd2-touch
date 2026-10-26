#include "pantalla.h"

#include <stdio.h>
#include <string.h>

#include <lvgl.h>

#include "config.h"

// 640x172: barra de ALTO_BARRA y debajo RENGLONES líneas de ALTO_RENGLON.
static const int ANCHO = 640;
static const int ALTO = 172;
static const int ALTO_BARRA = 28;
static const int RENGLONES = 4;
static const int ALTO_RENGLON = (ALTO - ALTO_BARRA) / RENGLONES;
static const int MARGEN = 8;
// Pantalla de datos: el número grande, alineado a la derecha en este ancho
// para que no salte al cambiar de cifras (4 cifras ocupan ~294 px), y la
// columna de las líneas.
static const int ANCHO_GRANDE = 310;
static const int COLUMNA_DATOS = 390;

// Sobre fondo negro, para la noche (los de la prueba de E1)
static const lv_color_t BLANCO = lv_color_hex(0xFFFFFF);
static const lv_color_t GRIS = lv_color_hex(0x707070);
static const lv_color_t VERDE = lv_color_hex(0x3CC864);
static const lv_color_t AMBAR = lv_color_hex(0xFFB000);
static const lv_color_t ROJO = lv_color_hex(0xFF3B30);
static const lv_color_t CIAN = lv_color_hex(0x40C8E0);
static const lv_color_t FONDO_BARRA = lv_color_hex(0x1A1A1A);

static const LineaDato VACIA = {"", "", true};

static lv_obj_t *titulo = nullptr;
static lv_obj_t *derecha = nullptr;
// Pantallas de estado: RENGLONES líneas de texto y valor
static lv_obj_t *vistaEstado = nullptr;
static lv_obj_t *textos[RENGLONES];
static lv_obj_t *valores[RENGLONES];
// Pantalla de datos
static lv_obj_t *vistaDatos = nullptr;
static lv_obj_t *grande = nullptr;
static lv_obj_t *unidadGrande = nullptr;
static lv_obj_t *etiquetasDatos[RENGLONES];
static lv_obj_t *valoresDatos[RENGLONES];

// Cambiar el texto, el color o la visibilidad hace que LVGL redibuje y
// mande la imagen entera (~33 ms): solo se tocan si cambian.
static void ponerTexto(lv_obj_t *e, const char *texto) {
  if (strcmp(lv_label_get_text(e), texto) != 0) {
    lv_label_set_text(e, texto);
  }
}

static void ponerColor(lv_obj_t *e, lv_color_t color) {
  if (!lv_color_eq(lv_obj_get_style_text_color(e, 0), color)) {
    lv_obj_set_style_text_color(e, color, 0);
  }
}

static void ponerFuente(lv_obj_t *e, const lv_font_t *fuente) {
  if (lv_obj_get_style_text_font(e, 0) != fuente) {
    lv_obj_set_style_text_font(e, fuente, 0);
  }
}

static void mostrar(lv_obj_t *obj, bool visible) {
  bool oculto = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
  if (visible && oculto) {
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else if (!visible && !oculto) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

static lv_obj_t *caja(lv_obj_t *padre, int y, int alto) {
  lv_obj_t *obj = lv_obj_create(padre);
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(obj, 0, y);
  lv_obj_set_size(obj, ANCHO, alto);
  return obj;
}

static lv_obj_t *etiqueta(lv_obj_t *padre, const lv_font_t *fuente, lv_color_t color) {
  lv_obj_t *e = lv_label_create(padre);
  lv_obj_set_style_text_font(e, fuente, 0);
  lv_obj_set_style_text_color(e, color, 0);
  lv_label_set_text(e, "");
  return e;
}

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

static void barra(const char *texto, lv_color_t color, const char *textoDerecha) {
  ponerTexto(titulo, texto);
  ponerColor(titulo, color);
  ponerTexto(derecha, textoDerecha);
}

static void ponerLinea(lv_obj_t *texto, lv_obj_t *valor, const LineaDato &linea) {
  ponerTexto(texto, linea.etiqueta);
  ponerTexto(valor, linea.valor);
  ponerColor(valor, linea.actual ? BLANCO : GRIS);
}

static void armarEstado(const char *texto, lv_color_t color, const LineaDato (&lineas)[RENGLONES]) {
  barra(texto, color, "");
  mostrar(vistaDatos, false);
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

// Los números grandes traen solo " %+,-./0-9:°": lo demás ("N/D") va en la
// letra normal.
static bool entraEnNumeros(const char *texto) {
  return strspn(texto, " %+,-./0123456789:") == strlen(texto);
}

void Pantalla::iniciar() {
  if (!hw.iniciada()) {
    return;
  }
  lv_obj_t *raiz = lv_screen_active();
  lv_obj_remove_flag(raiz, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(raiz, lv_color_black(), 0);

  lv_obj_t *fondoBarra = caja(raiz, 0, ALTO_BARRA);
  lv_obj_set_style_bg_color(fondoBarra, FONDO_BARRA, 0);
  lv_obj_set_style_bg_opa(fondoBarra, LV_OPA_COVER, 0);
  titulo = etiqueta(fondoBarra, &lv_font_montserrat_20, BLANCO);
  lv_obj_align(titulo, LV_ALIGN_LEFT_MID, MARGEN, 0);
  derecha = etiqueta(fondoBarra, &lv_font_montserrat_20, GRIS);
  lv_obj_align(derecha, LV_ALIGN_RIGHT_MID, -MARGEN, 0);

  vistaEstado = caja(raiz, ALTO_BARRA, ALTO - ALTO_BARRA);
  for (int i = 0; i < RENGLONES; i++) {
    renglon(vistaEstado, i, MARGEN, BLANCO, &textos[i], &valores[i]);
  }

  vistaDatos = caja(raiz, ALTO_BARRA, ALTO - ALTO_BARRA);
  lv_obj_add_flag(vistaDatos, LV_OBJ_FLAG_HIDDEN);
  grande = etiqueta(vistaDatos, &fuente_numeros_110, BLANCO);
  lv_obj_set_width(grande, ANCHO_GRANDE);
  lv_obj_set_style_text_align(grande, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_align(grande, LV_ALIGN_LEFT_MID, MARGEN, 0);
  // Abajo, a la altura de la base de los números
  unidadGrande = etiqueta(vistaDatos, &lv_font_montserrat_20, GRIS);
  lv_obj_align(unidadGrande, LV_ALIGN_LEFT_MID, MARGEN + ANCHO_GRANDE + MARGEN, 26);
  for (int i = 0; i < RENGLONES; i++) {
    renglon(vistaDatos, i, COLUMNA_DATOS, GRIS, &etiquetasDatos[i], &valoresDatos[i]);
  }
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

void Pantalla::sinConexion(const char *motivo, uint32_t segundos) {
  if (!lista) {
    return;
  }
  char reintento[24];
  textoReintento(segundos, reintento, sizeof(reintento));
  armarEstado("SIN CONEXION", ROJO, {{motivo, "", true}, VACIA, VACIA, {reintento, "", true}});
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

void Pantalla::datos(const char *version, const LineaDato &principal, const LineaDato *lineas,
                     uint8_t cantidad) {
  if (!lista) {
    return;
  }
  char elm[20];
  snprintf(elm, sizeof(elm), "ELM %s", version);
  barra("ECU OK", VERDE, elm);
  mostrar(vistaEstado, false);
  mostrar(vistaDatos, true);

  ponerFuente(grande,
              entraEnNumeros(principal.valor) ? &fuente_numeros_110 : &lv_font_montserrat_20);
  ponerTexto(grande, principal.valor);
  ponerColor(grande, principal.actual ? BLANCO : GRIS);
  ponerTexto(unidadGrande, principal.etiqueta);
  for (int i = 0; i < RENGLONES; i++) {
    ponerLinea(etiquetasDatos[i], valoresDatos[i], i < cantidad ? lineas[i] : VACIA);
  }
}
