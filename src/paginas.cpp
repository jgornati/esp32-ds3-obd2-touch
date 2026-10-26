#include "paginas.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <respuesta.h>

#include "config.h"
#include "dibujo.h"

enum : uint8_t {
  PAGINA_MOTOR,
  PAGINA_AIRE,
  PAGINA_CARGA,
  PAGINA_MEZCLA,
  PAGINA_FALLAS,
  PAGINA_INFO,
  CANT_PAGINAS
};

static const Magnitud DE_MOTOR[] = {RPM, REFRIGERANTE, VELOCIDAD};
static const Magnitud DE_AIRE[] = {ADMISION, VOLT_ELM};
static const Magnitud DE_CARGA[] = {CARGA, AVANCE, ACELERADOR};
static const Magnitud DE_MEZCLA[] = {LAZO, AJ_CORTO, AJ_LARGO, LAMBDA, SONDA_POST};

struct Definicion {
  const char *titulo;
  const Magnitud *magnitudes;
  uint8_t cantidad;
};

template <size_t N>
static constexpr uint8_t largo(const Magnitud (&)[N]) {
  static_assert(N <= MAX_VISIBLES, "una página tiene que entrar en una consulta");
  return N;
}

// En el orden del enum
static const Definicion PAGINAS[] = {
    {"MOTOR", DE_MOTOR, largo(DE_MOTOR)},
    {"AIRE/VOLT", DE_AIRE, largo(DE_AIRE)},
    {"CARGA", DE_CARGA, largo(DE_CARGA)},
    {"MEZCLA", DE_MEZCLA, largo(DE_MEZCLA)},
    {"FALLAS", nullptr, 0},
    {"INFO", nullptr, 0},
};
static_assert(sizeof(PAGINAS) / sizeof(PAGINAS[0]) == CANT_PAGINAS,
              "PAGINAS tiene que seguir el orden del enum");
static_assert(CANT_PAGINAS <= MAX_PAGINAS, "un punto por página en la barra");

// Escalas de las barras
static const float RPM_MAXIMA = 7000;
static const float RPM_ZONA_ROJA = 6000;
static const float ADMISION_MAXIMA = 80; // °C, desde 0
static const float AVANCE_MINIMO = -10;  // grados
static const float AVANCE_MAXIMO = 40;
static const float SONDA_MAXIMA = 1; // V, desde 0
// Lambda: rica a la izquierda, pobre a la derecha y 1 en el medio. La
// marca va en verde hasta LAMBDA_TOLERANCIA de 1; más allá, ámbar o cian.
static const float LAMBDA_RICA = 0.8f;
static const float LAMBDA_POBRE = 1.2f;
static const float LAMBDA_TOLERANCIA = 0.03f;

static const lv_color_t ZONA_ROJA = lv_color_hex(0x5A1A17);
static const lv_color_t FONDO_RICA = lv_color_hex(0x3D2A00);
static const lv_color_t FONDO_POBRE = lv_color_hex(0x0E3238);

// Medidas de la maqueta de E4, en el área de 640x144 debajo de la barra.
// Las y de los textos son de la base de las letras: así quedan alineados
// aunque cambie la fuente.
static const int32_t ANCHO = 640;
static const int32_t ALTO = 144;
static const int32_t MARGEN = 8;
static const int32_t BORDE_DERECHO = ANCHO - MARGEN;
// Motor y Aire: el número grande alineado a la derecha en 310 px, para que
// no salte al cambiar de cifras, y una barra a lo ancho abajo
static const int32_t BORDE_GRANDE = MARGEN + 310;
static const int32_t BASE_GRANDE = 89;
static const int32_t X_JUNTO_GRANDE = 326;
static const int32_t Y_BARRA_ANCHA = 106;
static const int32_t ALTO_BARRA_ANCHA = 12;
static const int32_t ANCHO_BARRA_ANCHA = ANCHO - 2 * MARGEN;
static const int32_t BASE_ESCALA_ANCHA = 136;
// Carga: tres columnas
static const int32_t ANCHO_COLUMNA = ANCHO / 3;
static const int32_t RELLENO_COLUMNA = 16;
static const int32_t ANCHO_EN_COLUMNA = ANCHO_COLUMNA - 2 * RELLENO_COLUMNA;
// Mezcla: la columna de la derecha
static const int32_t X_DERECHA = 332;
static const int32_t ANCHO_DERECHA = BORDE_DERECHO - X_DERECHA;

static const lv_font_t *const CHICA = &lv_font_montserrat_14;
static const lv_font_t *const NORMAL = &lv_font_montserrat_20;

