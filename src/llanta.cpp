#include "llanta.h"

#include <math.h>
#include <string.h>

uint32_t bytesLlanta(const LlantaFuente &llanta) {
  return (uint32_t)llanta.ancho * llanta.alto * 3;
}

// El color de la llanta quieta en (x, y), entre los cuatro píxeles más
// cercanos. x, y desde el borde, como cx y cy.
static uint16_t muestra(const LlantaFuente &l, float x, float y) {
  x -= 0.5f;
  y -= 0.5f;
  int x0 = (int)floorf(x);
  int y0 = (int)floorf(y);
  float fx = x - x0;
  float fy = y - y0;
  float r = 0, g = 0, b = 0;
  for (int dy = 0; dy <= 1; dy++) {
    for (int dx = 0; dx <= 1; dx++) {
      int px = x0 + dx < 0 ? 0 : (x0 + dx >= l.ancho ? l.ancho - 1 : x0 + dx);
      int py = y0 + dy < 0 ? 0 : (y0 + dy >= l.alto ? l.alto - 1 : y0 + dy);
      float peso = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
      uint16_t c = l.pixeles[py * l.ancho + px];
      r += (c >> 11) * peso;
      g += ((c >> 5) & 0x3F) * peso;
      b += (c & 0x1F) * peso;
    }
  }
  return (uint16_t)((int)(r + 0.5f) << 11 | (int)(g + 0.5f) << 5 | (int)(b + 0.5f));
}

void girarLlanta(const LlantaFuente &l, float grados, uint8_t *salida) {
  uint16_t *colores = (uint16_t *)salida;
  uint8_t *alfa = salida + l.ancho * l.alto * 2;
  float angulo = grados * (float)M_PI / 180;
  float coseno = cosf(angulo);
  float seno = sinf(angulo);
  float estirado = l.ry / l.rx;
  // El borde se suaviza en un píxel
  float radio = (l.rx + l.ry) / 2;

  for (int y = 0; y < l.alto; y++) {
    for (int x = 0; x < l.ancho; x++) {
      int i = y * l.ancho + x;
      float dx = x + 0.5f - l.cx;
      float dy = y + 0.5f - l.cy;
      float distancia = sqrtf((dx / l.rx) * (dx / l.rx) + (dy / l.ry) * (dy / l.ry));
      float cubre = (1 - distancia) * radio + 0.5f;
      if (cubre <= 0) {
        colores[i] = 0;
        alfa[i] = 0;
        continue;
      }
      // Hacia atrás: de dónde viene este píxel en la llanta quieta. En la
      // pantalla la y va hacia abajo: así el giro es horario.
      float u = dx * estirado;
      float origenU = coseno * u + seno * dy;
      float origenV = -seno * u + coseno * dy;
      colores[i] = muestra(l, l.cx + origenU / estirado, l.cy + origenV);
      alfa[i] = cubre >= 1 ? 255 : (uint8_t)(cubre * 255 + 0.5f);
    }
  }
}
