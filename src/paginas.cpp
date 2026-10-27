#include "paginas.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "animaciones.h"
#include "config.h"
#include "dibujo.h"
#include "imagenes/imagenes.h"

enum : uint8_t {
  PAGINA_MOTOR,
  PAGINA_AIRE,
  PAGINA_CARGA,
  PAGINA_MEZCLA,
  PAGINA_FALLAS,
  PAGINA_INFO,
  PAGINA_VIAJE,
  PAGINA_FUERZA_G,
  PAGINA_DS3,
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
    {"VIAJE", nullptr, 0},
    {"FUERZA G", nullptr, 0},
    {"DS3", nullptr, 0},
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

// Fondos: las mitades rica y pobre de lambda y Reconectar apretado. La zona
// roja de las RPM y Olvidar van en ROJO_OSCURO, de dibujo.h.
static const lv_color_t AMBAR_OSCURO = lv_color_hex(0x3D2A00);
static const lv_color_t CIAN_OSCURO = lv_color_hex(0x0E3238);

// Medidas de la maqueta de E4, en el área de 640x144 debajo de la barra.
// Las y de los textos son de la base de las letras: así quedan alineados
// aunque cambie la fuente.
static const int32_t ANCHO = 640;
static const int32_t ALTO = 144;
static const int32_t ALTO_COMPLETA = 172; // DS3, sin la barra
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
static lv_obj_t *areaCompleta = nullptr;
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

// Un valor actual va en blanco, o en rojo si tiene una alarma
static void ponerValor(Valor &v, const char *texto, bool actual, bool enAlarma = false) {
  const lv_font_t *fuente = v.fuente;
  if (fuente == &fuente_numeros_110 && !entraEnNumeros(texto)) {
    fuente = &lv_font_montserrat_48;
  }
  ponerFuente(v.numero, fuente);
  ponerTexto(v.numero, texto);
  ponerColor(v.numero, !actual ? GRIS : (enAlarma ? ROJO : BLANCO));

  int32_t ancho = lv_text_get_width(texto, strlen(texto), fuente, 0);
  bool conUnidad = v.unidad != nullptr && actual;
  int32_t x = v.aLaDerecha ? v.x - ancho - (conUnidad ? v.anchoUnidad : 0) : v.x;
  ponerPosicion(v.numero, x, yDeBase(fuente, v.base));
  if (v.unidad != nullptr) {
    mostrar(v.unidad, conUnidad);
    ponerPosicion(v.unidad, x + ancho, yDeBase(NORMAL, v.base));
  }
}

// Para una unidad que cambia (el tiempo del viaje: min o h). Antes de
// ponerValor(), que la acomoda.
static void ponerUnidad(Valor &v, const char *unidad) {
  ponerTexto(v.unidad, unidad);
  v.anchoUnidad = lv_text_get_width(unidad, strlen(unidad), NORMAL, 0);
}

// Las magnitudes con una alarma, un bit por cada una: van en rojo
static uint16_t enAlarma = 0;

// El texto de la magnitud según las convenciones de pantalla. true si es un
// valor actual, y entonces queda en valor.
static bool ponerMagnitud(Valor &v, Magnitud m, uint32_t ahora, float *valor = nullptr) {
  char texto[16];
  bool actual = textoValor(m, ahora, texto, sizeof(texto));
  ponerValor(v, texto, actual, (enAlarma >> m) & 1);
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
             ALTO_BARRA_ANCHA, ROJO_OSCURO);
  rectangulo(v, BORDE_DERECHO - ALTO_BARRA_ANCHA, Y_BARRA_ANCHA, ALTO_BARRA_ANCHA, ALTO_BARRA_ANCHA,
             ROJO_OSCURO, ALTO_BARRA_ANCHA / 2);
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
  rectangulo(v, X_DERECHA, 42, 14, 14, AMBAR_OSCURO, 7);
  rectangulo(v, X_DERECHA + 7, 42, medio - X_DERECHA - 7, 14, AMBAR_OSCURO);
  rectangulo(v, medio, 42, BORDE_DERECHO - 7 - medio, 14, CIAN_OSCURO);
  rectangulo(v, BORDE_DERECHO - 14, 42, 14, 14, CIAN_OSCURO, 7);
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

// Info: a la izquierda lo que se sabe del adaptador, la tarjeta y la ECU y,
// abajo, el brillo con - y +; a la derecha Reconectar y Olvidar adaptador,
// como en la maqueta. Los botones se quedan con su toque: no llega a la raíz
// y no cambia la página. La tarjeta (E13) va donde estaba la dirección del
// adaptador, que queda en !info.

static const char *const NOMBRES_INFO[] = {"Adaptador", "Tarjeta", "ELM", "Lectura"};
static const uint8_t RENGLONES_INFO = sizeof(NOMBRES_INFO) / sizeof(NOMBRES_INFO[0]);
// Renglones de 24 px (en la maqueta, 36): abajo va el brillo
static const int32_t ALTO_RENGLON_INFO = 24;
// El brillo: - y + a los costados y un segmento por nivel en el medio
static const int32_t Y_BRILLO = 102;
static const int32_t ALTO_BOTON_BRILLO = 38;
static const int32_t ANCHO_BOTON_BRILLO = 56;
static const int32_t ANCHO_SEGMENTO = 24;
static const int32_t ENTRE_SEGMENTOS = 6;
static const int32_t ALTO_SEGMENTO = 10;
static const int32_t X_VALOR_INFO = 132;
static const int32_t X_BOTONES = 424;
static const int32_t ANCHO_BOTONES = BORDE_DERECHO - X_BOTONES;
static const int32_t ALTO_BOTONES = 56;
static const int32_t Y_RECONECTAR = 10;
static const int32_t Y_OLVIDAR = Y_RECONECTAR + ALTO_BOTONES + 12;
static const int32_t BORDE_BOTON = 2;
static const int32_t RADIO_BOTON = 8;

static lv_obj_t *datosInfo[RENGLONES_INFO];
static lv_obj_t *segmentosBrillo[CANT_NIVELES_BRILLO];
static lv_obj_t *rellenoOlvidar = nullptr;
static Accion accion = Accion::NINGUNA;
static bool olvidando = false;    // Olvidar apretado
static uint32_t olvidoDesde = 0;  // desde cuándo
static bool olvidoPedido = false; // ya se pidió en esta apretada

static void alSoltarBoton(lv_event_t *e) {
  accion = (Accion)(uintptr_t)lv_event_get_user_data(e);
}

// Olvidar no cuenta al soltarlo sino después de MANTENER_OLVIDAR_MS
// apretado: lo mide tomarAccion(). Si el dedo se va del botón, se cancela.
static void alApretarOlvidar(lv_event_t *e) {
  lv_event_code_t codigo = lv_event_get_code(e);
  if (codigo == LV_EVENT_PRESSED) {
    olvidando = true;
    olvidoDesde = lv_tick_get();
    olvidoPedido = false;
  } else if (codigo == LV_EVENT_RELEASED || codigo == LV_EVENT_PRESS_LOST) {
    olvidando = false;
  }
}

// Borde de color y, mientras se lo aprieta, el fondo en apretado. Sin
// PRESS_LOCK: si el dedo se va del botón, deja de estar apretado y al soltar
// no cuenta. alSoltar, si no es NINGUNA, queda como pedido al soltarlo.
static lv_obj_t *boton(lv_obj_t *padre, int32_t x, int32_t y, int32_t ancho, int32_t alto,
                       lv_color_t borde, lv_color_t apretado, Accion alSoltar) {
  lv_obj_t *b = caja(padre, x, y, ancho, alto);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_set_style_radius(b, RADIO_BOTON, 0);
  lv_obj_set_style_border_width(b, BORDE_BOTON, 0);
  lv_obj_set_style_border_color(b, borde, 0);
  lv_obj_set_style_bg_color(b, apretado, LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_STATE_PRESSED);
  if (alSoltar != Accion::NINGUNA) {
    lv_obj_add_event_cb(b, alSoltarBoton, LV_EVENT_CLICKED, (void *)(uintptr_t)alSoltar);
  }
  return b;
}

// Un texto centrado en el botón, corrido dy hacia abajo
static void textoBoton(lv_obj_t *b, const char *texto, const lv_font_t *fuente, lv_color_t color,
                       int32_t dy = 0) {
  lv_obj_t *e = etiqueta(b, fuente, color);
  lv_label_set_text(e, texto);
  lv_obj_align(e, LV_ALIGN_CENTER, 0, dy);
}

static void armarBrillo(lv_obj_t *v) {
  int32_t medio = Y_BRILLO + ALTO_BOTON_BRILLO / 2;
  fijo(v, "Brillo", NORMAL, ETIQUETA, MARGEN, medio + 7);
  lv_obj_t *menos = boton(v, X_VALOR_INFO, Y_BRILLO, ANCHO_BOTON_BRILLO, ALTO_BOTON_BRILLO,
                          ETIQUETA, FONDO_ESCALA, Accion::BAJAR_BRILLO);
  textoBoton(menos, LV_SYMBOL_MINUS, NORMAL, BLANCO);
  int32_t ancho = CANT_NIVELES_BRILLO * (ANCHO_SEGMENTO + ENTRE_SEGMENTOS) - ENTRE_SEGMENTOS;
  int32_t x = X_BOTONES - MARGEN - ANCHO_BOTON_BRILLO;
  int32_t inicio = (X_VALOR_INFO + ANCHO_BOTON_BRILLO + x - ancho) / 2;
  for (uint8_t i = 0; i < CANT_NIVELES_BRILLO; i++) {
    segmentosBrillo[i] =
        rectangulo(v, inicio + i * (ANCHO_SEGMENTO + ENTRE_SEGMENTOS), medio - ALTO_SEGMENTO / 2,
                   ANCHO_SEGMENTO, ALTO_SEGMENTO, FONDO_ESCALA, 3);
  }
  lv_obj_t *mas = boton(v, x, Y_BRILLO, ANCHO_BOTON_BRILLO, ALTO_BOTON_BRILLO, ETIQUETA,
                        FONDO_ESCALA, Accion::SUBIR_BRILLO);
  textoBoton(mas, LV_SYMBOL_PLUS, NORMAL, BLANCO);
}

static void armarInfo(lv_obj_t *v) {
  for (uint8_t i = 0; i < RENGLONES_INFO; i++) {
    int32_t base = 22 + ALTO_RENGLON_INFO * i;
    fijo(v, NOMBRES_INFO[i], NORMAL, ETIQUETA, MARGEN, base);
    datosInfo[i] = etiqueta(v, NORMAL, BLANCO);
    lv_obj_set_pos(datosInfo[i], X_VALOR_INFO, yDeBase(NORMAL, base));
  }

  lv_obj_t *reconectar = boton(v, X_BOTONES, Y_RECONECTAR, ANCHO_BOTONES, ALTO_BOTONES, CIAN,
                               CIAN_OSCURO, Accion::RECONECTAR);
  textoBoton(reconectar, "Reconectar", NORMAL, CIAN);

  // Apretado, el fondo sigue negro: lo que se ve es el relleno, que crece
  // hasta MANTENER_OLVIDAR_MS. Va antes que los textos, para quedar abajo.
  lv_obj_t *olvidar = boton(v, X_BOTONES, Y_OLVIDAR, ANCHO_BOTONES, ALTO_BOTONES, ROJO,
                            lv_color_black(), Accion::NINGUNA);
  static const lv_event_code_t EVENTOS[] = {LV_EVENT_PRESSED, LV_EVENT_RELEASED,
                                            LV_EVENT_PRESS_LOST};
  for (lv_event_code_t codigo : EVENTOS) {
    lv_obj_add_event_cb(olvidar, alApretarOlvidar, codigo, nullptr);
  }
  rellenoOlvidar = rectangulo(olvidar, 0, 0, 0, ALTO_BOTONES - 2 * BORDE_BOTON, ROJO_OSCURO,
                              RADIO_BOTON - BORDE_BOTON);
  lv_obj_add_flag(rellenoOlvidar, LV_OBJ_FLAG_HIDDEN);
  textoBoton(olvidar, "Olvidar adaptador", NORMAL, ROJO_CLARO, -8);
  char texto[24];
  snprintf(texto, sizeof(texto), "mantener %lu s", (unsigned long)(MANTENER_OLVIDAR_MS / 1000));
  textoBoton(olvidar, texto, CHICA, ROJO_CLARO, 12);
  armarBrillo(v);
}

static void dibujarInfo(uint32_t ahora, const DatosInfo &info) {
  char texto[40];
  if (info.rssi < 0) {
    snprintf(texto, sizeof(texto), "%s, %d dBm", BLE_NOMBRE_REMOTO, info.rssi);
  } else {
    strlcpy(texto, BLE_NOMBRE_REMOTO, sizeof(texto));
  }
  ponerTexto(datosInfo[0], texto);
  // Gris sin tarjeta, como un dato que no hay; ámbar si está pero no graba
  textoTarjeta(info.tarjeta, info.viaje, texto, sizeof(texto));
  ponerTexto(datosInfo[1], texto);
  bool sinTarjeta =
      info.tarjeta == EstadoTarjeta::BUSCANDO || info.tarjeta == EstadoTarjeta::SIN_TARJETA;
  ponerColor(datosInfo[1], sinTarjeta ? GRIS : problemaTarjeta(info.tarjeta) ? AMBAR : BLANCO);
  snprintf(texto, sizeof(texto), "%s, protocolo %s", info.version, info.protocolo);
  ponerTexto(datosInfo[2], texto);
  if (info.consultasPorSegundo >= 0) {
    snprintf(texto, sizeof(texto), "%u PIDs, %d consultas/s", (unsigned)info.pids,
             info.consultasPorSegundo);
  } else {
    snprintf(texto, sizeof(texto), "%u PIDs", (unsigned)info.pids);
  }
  ponerTexto(datosInfo[3], texto);
  for (uint8_t i = 0; i < CANT_NIVELES_BRILLO; i++) {
    ponerFondo(segmentosBrillo[i], i < info.brillo ? BLANCO : FONDO_ESCALA);
  }

  int32_t lleno = 0;
  if (olvidando) {
    float apretado = (int32_t)(ahora - olvidoDesde);
    lleno = lroundf((ANCHO_BOTONES - 2 * BORDE_BOTON) * fraccion(apretado, 0, MANTENER_OLVIDAR_MS));
  }
  mostrar(rellenoOlvidar, lleno > 0);
  if (lleno > 0) {
    ponerAncho(rellenoOlvidar, lleno);
  }
}

// Viaje: ocho casillas de 160x72, como en la maqueta B de E10. Mantener
// apretada la página MANTENER_VIAJE_MS reinicia el viaje: mientras tanto la
// barra de arriba se llena de rojo.

static const int32_t ANCHO_CASILLA = ANCHO / 4;
static const int32_t ALTO_CASILLA = ALTO / 2;
static const int32_t RELLENO_CASILLA = 12;

static Valor distanciaViaje, tiempoViaje, promedioViaje, velocidadMaxima;
static Valor rpmMaxima, refrigeranteMaximo, voltajeMinimo, voltajeMaximo;

// La pantalla apretada, en cualquier página: en Viaje, mantenerla reinicia
static bool mantenida = false;
static uint32_t mantenidaDesde = 0;
static bool reinicioPedido = false; // ya se pidió en esta apretada
static bool toqueLargo = false;     // el último toque duró AVISO_MANTENER_MS o más

// Casilla 0 a 7, de izquierda a derecha y de arriba abajo: nombre y valor.
// Si el nombre cambia (Fuerza G), queda en etiquetaNombre.
static Valor casilla(lv_obj_t *v, uint8_t numero, const char *nombre, const char *unidad = nullptr,
                     lv_obj_t **etiquetaNombre = nullptr) {
  int32_t x = (numero % 4) * ANCHO_CASILLA + RELLENO_CASILLA;
  int32_t y = (numero / 4) * ALTO_CASILLA;
  lv_obj_t *e = fijo(v, nombre, NORMAL, ETIQUETA, x, y + 25);
  if (etiquetaNombre != nullptr) {
    *etiquetaNombre = e;
  }
  return crearValor(v, &lv_font_montserrat_36, x, y + 63, false, unidad);
}

static void armarViaje(lv_obj_t *v) {
  for (int32_t i = 1; i < 4; i++) {
    rectangulo(v, i * ANCHO_CASILLA - 1, 0, 1, ALTO, FONDO_ESCALA);
  }
  rectangulo(v, 0, ALTO_CASILLA - 1, ANCHO, 1, FONDO_ESCALA);
  distanciaViaje = casilla(v, 0, "Distancia", " km");
  tiempoViaje = casilla(v, 1, "Tiempo", " min");
  promedioViaje = casilla(v, 2, "Promedio", " km/h");
  velocidadMaxima = casilla(v, 3, "Vel. max", " km/h");
  rpmMaxima = casilla(v, 4, "RPM max");
  refrigeranteMaximo = casilla(v, 5, "Refrig. max", " °C");
  voltajeMinimo = casilla(v, 6, "Volt. min", " V");
  voltajeMaximo = casilla(v, 7, "Volt. max", " V");
}

static bool soportada(Magnitud m) {
  return dato(m).estado != EstadoDato::NO_SOPORTADO;
}

// Un dato del viaje: "N/D" si la ECU no da lo que hace falta para sacarlo, y
// "..." si todavía no hay nada que mostrar
static void ponerDelViaje(Valor &v, bool sePuede, bool hay, const char *texto) {
  if (!sePuede) {
    ponerValor(v, "N/D", false);
  } else {
    ponerValor(v, hay ? texto : "...", hay);
  }
}

static void ponerExtremo(Valor &v, bool sePuede, const Extremo &e, uint8_t decimales) {
  char texto[16];
  snprintf(texto, sizeof(texto), "%.*f", decimales, e.valor);
  ponerDelViaje(v, sePuede, e.hay, texto);
}

// La distancia y la velocidad máxima salen de la velocidad; el tiempo en
// marcha y los voltajes, de la RPM; el promedio, de las dos
static void dibujarViaje(const Viaje &viaje) {
  bool conVelocidad = soportada(VELOCIDAD);
  bool conRpm = soportada(RPM);
  // La velocidad máxima dice si ya hubo alguna velocidad, y la RPM máxima,
  // alguna RPM
  bool velocidadLeida = viaje.velocidadMaxima().hay;
  char texto[16];
  textoDistancia(viaje.distanciaKm(), texto, sizeof(texto));
  ponerDelViaje(distanciaViaje, conVelocidad, velocidadLeida, texto);

  char unidad[8];
  snprintf(unidad, sizeof(unidad), " %s",
           textoTiempo(viaje.tiempoEnMarchaMs(), texto, sizeof(texto)));
  ponerUnidad(tiempoViaje, unidad);
  ponerDelViaje(tiempoViaje, conRpm, viaje.rpmMaxima().hay, texto);

  float kmh = 0;
  bool conPromedio = velocidadLeida && viaje.promedio(&kmh);
  snprintf(texto, sizeof(texto), "%.0f", kmh);
  ponerDelViaje(promedioViaje, conVelocidad && conRpm, conPromedio, texto);

  ponerExtremo(velocidadMaxima, conVelocidad, viaje.velocidadMaxima(), 0);
  ponerExtremo(rpmMaxima, conRpm, viaje.rpmMaxima(), 0);
  ponerExtremo(refrigeranteMaximo, soportada(REFRIGERANTE), viaje.refrigeranteMaximo(), 0);
  ponerExtremo(voltajeMinimo, conRpm, viaje.voltajeMinimo(), 1);
  ponerExtremo(voltajeMaximo, conRpm, viaje.voltajeMaximo(), 1);
}

// Lo que lleva apretada, de 0 a 1, para la barra; 0 si todavía es un toque
static float fraccionMantenida(uint32_t ahora) {
  int32_t tiempo = mantenida ? (int32_t)(ahora - mantenidaDesde) : 0;
  return tiempo >= (int32_t)AVISO_MANTENER_MS ? fraccion(tiempo, 0, MANTENER_VIAJE_MS) : 0;
}

// Los toques que llegan a la raíz. Al soltar, LVGL manda RELEASED antes que
// el CLICKED de alTocar.
static void alApretar(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
    mantenida = true;
    mantenidaDesde = lv_tick_get();
    reinicioPedido = false;
  } else {
    mantenida = false;
  }
}