static lv_obj_t *area = nullptr;
static lv_obj_t *vistas[CANT_PAGINAS];
static int8_t toque = 0;

enum class Alineacion { IZQUIERDA, CENTRO, DERECHA };

// Un texto que no cambia. x es su borde izquierdo, su centro o su borde derecho.
static lv_obj_t *fijo(lv_obj_t *padre, const char *texto, const lv_font_t *fuente, lv_color_t color,
                      int32_t x, int32_t base, Alineacion alineacion = Alineacion::IZQUIERDA) {
  lv_obj_t *e = etiqueta(padre, fuente, color);
  lv_label_set_text(e, texto);
  if (alineacion != Alineacion::IZQUIERDA) {
    int32_t ancho = lv_text_get_width(texto, strlen(texto), fuente, 0);
    x -= alineacion == Alineacion::CENTRO ? ancho / 2 : ancho;
  }
  lv_obj_set_pos(e, x, yDeBase(fuente, base));
  return e;
}

// Un valor con su unidad aparte, más chica y en gris, que se ve solo con un
// valor actual. Pegado a la derecha (la unidad queda fija y el número se
// corre) o a la izquierda (la unidad sigue al número).
struct Valor {
  lv_obj_t *numero;
  lv_obj_t *unidad; // nullptr si no tiene
  const lv_font_t *fuente;
  int32_t x; // borde derecho o izquierdo
  int32_t base;
  bool aLaDerecha;
  int32_t anchoUnidad;
};

static Valor crearValor(lv_obj_t *padre, const lv_font_t *fuente, int32_t x, int32_t base,
                        bool aLaDerecha, const char *unidad = nullptr) {
  Valor v = {etiqueta(padre, fuente, BLANCO), nullptr, fuente, x, base, aLaDerecha, 0};
  if (unidad != nullptr) {
    v.unidad = etiqueta(padre, NORMAL, ETIQUETA);
    lv_label_set_text(v.unidad, unidad);
    lv_obj_add_flag(v.unidad, LV_OBJ_FLAG_HIDDEN);
    v.anchoUnidad = lv_text_get_width(unidad, strlen(unidad), NORMAL, 0);
  }
  return v;
}

// Los números grandes traen solo " %+,-./0-9:°": lo demás ("N/D") va en
// Montserrat 48.
static bool entraEnNumeros(const char *texto) {
  return strspn(texto, " %+,-./0123456789:") == strlen(texto);
}

static void ponerValor(Valor &v, const char *texto, bool actual) {
  const lv_font_t *fuente = v.fuente;
  if (fuente == &fuente_numeros_110 && !entraEnNumeros(texto)) {
    fuente = &lv_font_montserrat_48;
  }
  ponerFuente(v.numero, fuente);
  ponerTexto(v.numero, texto);
  ponerColor(v.numero, actual ? BLANCO : GRIS);

  int32_t ancho = lv_text_get_width(texto, strlen(texto), fuente, 0);
  bool conUnidad = v.unidad != nullptr && actual;
  int32_t x = v.aLaDerecha ? v.x - ancho - (conUnidad ? v.anchoUnidad : 0) : v.x;
  ponerPosicion(v.numero, x, yDeBase(fuente, v.base));
  if (v.unidad != nullptr) {
    mostrar(v.unidad, conUnidad);
    ponerPosicion(v.unidad, x + ancho, yDeBase(NORMAL, v.base));
  }
}

// El texto de la magnitud según las convenciones de pantalla. true si es un
// valor actual, y entonces queda en valor.
static bool ponerMagnitud(Valor &v, Magnitud m, uint32_t ahora, float *valor = nullptr) {
  char texto[16];
  bool actual = textoValor(m, ahora, texto, sizeof(texto));
  ponerValor(v, texto, actual);
  float leido;
  return actual && valorActual(m, ahora, valor != nullptr ? valor : &leido);
}

// Fondo redondeado y un relleno blanco, que va de desde a hasta (de 0 a 1
// sobre el ancho). Sin un valor actual queda vacía.
struct Barra {
  lv_obj_t *relleno;
  int32_t x;
  int32_t y;
  int32_t ancho;
};

static Barra crearBarra(lv_obj_t *padre, int32_t x, int32_t y, int32_t ancho, int32_t alto) {
  rectangulo(padre, x, y, ancho, alto, FONDO_ESCALA, alto / 2);
  lv_obj_t *relleno = rectangulo(padre, x, y, 0, alto, BLANCO, alto / 2);
  lv_obj_add_flag(relleno, LV_OBJ_FLAG_HIDDEN);
  return {relleno, x, y, ancho};
}

