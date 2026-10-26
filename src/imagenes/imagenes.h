#pragma once

#include <stdint.h>

#include <lvgl.h>

// Las animaciones del DS3 (E9). Los datos de entrada.c y paseo.c los genera
// herramientas/imagenes/convertir.py desde los cuadros originales.

#ifdef __cplusplus
extern "C" {
#endif

// Una llanta quieta, en RGB565, recortada con un píxel de margen alrededor de
// su elipse. x, y: dónde va el recorte (en el auto o en la pantalla). cx,
// cy: el centro de la elipse en el recorte, contando desde el borde (el
// píxel i va de i a i + 1); rx, ry: los semiejes.
typedef struct {
  const uint16_t *pixeles;
  uint16_t ancho;
  uint16_t alto;
  int16_t x;
  int16_t y;
  float cx;
  float cy;
  float rx;
  float ry;
} LlantaFuente;

// La pantalla de espera: el auto entra desde la izquierda, una vez, y se
// queda en el último cuadro
typedef struct {
  int16_t x;    // del auto
  float grados; // de las llantas, en sentido horario
} CuadroEntrada;

extern const lv_image_dsc_t entrada_auto; // mezclado con el negro, sin alfa
extern const LlantaFuente entrada_llantas[2];
extern const CuadroEntrada entrada_cuadros[];
extern const uint8_t entrada_cantidad;
extern const uint16_t entrada_ms_cuadro;
extern const int16_t entrada_y;

// La página DS3: el auto quieto con las llantas girando, y las nubes y las
// rayas del camino corriéndose a la izquierda todo el ancho en cada vuelta
typedef struct {
  uint16_t duracion_ms; // de una vuelta
  uint16_t ms_cuadro;   // el de los cuadros originales
  uint16_t grados_por_segundo;
  int16_t y_fondo; // paseo_fondo va debajo de las nubes
  int16_t techo_x; // el techo, que asoma entre las nubes
  int16_t techo_y;
  int16_t y_camino; // desde acá, el camino lo dibuja LVGL
  uint32_t color_camino;
  int16_t x_raya; // la primera, al empezar la vuelta
  int16_t y_raya;
  int16_t alto_raya;
  int16_t largo_raya;
  int16_t periodo_raya;
  uint32_t color_raya;
} DatosPaseo;

extern const lv_image_dsc_t paseo_nubes; // 640 de ancho, desde y 0
extern const lv_image_dsc_t paseo_fondo; // el cielo, el auto y el camino, quietos
extern const lv_image_dsc_t paseo_techo; // con alfa
extern const LlantaFuente paseo_llantas[2];
extern const DatosPaseo paseo;

#ifdef __cplusplus
}
#endif