// Fuerza G: el círculo en la primera columna y seis casillas como las de
// Viaje (la maqueta A de E12): la longitudinal y la lateral de ahora y los
// máximos del viaje. El punto va como una bolita, hacia donde empuja el auto
// (al revés de su aceleración): al frenar sube, y en una curva a la derecha
// va a la izquierda.

static const float FUERZA_G_MAXIMA = 1; // en el borde del círculo
static const int32_t CENTRO_G_X = ANCHO_CASILLA / 2;
static const int32_t CENTRO_G_Y = ALTO / 2;
static const int32_t RADIO_G = 64;
static const int32_t RADIO_PUNTO = 7;
static const lv_color_t ANILLO_MEDIO = lv_color_hex(0x333333);

static lv_obj_t *puntoG, *avisoG, *textoAvisoG, *nombreLongitudinalG, *nombreLateralG;
static Valor longitudinalG, lateralG, frenadaMaxima, aceleracionMaxima, izquierdaMaxima,
    derechaMaxima;

// Un anillo con el centro en el del círculo
static void anillo(lv_obj_t *v, int32_t radio, int32_t borde, lv_color_t color) {
  lv_obj_t *a = caja(v, CENTRO_G_X - radio, CENTRO_G_Y - radio, 2 * radio, 2 * radio);
  lv_obj_set_style_radius(a, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_width(a, borde, 0);
  lv_obj_set_style_border_color(a, color, 0);
}

static void armarFuerzaG(lv_obj_t *v) {
  for (int32_t i = 1; i < 4; i++) {
    rectangulo(v, i * ANCHO_CASILLA - 1, 0, 1, ALTO, FONDO_ESCALA);
  }
  rectangulo(v, ANCHO_CASILLA, ALTO_CASILLA - 1, ANCHO - ANCHO_CASILLA, 1, FONDO_ESCALA);
  // El círculo: los ejes, 0,5 g y 1 g
  rectangulo(v, CENTRO_G_X - RADIO_G, CENTRO_G_Y, 2 * RADIO_G, 1, FONDO_ESCALA);
  rectangulo(v, CENTRO_G_X, CENTRO_G_Y - RADIO_G, 1, 2 * RADIO_G, FONDO_ESCALA);
  anillo(v, RADIO_G / 2, 1, ANILLO_MEDIO);
  anillo(v, RADIO_G, 2, PUNTO_APAGADO);
  fijo(v, "1 g", CHICA, ETIQUETA, ANCHO_CASILLA - 6, ALTO - 4, Alineacion::DERECHA);
  // "Calibrando" o "Sin sensor", sobre el eje
  avisoG = rectangulo(v, CENTRO_G_X - 42, CENTRO_G_Y + 16, 84, 20, lv_color_black());
  textoAvisoG = etiqueta(avisoG, CHICA, GRIS);
  lv_obj_center(textoAvisoG);
  puntoG = rectangulo(v, CENTRO_G_X - RADIO_PUNTO, CENTRO_G_Y - RADIO_PUNTO, 2 * RADIO_PUNTO,
                      2 * RADIO_PUNTO, BLANCO, LV_RADIUS_CIRCLE);
  lv_obj_add_flag(puntoG, LV_OBJ_FLAG_HIDDEN);

  longitudinalG = casilla(v, 1, "Long.", " g", &nombreLongitudinalG);
  frenadaMaxima = casilla(v, 2, "Frenada max", " g");
  aceleracionMaxima = casilla(v, 3, "Acel. max", " g");
  lateralG = casilla(v, 5, "Lateral", " g", &nombreLateralG);
  izquierdaMaxima = casilla(v, 6, "Izq. max", " g");
  derechaMaxima = casilla(v, 7, "Der. max", " g");
}

static void ponerMaximoG(Valor &v, bool conSensor, const Extremo &e) {
  char texto[8];
  textoFuerzaG(e.valor, texto, sizeof(texto));
  ponerDelViaje(v, conSensor, e.hay, texto);
}

// Sin sensor, "N/D"; hasta calibrar, "..."; sin muestras recientes, "--" y
// sin el punto. Los máximos quedan.
static void dibujarFuerzaG(const DatosFuerzaG &datos) {
  const LecturaG &g = datos.lectura;
  bool hay = datos.conSensor && g.actual;
  const char *sinValor = !datos.conSensor ? "N/D" : (g.calibrada ? "--" : "...");
  char texto[8];
  ponerTexto(nombreLongitudinalG, hay ? nombreLongitudinal(g.longitudinal) : "Long.");
  textoFuerzaG(g.longitudinal, texto, sizeof(texto));
  ponerValor(longitudinalG, hay ? texto : sinValor, hay);
  ponerTexto(nombreLateralG, hay ? nombreLateral(g.lateral) : "Lateral");
  textoFuerzaG(g.lateral, texto, sizeof(texto));
  ponerValor(lateralG, hay ? texto : sinValor, hay);
  ponerMaximoG(frenadaMaxima, datos.conSensor, g.frenada);
  ponerMaximoG(aceleracionMaxima, datos.conSensor, g.aceleracion);
  ponerMaximoG(izquierdaMaxima, datos.conSensor, g.curvaIzquierda);
  ponerMaximoG(derechaMaxima, datos.conSensor, g.curvaDerecha);

  mostrar(avisoG, !datos.conSensor || !g.calibrada);
  ponerTexto(textoAvisoG, datos.conSensor ? "Calibrando" : "Sin sensor");
  mostrar(puntoG, hay);
  if (hay) {
    // Al revés de la aceleración; en la pantalla, la y crece para abajo. Lo
    // que pasa de FUERZA_G_MAXIMA queda en el borde.
    float x = -g.lateral / FUERZA_G_MAXIMA;
    float y = g.longitudinal / FUERZA_G_MAXIMA;
    float distancia = sqrtf(x * x + y * y);
    if (distancia > 1) {
      x /= distancia;
      y /= distancia;
    }
    ponerPosicion(puntoG, CENTRO_G_X + lroundf(x * RADIO_G) - RADIO_PUNTO,
                  CENTRO_G_Y + lroundf(y * RADIO_G) - RADIO_PUNTO);
  }
}

// DS3: el paseo, a pantalla completa (sin la barra)

static Paseo paseoDs3;

static void alAnimarPaseo(lv_timer_t *) {
  paseoDs3.mover(lv_tick_get());
}

// Cuenta toda la pantalla, también la barra: los objetos de las páginas y de
// la barra no reciben toques, así que el evento llega a la raíz.
static void alTocar(lv_event_t *) {
  if (lv_obj_has_flag(area, LV_OBJ_FLAG_HIDDEN) &&
      lv_obj_has_flag(areaCompleta, LV_OBJ_FLAG_HIDDEN)) {
    return;
  }
  lv_point_t punto;
  lv_indev_get_point(lv_indev_active(), &punto);
  toque = punto.x < ANCHO / 2 ? -1 : 1;
  toqueLargo = lv_tick_get() - mantenidaDesde >= AVISO_MANTENER_MS;
}

void Paginas::iniciar() {
  area = pantalla.areaPaginas();
  areaCompleta = pantalla.areaCompleta();
  if (area == nullptr) {
    return;
  }
  for (uint8_t i = 0; i < CANT_PAGINAS; i++) {
    vistas[i] = i == PAGINA_DS3 ? caja(areaCompleta, 0, 0, ANCHO, ALTO_COMPLETA)
                                : caja(area, 0, 0, ANCHO, ALTO);
    lv_obj_add_flag(vistas[i], LV_OBJ_FLAG_HIDDEN);
  }
  armarMotor(vistas[PAGINA_MOTOR]);
  armarAire(vistas[PAGINA_AIRE]);
  armarCarga(vistas[PAGINA_CARGA]);
  armarMezcla(vistas[PAGINA_MEZCLA]);
  armarFallas(vistas[PAGINA_FALLAS]);
  armarInfo(vistas[PAGINA_INFO]);
  armarViaje(vistas[PAGINA_VIAJE]);
  armarFuerzaG(vistas[PAGINA_FUERZA_G]);
  paseoDs3.crear(vistas[PAGINA_DS3]);
  lv_timer_create(alAnimarPaseo, paseo.ms_cuadro, nullptr);
  lv_obj_t *raiz = lv_screen_active();
  lv_obj_add_event_cb(raiz, alTocar, LV_EVENT_CLICKED, nullptr);
  static const lv_event_code_t EVENTOS[] = {LV_EVENT_PRESSED, LV_EVENT_RELEASED,
                                            LV_EVENT_PRESS_LOST};
  for (lv_event_code_t codigo : EVENTOS) {
    lv_obj_add_event_cb(raiz, alApretar, codigo, nullptr);
  }
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

bool Paginas::muestraInfo() const {
  return actual == PAGINA_INFO;
}

bool Paginas::muestraFuerzaG() const {
  return actual == PAGINA_FUERZA_G;
}

int8_t Paginas::tomarToque() {
  int8_t paso = toque;
  // En Viaje, soltarla después de mantenerla no cambia la página
  if (toqueLargo && actual == PAGINA_VIAJE) {
    paso = 0;
  }
  toque = 0;
  return paso;
}

Accion Paginas::tomarAccion(uint32_t ahora) {
  if (olvidando && !olvidoPedido && actual == PAGINA_INFO &&
      (int32_t)(ahora - olvidoDesde) >= (int32_t)MANTENER_OLVIDAR_MS) {
    olvidoPedido = true;
    accion = Accion::OLVIDAR;
  }
  if (mantenida && !reinicioPedido && actual == PAGINA_VIAJE &&
      (int32_t)(ahora - mantenidaDesde) >= (int32_t)MANTENER_VIAJE_MS) {
    reinicioPedido = true;
    accion = Accion::REINICIAR_VIAJE;
  }
  Accion pedida = accion;
  accion = Accion::NINGUNA;
  return pedida;
}

void Paginas::dibujar(uint32_t ahora, const DatosInfo &info, const LecturaFallas &fallas,
                      uint32_t reintento, const Viaje &viaje, const DatosFuerzaG &fuerzaG,
                      const AlarmaALaVista &alarma) {
  if (!armadas) {
    return;
  }
  enAlarma = alarma.magnitudes;
  for (uint8_t i = 0; i < CANT_PAGINAS; i++) {
    mostrar(vistas[i], i == actual);
  }
  const char *titulo = PAGINAS[actual].titulo;
  float apretada = 0;
  // Mientras graba y si falla; sin tarjeta, nada
  MarcaSd sd = info.tarjeta == EstadoTarjeta::GRABANDO ? MarcaSd::GRABANDO
               : problemaTarjeta(info.tarjeta)         ? MarcaSd::PROBLEMA
                                                       : MarcaSd::NINGUNA;
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
    dibujarInfo(ahora, info);
    break;
  case PAGINA_VIAJE:
    dibujarViaje(viaje);
    apretada = fraccionMantenida(ahora);
    if (apretada > 0) {
      titulo = reinicioPedido ? "Viaje reiniciado" : "Mantener para reiniciar el viaje";
    }
    break;
  case PAGINA_FUERZA_G:
    dibujarFuerzaG(fuerzaG);
    break;
  case PAGINA_DS3:
    // Se mueve sola, con alAnimarPaseo
    pantalla.paginaCompleta(actual, CANT_PAGINAS, alarma.texto, sd);
    return;
  }
  pantalla.pagina(titulo, actual, CANT_PAGINAS, apretada, alarma.texto, sd);
}