static void ponerBarra(const Barra &b, bool actual, float desde, float hasta) {
  if (hasta < desde) {
    float otro = desde;
    desde = hasta;
    hasta = otro;
  }
  int32_t inicio = lroundf(desde * b.ancho);
  int32_t fin = lroundf(hasta * b.ancho);
  bool visible = actual && fin > inicio;
  mostrar(b.relleno, visible);
  if (visible) {
    ponerPosicion(b.relleno, b.x + inicio, b.y);
    ponerAncho(b.relleno, fin - inicio);
  }
}

// Dónde cae valor entre minimo y maximo, de 0 a 1
static float fraccion(float valor, float minimo, float maximo) {
  float f = (valor - minimo) / (maximo - minimo);
  return f < 0 ? 0 : (f > 1 ? 1 : f);
}

// Motor: RPM en grande con una barra de 0 a RPM_MAXIMA, como una aguja

static Valor rpm, refrigerante, velocidad;
static Barra barraRpm;

static void armarMotor(lv_obj_t *v) {
  rpm = crearValor(v, &fuente_numeros_110, BORDE_GRANDE, BASE_GRANDE, true);
  fijo(v, "RPM", NORMAL, ETIQUETA, X_JUNTO_GRANDE, 81);
  fijo(v, "Refrig.", NORMAL, ETIQUETA, 400, 43);
  refrigerante = crearValor(v, &lv_font_montserrat_36, BORDE_DERECHO, 43, true, " °C");
  fijo(v, "Vel.", NORMAL, ETIQUETA, 400, 93);
  velocidad = crearValor(v, &lv_font_montserrat_36, BORDE_DERECHO, 93, true, " km/h");

  barraRpm = crearBarra(v, MARGEN, Y_BARRA_ANCHA, ANCHO_BARRA_ANCHA, ALTO_BARRA_ANCHA);
  // Zona roja debajo del relleno, con el extremo derecho redondeado como el fondo
  int32_t zona = lroundf(ANCHO_BARRA_ANCHA * RPM_ZONA_ROJA / RPM_MAXIMA);
  rectangulo(v, MARGEN + zona, Y_BARRA_ANCHA, ANCHO_BARRA_ANCHA - zona - ALTO_BARRA_ANCHA / 2,
             ALTO_BARRA_ANCHA, ZONA_ROJA);
  rectangulo(v, BORDE_DERECHO - ALTO_BARRA_ANCHA, Y_BARRA_ANCHA, ALTO_BARRA_ANCHA, ALTO_BARRA_ANCHA,
             ZONA_ROJA, ALTO_BARRA_ANCHA / 2);
  lv_obj_move_to_index(barraRpm.relleno, -1);

  for (int miles = 1; miles * 1000 < RPM_MAXIMA; miles++) {
    char texto[8];
    snprintf(texto, sizeof(texto), "%d", miles);
    fijo(v, texto, CHICA, ETIQUETA, MARGEN + lroundf(ANCHO_BARRA_ANCHA * miles * 1000 / RPM_MAXIMA),
         BASE_ESCALA_ANCHA, Alineacion::CENTRO);
  }
  fijo(v, "x1000", CHICA, ETIQUETA, BORDE_DERECHO, BASE_ESCALA_ANCHA, Alineacion::DERECHA);
}

static void dibujarMotor(uint32_t ahora) {
  float valor = 0;
  bool actual = ponerMagnitud(rpm, RPM, ahora, &valor);
  ponerBarra(barraRpm, actual, 0, fraccion(valor, 0, RPM_MAXIMA));
  ponerMagnitud(refrigerante, REFRIGERANTE, ahora);
  ponerMagnitud(velocidad, VELOCIDAD, ahora);
}

// Aire/Volt: como Motor, con la admisión en grande y el voltaje del ELM

static Valor admision, voltaje;
static Barra barraAdmision;

