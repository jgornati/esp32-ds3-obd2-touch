#pragma once

#include <stdint.h>

#include "imagenes/imagenes.h"

// Lo que ocupa en salida el recorte de la llanta girada: los colores en
// RGB565 y después el alfa, como LV_COLOR_FORMAT_RGB565A8
uint32_t bytesLlanta(const LlantaFuente &llanta);

// Gira la llanta en sentido horario como en las animaciones: la elipse se
// estira hasta un círculo, se gira y se vuelve a achicar (LVGL gira y
// escala en el otro orden). Adentro de la elipse queda opaca, con el borde
// suavizado; afuera, transparente, para que se vea el auto.
void girarLlanta(const LlantaFuente &llanta, float grados, uint8_t *salida);
