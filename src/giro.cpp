#include "giro.h"

// Dos píxeles en una palabra, sin romper las reglas de alias de C++
typedef uint32_t __attribute__((may_alias)) Palabra;

// Da vuelta los bytes de cada píxel de la palabra
static inline uint32_t paraElPanel(uint32_t dos) {
  return ((dos & 0x00FF00FF) << 8) | ((dos >> 8) & 0x00FF00FF);
}

void girarTanda(const uint16_t *imagen, int ancho, int alto, int fila, int filas, bool girada,
                uint16_t *destino) {
  // Una fila del panel son alto / 2 palabras
  const int fila2 = alto / 2;
  for (int y = 0; y < alto; y += 2) {
    const uint16_t *arriba = imagen + y * ancho;
    const uint16_t *abajo = arriba + ancho;
    if (!girada) {
      // Las columnas alto - 1 - y y alto - 2 - y del panel: va primero la de
      // y + 1. En cada palabra de la imagen, x y x + 1: dos filas del panel.
      const Palabra *a = (const Palabra *)(arriba + fila);
      const Palabra *b = (const Palabra *)(abajo + fila);
      Palabra *salida = (Palabra *)(destino + alto - 2 - y);
      for (int i = 0; i < filas; i += 2) {
        uint32_t pa = *a++;
        uint32_t pb = *b++;
        salida[0] = paraElPanel((pb & 0xFFFF) | (pa << 16));
        salida[fila2] = paraElPanel((pb >> 16) | (pa & 0xFFFF0000));
        salida += alto;
      }
    } else {
      // Girada: la fila i del panel es la columna ancho - 1 - (fila + i), y
      // las columnas del panel, y e y + 1. La palabra trae x - 1 y x.
      const Palabra *a = (const Palabra *)(arriba + ancho - 2 - fila);
      const Palabra *b = (const Palabra *)(abajo + ancho - 2 - fila);
      Palabra *salida = (Palabra *)(destino + y);
      for (int i = 0; i < filas; i += 2) {
        uint32_t pa = *a--;
        uint32_t pb = *b--;
        salida[0] = paraElPanel((pa >> 16) | (pb & 0xFFFF0000));
        salida[fila2] = paraElPanel((pa & 0xFFFF) | (pb << 16));
        salida += alto;
      }
    }
  }
}