static void armarAire(lv_obj_t *v) {
  admision = crearValor(v, &fuente_numeros_110, BORDE_GRANDE, BASE_GRANDE, true);
  // Dos renglones a 24 px, como en la maqueta
  lv_obj_t *nombre = fijo(v, "°C\nadmision", NORMAL, ETIQUETA, X_JUNTO_GRANDE, 57);
  lv_obj_set_style_text_line_space(nombre, 24 - NORMAL->line_height, 0);
  fijo(v, "Voltaje", NORMAL, ETIQUETA, BORDE_DERECHO, 33, Alineacion::DERECHA);
  voltaje = crearValor(v, &lv_font_montserrat_36, BORDE_DERECHO, 73, true, " V");

  barraAdmision = crearBarra(v, MARGEN, Y_BARRA_ANCHA, ANCHO_BARRA_ANCHA, ALTO_BARRA_ANCHA);
  fijo(v, "0", CHICA, ETIQUETA, MARGEN, BASE_ESCALA_ANCHA);
  for (int grados = 20; grados < ADMISION_MAXIMA; grados += 20) {
    char texto[8];
    snprintf(texto, sizeof(texto), "%d", grados);
    fijo(v, texto, CHICA, ETIQUETA, MARGEN + lroundf(ANCHO_BARRA_ANCHA * grados / ADMISION_MAXIMA),
         BASE_ESCALA_ANCHA, Alineacion::CENTRO);
  }
  fijo(v, "80 °C", CHICA, ETIQUETA, BORDE_DERECHO, BASE_ESCALA_ANCHA, Alineacion::DERECHA);
}

static void dibujarAire(uint32_t ahora) {
  float valor = 0;
  bool actual = ponerMagnitud(admision, ADMISION, ahora, &valor);
  ponerBarra(barraAdmision, actual, 0, fraccion(valor, 0, ADMISION_MAXIMA));
  ponerMagnitud(voltaje, VOLT_ELM, ahora);
}

// Carga: carga del motor, avance y acelerador, cada uno con su barra

static Valor carga, avance, acelerador;
static Barra barraCarga, barraAvance, barraAcelerador;

// Columna 0, 1 o 2: nombre, valor y barra. Devuelve dónde empieza.
static int32_t columna(lv_obj_t *v, uint8_t numero, const char *nombre, const char *unidad,
                       Valor &valor, Barra &barra) {
  int32_t x = numero * ANCHO_COLUMNA + RELLENO_COLUMNA;
  fijo(v, nombre, NORMAL, ETIQUETA, x, 27);
  valor = crearValor(v, &lv_font_montserrat_48, x, 78, false, unidad);
  barra = crearBarra(v, x, 100, ANCHO_EN_COLUMNA, 10);
  return x;
}

// Escala de 0 a 100 debajo de una barra de porcentaje
static void escalaPorcentaje(lv_obj_t *v, int32_t x) {
  fijo(v, "0", CHICA, ETIQUETA, x, 128);
  fijo(v, "100", CHICA, ETIQUETA, x + ANCHO_EN_COLUMNA, 128, Alineacion::DERECHA);
}

static void armarCarga(lv_obj_t *v) {
  rectangulo(v, ANCHO_COLUMNA, 0, 1, ALTO, FONDO_ESCALA);
  rectangulo(v, 2 * ANCHO_COLUMNA, 0, 1, ALTO, FONDO_ESCALA);

  escalaPorcentaje(v, columna(v, 0, "Carga", " %", carga, barraCarga));

  int32_t x = columna(v, 1, "Avance", " °", avance, barraAvance);
  // El relleno va desde el 0, marcado arriba de la barra
  int32_t cero = x + lroundf(ANCHO_EN_COLUMNA * fraccion(0, AVANCE_MINIMO, AVANCE_MAXIMO));
  rectangulo(v, cero - 1, 100, 2, 10, ETIQUETA);
  fijo(v, "-10", CHICA, ETIQUETA, x, 128);
  fijo(v, "0", CHICA, ETIQUETA, cero, 128, Alineacion::CENTRO);
  fijo(v, "40", CHICA, ETIQUETA, x + ANCHO_EN_COLUMNA, 128, Alineacion::DERECHA);

  escalaPorcentaje(v, columna(v, 2, "Acelerador", " %", acelerador, barraAcelerador));
}

static void dibujarCarga(uint32_t ahora) {
  float valor = 0;
  bool actual = ponerMagnitud(carga, CARGA, ahora, &valor);
  ponerBarra(barraCarga, actual, 0, fraccion(valor, 0, 100));
  actual = ponerMagnitud(avance, AVANCE, ahora, &valor);
  ponerBarra(barraAvance, actual, fraccion(0, AVANCE_MINIMO, AVANCE_MAXIMO),
             fraccion(valor, AVANCE_MINIMO, AVANCE_MAXIMO));
  actual = ponerMagnitud(acelerador, ACELERADOR, ahora, &valor);
  ponerBarra(barraAcelerador, actual, 0, fraccion(valor, 0, 100));
}

// Mezcla: lazo y ajustes a la izquierda; lambda (barra rica/pobre) y sonda
// post a la derecha

