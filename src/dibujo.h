#pragma once

#include <stdint.h>

#include <lvgl.h>

// Lo de LVGL que comparten pantalla y paginas: colores y funciones chicas.

// Sobre fondo negro, para la noche (la prueba de E1 y la maqueta de E4)
static const lv_color_t BLANCO = lv_color_hex(0xFFFFFF);
static const lv_color_t GRIS = lv_color_hex(0x707070);     // datos que no son válidos y actuales
static const lv_color_t ETIQUETA = lv_color_hex(0x8C8C8C); // etiquetas, unidades y escalas
static const lv_color_t VERDE = lv_color_hex(0x3CC864);
static const lv_color_t AMBAR = lv_color_hex(0xFFB000);
static const lv_color_t ROJO = lv_color_hex(0xFF3B30);
static const lv_color_t CIAN = lv_color_hex(0x40C8E0);
// El aviso de cambio (E25): toda la pantalla, distinto del rojo de las alarmas
static const lv_color_t AZUL = lv_color_hex(0x2A7FFF);
static const lv_color_t FONDO_BARRA = lv_color_hex(0x1A1A1A);  // barra de estado
static const lv_color_t FONDO_ESCALA = lv_color_hex(0x262626); // barras de valores y separadores
static const lv_color_t PUNTO_APAGADO = lv_color_hex(0x444444);
// Lo que se borra manteniendo apretado: el relleno que crece y su texto
static const lv_color_t ROJO_OSCURO = lv_color_hex(0x5A1A17);
static const lv_color_t ROJO_CLARO = lv_color_hex(0xFF6B60);

// Cambiar un objeto hace que LVGL redibuje y mande la imagen entera
// (~33 ms): estas lo tocan solo si cambia.
void ponerTexto(lv_obj_t *e, const char *texto);
void ponerColor(lv_obj_t *e, lv_color_t color);
void ponerFuente(lv_obj_t *e, const lv_font_t *fuente);
void ponerFondo(lv_obj_t *obj, lv_color_t color);
void ponerPosicion(lv_obj_t *obj, int32_t x, int32_t y);
void ponerAncho(lv_obj_t *obj, int32_t ancho);
void mostrar(lv_obj_t *obj, bool visible);

// Un rectángulo que no recibe toques ni se desplaza. Sin color es
// transparente (un contenedor).
lv_obj_t *caja(lv_obj_t *padre, int32_t x, int32_t y, int32_t ancho, int32_t alto);
lv_obj_t *rectangulo(lv_obj_t *padre, int32_t x, int32_t y, int32_t ancho, int32_t alto,
                     lv_color_t color, int32_t radio = 0);
lv_obj_t *etiqueta(lv_obj_t *padre, const lv_font_t *fuente, lv_color_t color);

// La y de una etiqueta para que la base de las letras quede en base
int32_t yDeBase(const lv_font_t *fuente, int32_t base);
