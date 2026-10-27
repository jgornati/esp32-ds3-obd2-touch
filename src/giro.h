#pragma once

#include <stdint.h>

// Pasa a vertical las filas [fila, fila + filas) del panel desde la imagen
// horizontal de LVGL (RGB565, ancho x alto): cada columna de la imagen es una
// fila del panel, y la columna del panel es alto - 1 - y. De paso da vuelta los
// bytes de cada píxel (el panel espera primero el alto). girada: a 180°.
//
// Va de a 2x2 píxeles, con palabras de 32 bits: lee dos filas de la imagen en
// orden (en la PSRAM, cada línea de la caché se trae una vez) y escribe dos
// píxeles por vez. ancho, alto, fila y filas tienen que ser pares, e imagen y
// destino, de a 4 bytes. Sin Arduino ni LVGL: el render lo compara con el giro
// de a un píxel.
void girarTanda(const uint16_t *imagen, int ancho, int alto, int fila, int filas, bool girada,
                uint16_t *destino);