static Valor lazo, ajusteCorto, ajusteLargo, lambda, sondaPost;
static lv_obj_t *marcaLambda = nullptr;
static Barra barraSonda;

static void armarMezcla(lv_obj_t *v) {
  // Renglones de 48 px con la etiqueta y el valor centrados en su alto
  fijo(v, "Lazo", NORMAL, ETIQUETA, MARGEN, 31);
  lazo = crearValor(v, &lv_font_montserrat_28, 300, 34, true);
  fijo(v, "Ajuste corto", NORMAL, ETIQUETA, MARGEN, 79);
  ajusteCorto = crearValor(v, &lv_font_montserrat_28, 300, 82, true, " %");
  fijo(v, "Ajuste largo", NORMAL, ETIQUETA, MARGEN, 127);
  ajusteLargo = crearValor(v, &lv_font_montserrat_28, 300, 130, true, " %");
  rectangulo(v, 316, 6, 1, 132, FONDO_ESCALA);

  fijo(v, "Lambda", NORMAL, ETIQUETA, X_DERECHA, 29);
  lambda = crearValor(v, &lv_font_montserrat_28, BORDE_DERECHO, 29, true);
  // Mitad rica y mitad pobre, redondeadas solo en los extremos
  int32_t medio = X_DERECHA + ANCHO_DERECHA / 2;
  rectangulo(v, X_DERECHA, 42, 14, 14, FONDO_RICA, 7);
  rectangulo(v, X_DERECHA + 7, 42, medio - X_DERECHA - 7, 14, FONDO_RICA);
  rectangulo(v, medio, 42, BORDE_DERECHO - 7 - medio, 14, FONDO_POBRE);
  rectangulo(v, BORDE_DERECHO - 14, 42, 14, 14, FONDO_POBRE, 7);
  rectangulo(v, medio - 1, 38, 2, 22, ETIQUETA);
  marcaLambda = rectangulo(v, medio - 4, 36, 8, 26, VERDE, 3);
  lv_obj_add_flag(marcaLambda, LV_OBJ_FLAG_HIDDEN);
  fijo(v, "Rica", CHICA, AMBAR, X_DERECHA, 74);
  fijo(v, "1.00", CHICA, ETIQUETA, medio, 74, Alineacion::CENTRO);
  fijo(v, "Pobre", CHICA, CIAN, BORDE_DERECHO, 74, Alineacion::DERECHA);

  fijo(v, "Sonda post", NORMAL, ETIQUETA, X_DERECHA, 113);
  sondaPost = crearValor(v, &lv_font_montserrat_28, BORDE_DERECHO, 113, true, " V");
  barraSonda = crearBarra(v, X_DERECHA, 126, ANCHO_DERECHA, 10);
}

static void dibujarMezcla(uint32_t ahora) {
  ponerMagnitud(lazo, LAZO, ahora);
  ponerMagnitud(ajusteCorto, AJ_CORTO, ahora);
  ponerMagnitud(ajusteLargo, AJ_LARGO, ahora);

  float valor = 0;
  bool actual = ponerMagnitud(lambda, LAMBDA, ahora, &valor);
  mostrar(marcaLambda, actual);
  if (actual) {
    int32_t centro =
        X_DERECHA + lroundf(ANCHO_DERECHA * fraccion(valor, LAMBDA_RICA, LAMBDA_POBRE));
    ponerPosicion(marcaLambda, centro - 4, 36);
    lv_color_t color = VERDE;
    if (valor < 1 - LAMBDA_TOLERANCIA) {
      color = AMBAR;
    } else if (valor > 1 + LAMBDA_TOLERANCIA) {
      color = CIAN;
    }
    ponerFondo(marcaLambda, color);
  }

  actual = ponerMagnitud(sondaPost, SONDA_POST, ahora, &valor);
  ponerBarra(barraSonda, actual, 0, fraccion(valor, 0, SONDA_MAXIMA));
}

// Fallas: a la izquierda la luz y la cantidad de cada tipo, a la derecha los
// códigos de a RENGLONES_DTC. "Leyendo..." y "Sin fallas" van al centro.

static const lv_font_t *const CODIGO = &lv_font_montserrat_28;
static const int32_t X_SEPARADOR = 220;
static const int32_t X_CODIGOS = 236;
static const int32_t ALTO_RENGLON = 36; // 4 renglones; la maqueta tiene 3 de 44
// La descripción, 8 px más cerca del código que en la maqueta: así entra
// "Sens. mariposa (pendiente)"
static const int32_t X_DESCRIPCION = X_CODIGOS + 104 + 8;
static const int32_t ANCHO_DESCRIPCION = BORDE_DERECHO - X_DESCRIPCION;
// Las cantidades: guardados y pendientes; con permanentes, tres renglones
static const int32_t BASES_DOS[] = {110, 138};
static const int32_t BASES_TRES[] = {94, 116, 138};

