#pragma once

#include <stdint.h>

#include <lvgl.h>

// Las animaciones del DS3 (E9), con las imágenes de src/imagenes. Las
// posiciones salen de la hora (lv_tick_get()), no de contar cuadros: si
// LVGL se atrasa, se saltea cuadros pero no se frena.

// Una llanta que gira: la imagen de LVGL y el búfer donde se la gira
struct LlantaGirando {
  lv_obj_t *imagen = nullptr;
  lv_image_dsc_t dsc = {};
  uint8_t *bufer = nullptr;
  float grados = -1; // los de la última vez; -1 sin girar
};

// La pantalla de espera: el auto entra desde la izquierda una sola vez, a
// partir de un momento, y se queda en el último cuadro.
class Entrada {
public:
  // Oculto hasta aparece (la hora de lv_tick_get()). y: la del auto (en los
  // cuadros originales, entrada_y).
  void crear(lv_obj_t *padre, uint32_t aparece, int32_t y);
  // Desde el temporizador de quien la usa. visible: si la pantalla de espera
  // está a la vista. Si a la hora de aparecer no lo está, no entra: queda
  // directamente quieto.
  void mover(uint32_t ahora, bool visible);
  // Ya entró (o no le tocaba entrar) y está quieto
  bool quieto() const {
    return etapa == Etapa::QUIETO;
  }

private:
  enum class Etapa : uint8_t { OCULTO, ENTRANDO, QUIETO };
  void ponerCuadro(uint8_t cuadro);

  lv_obj_t *contenedor = nullptr; // el auto y sus llantas
  LlantaGirando llantas[2];
  Etapa etapa = Etapa::OCULTO;
  int32_t y = 0;
  uint32_t aparece = 0;
  uint32_t desde = 0; // cuándo empezó a entrar
  int16_t cuadroPuesto = -1;
};

// La página DS3: el auto andando, con las nubes y el camino corriéndose.
class Paseo {
public:
  void crear(lv_obj_t *padre);
  // Desde el temporizador de quien lo usa. Oculto, no hace nada.
  void mover(uint32_t ahora);

private:
  lv_obj_t *contenedor = nullptr;
  lv_obj_t *nubes[2] = {nullptr, nullptr};
  lv_obj_t *rayas = nullptr;
  LlantaGirando llantas[2];
};