static lv_obj_t *centroFallas, *columnasFallas;
static lv_obj_t *grande, *debajo;
static lv_obj_t *luz, *mensaje;
static lv_obj_t *cantidades[3]; // en el orden de TipoDtc
static lv_obj_t *codigos[RENGLONES_DTC], *descripciones[RENGLONES_DTC];
static char tituloFallas[16];

// Un texto centrado en el ancho de la página
static lv_obj_t *centrada(lv_obj_t *padre, const lv_font_t *fuente, lv_color_t color,
                          int32_t base) {
  lv_obj_t *e = etiqueta(padre, fuente, color);
  lv_obj_set_width(e, ANCHO);
  lv_obj_set_style_text_align(e, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(e, 0, yDeBase(fuente, base));
  return e;
}

static void armarFallas(lv_obj_t *v) {
  centroFallas = caja(v, 0, 0, ANCHO, ALTO);
  grande = centrada(centroFallas, &lv_font_montserrat_48, GRIS, 74);
  debajo = centrada(centroFallas, NORMAL, ETIQUETA, 111);

  columnasFallas = caja(v, 0, 0, ANCHO, ALTO);
  fijo(columnasFallas, "Luz de falla", NORMAL, ETIQUETA, MARGEN, 29);
  luz = etiqueta(columnasFallas, CODIGO, BLANCO);
  lv_obj_set_pos(luz, MARGEN, yDeBase(CODIGO, 65));
  for (lv_obj_t *&cantidad : cantidades) {
    cantidad = etiqueta(columnasFallas, NORMAL, ETIQUETA);
  }
  rectangulo(columnasFallas, X_SEPARADOR, 6, 1, 132, FONDO_ESCALA);

  // Código y descripción centrados en el alto del renglón, como en la maqueta
  for (uint8_t i = 0; i < RENGLONES_DTC; i++) {
    int32_t y = i * ALTO_RENGLON;
    codigos[i] = etiqueta(columnasFallas, CODIGO, BLANCO);
    lv_obj_set_pos(codigos[i], X_CODIGOS, yDeBase(CODIGO, y + 28));
    // Si aun abreviado no entra, se corta en el borde. Sin puntos: LVGL los
    // escribe en el texto y ponerTexto() lo volvería a poner en cada dibujo.
    descripciones[i] = etiqueta(columnasFallas, NORMAL, BLANCO);
    lv_obj_set_pos(descripciones[i], X_DESCRIPCION, yDeBase(NORMAL, y + 25));
    lv_obj_set_size(descripciones[i], ANCHO_DESCRIPCION, NORMAL->line_height);
    lv_label_set_long_mode(descripciones[i], LV_LABEL_LONG_MODE_CLIP);
  }
  // En lugar de los códigos, si no hay ninguno
  mensaje = etiqueta(columnasFallas, NORMAL, ETIQUETA);
  lv_obj_set_pos(mensaje, X_CODIGOS, yDeBase(NORMAL, 79));
}

// "Catalizador", "Falla encend. (pendiente)" o, sin descripción, "(pendiente)".
// Si no entra, con el tipo abreviado: "Sonda lambda 2 (perm.)".
static void textoDescripcion(const RenglonDtc &renglon, char *salida, size_t tam) {
  static const char *const SUFIJOS[][2] = {
      {"", ""}, {"(permanente)", "(perm.)"}, {"(pendiente)", "(pend.)"}};
  const char *descripcion = descripcionDtc(renglon.codigo);
  for (const char *sufijo : SUFIJOS[(uint8_t)renglon.tipo]) {
    if (descripcion == nullptr) {
      strlcpy(salida, sufijo, tam);
    } else {
      snprintf(salida, tam, "%s%s%s", descripcion, sufijo[0] != '\0' ? " " : "", sufijo);
    }
    if (lv_text_get_width(salida, strlen(salida), NORMAL, 0) <= ANCHO_DESCRIPCION) {
      return;
    }
  }
}

static void dibujarColumnas(const PantallaFallas &p, const LecturaFallas &lectura,
                            uint32_t reintento) {
  ponerTexto(luz, textoLuz(lectura));
  ponerColor(luz, !lectura.luzLeida ? GRIS : (lectura.luz ? AMBAR : BLANCO));

  // Guardados y pendientes siempre, aunque sean 0 o hayan fallado; los
  // permanentes solo si 0A trajo alguno (el DS3 no lo tiene)
  const Codigos *tipos[] = {&lectura.guardados, &lectura.permanentes, &lectura.pendientes};
  bool conPermanentes =
      lectura.permanentes.estado == EstadoCodigos::LEIDOS && lectura.permanentes.lista.cantidad > 0;
  uint8_t renglon = 0;
  for (uint8_t i = 0; i < 3; i++) {
    bool visible = (TipoDtc)i != TipoDtc::PERMANENTE || conPermanentes;
    mostrar(cantidades[i], visible);
    if (!visible) {
      continue;
    }
    char texto[24];
    bool leida = textoCantidad(*tipos[i], (TipoDtc)i, texto, sizeof(texto));
    ponerTexto(cantidades[i], texto);
    ponerColor(cantidades[i], leida ? ETIQUETA : AMBAR);
    int32_t base = conPermanentes ? BASES_TRES[renglon] : BASES_DOS[renglon];
    ponerPosicion(cantidades[i], MARGEN, yDeBase(NORMAL, base));
    renglon++;
  }

  for (uint8_t i = 0; i < RENGLONES_DTC; i++) {
    bool visible = i < p.cantidad;
    mostrar(codigos[i], visible);
    mostrar(descripciones[i], visible);
    if (!visible) {
      continue;
    }
    // Los pendientes, en gris como las etiquetas
    lv_color_t color = p.renglones[i].tipo == TipoDtc::PENDIENTE ? ETIQUETA : BLANCO;
    char texto[32];
    textoDtc(p.renglones[i].codigo, texto);
    ponerTexto(codigos[i], texto);
    ponerColor(codigos[i], color);
    textoDescripcion(p.renglones[i], texto, sizeof(texto));
    ponerTexto(descripciones[i], texto);
    ponerColor(descripciones[i], color);
  }

  // Sin códigos que mostrar: con la luz encendida (no es "Sin fallas"), o
  // porque algo falló y se reintenta
  mostrar(mensaje, p.cantidad == 0);
  if (p.cantidad == 0) {
    char texto[32];
    if (!errorAlLeer(lectura)) {
      ponerTexto(mensaje, "Ningun codigo guardado ni pendiente");
    } else if (reintento > 0) {
      snprintf(texto, sizeof(texto), "Reintento en %lu s", (unsigned long)reintento);
      ponerTexto(mensaje, texto);
    } else {
      ponerTexto(mensaje, "Leyendo...");
    }
  }
}

// Devuelve el título: con varios grupos de códigos, "FALLAS 1/2"
static const char *dibujarFallas(uint32_t ahora, const LecturaFallas &lectura, uint32_t reintento) {
  PantallaFallas p = pantallaFallas(lectura, ahora / PERIODO_GRUPO_FALLAS_MS);
  bool alCentro = p.vista != VistaFallas::CODIGOS;
  mostrar(centroFallas, alCentro);
  mostrar(columnasFallas, !alCentro);
  if (p.vista == VistaFallas::LEYENDO) {
    ponerTexto(grande, "Leyendo...");
    ponerColor(grande, GRIS);
    ponerTexto(debajo, "");
  } else if (p.vista == VistaFallas::SIN_FALLAS) {
    ponerTexto(grande, "Sin fallas");
    ponerColor(grande, VERDE);
    ponerTexto(debajo, lectura.luzLeida
                           ? "Luz de falla apagada, ningun codigo guardado ni pendiente"
                           : "Luz de falla sin leer, ningun codigo guardado ni pendiente");
  } else {
    dibujarColumnas(p, lectura, reintento);
  }

  if (p.grupos < 2) {
    return PAGINAS[PAGINA_FALLAS].titulo;
  }
  snprintf(tituloFallas, sizeof(tituloFallas), "%s %u/%u", PAGINAS[PAGINA_FALLAS].titulo,
           (unsigned)(p.grupo + 1), (unsigned)p.grupos);
  return tituloFallas;
}

// Info: lo que ya se sabe de la conexión. RSSI y las acciones son de E6.

static const char *const NOMBRES_INFO[] = {"Adaptador", "ELM", "Protocolo", "Lectura"};
static const uint8_t RENGLONES_INFO = sizeof(NOMBRES_INFO) / sizeof(NOMBRES_INFO[0]);
static lv_obj_t *datosInfo[RENGLONES_INFO];

static void armarInfo(lv_obj_t *v) {
  // Renglones de 36 px
  for (uint8_t i = 0; i < RENGLONES_INFO; i++) {
    int32_t base = 25 + 36 * i;
    fijo(v, NOMBRES_INFO[i], NORMAL, ETIQUETA, MARGEN, base);
    datosInfo[i] = etiqueta(v, NORMAL, BLANCO);
    lv_obj_set_pos(datosInfo[i], 132, yDeBase(NORMAL, base));
  }
}

static void dibujarInfo(const InfoConexion &info) {
  ponerTexto(datosInfo[0], BLE_NOMBRE_REMOTO);
  ponerTexto(datosInfo[1], info.version);

  char texto[40];
  const char *nombre = nombreProtocolo(info.protocolo);
  if (nombre != nullptr) {
    snprintf(texto, sizeof(texto), "%s, %s", info.protocolo, nombre);
  } else {
    strlcpy(texto, info.protocolo, sizeof(texto));
  }
  ponerTexto(datosInfo[2], texto);

  if (info.consultasPorSegundo >= 0) {
    snprintf(texto, sizeof(texto), "%u PIDs, %d consultas/s", (unsigned)info.pids,
             info.consultasPorSegundo);
  } else {
    snprintf(texto, sizeof(texto), "%u PIDs", (unsigned)info.pids);
  }
  ponerTexto(datosInfo[3], texto);
}

// Cuenta toda la pantalla, también la barra: los objetos de las páginas y de
// la barra no reciben toques, así que el evento llega a la raíz.
static void alTocar(lv_event_t *) {
  if (lv_obj_has_flag(area, LV_OBJ_FLAG_HIDDEN)) {
    return;
  }
  lv_point_t punto;
  lv_indev_get_point(lv_indev_active(), &punto);
  toque = punto.x < ANCHO / 2 ? -1 : 1;
}

void Paginas::iniciar() {
  area = pantalla.areaPaginas();
  if (area == nullptr) {
    return;
  }
  for (uint8_t i = 0; i < CANT_PAGINAS; i++) {
    vistas[i] = caja(area, 0, 0, ANCHO, ALTO);
    lv_obj_add_flag(vistas[i], LV_OBJ_FLAG_HIDDEN);
  }
  armarMotor(vistas[PAGINA_MOTOR]);
  armarAire(vistas[PAGINA_AIRE]);
  armarCarga(vistas[PAGINA_CARGA]);
  armarMezcla(vistas[PAGINA_MEZCLA]);
  armarFallas(vistas[PAGINA_FALLAS]);
  armarInfo(vistas[PAGINA_INFO]);
  lv_obj_add_event_cb(lv_screen_active(), alTocar, LV_EVENT_CLICKED, nullptr);
  armadas = true;
}

void Paginas::cambiar(int8_t paso) {
  actual = (actual + CANT_PAGINAS + paso) % CANT_PAGINAS;
}

const char *Paginas::titulo() const {
  return PAGINAS[actual].titulo;
}

static uint8_t copiar(const Definicion &def, Magnitud lista[MAX_VISIBLES]) {
  for (uint8_t i = 0; i < def.cantidad; i++) {
    lista[i] = def.magnitudes[i];
  }
  return def.cantidad;
}

uint8_t Paginas::visibles(Magnitud lista[MAX_VISIBLES]) const {
  return copiar(PAGINAS[actual], lista);
}

uint8_t Paginas::primeras(Magnitud lista[MAX_VISIBLES]) const {
  return copiar(PAGINAS[PAGINA_MOTOR], lista);
}

bool Paginas::muestraFallas() const {
  return actual == PAGINA_FALLAS;
}

int8_t Paginas::tomarToque() {
  int8_t paso = toque;
  toque = 0;
  return paso;
}

void Paginas::dibujar(uint32_t ahora, const InfoConexion &info, const LecturaFallas &fallas,
                      uint32_t reintento) {
  if (!armadas) {
    return;
  }
  for (uint8_t i = 0; i < CANT_PAGINAS; i++) {
    mostrar(vistas[i], i == actual);
  }
  const char *titulo = PAGINAS[actual].titulo;
  switch (actual) {
  case PAGINA_MOTOR:
    dibujarMotor(ahora);
    break;
  case PAGINA_AIRE:
    dibujarAire(ahora);
    break;
  case PAGINA_CARGA:
    dibujarCarga(ahora);
    break;
  case PAGINA_MEZCLA:
    dibujarMezcla(ahora);
    break;
  case PAGINA_FALLAS:
    titulo = dibujarFallas(ahora, fallas, reintento);
    break;
  case PAGINA_INFO:
    dibujarInfo(info);
    break;
  }
  pantalla.pagina(titulo, actual, CANT_PAGINAS);
}
