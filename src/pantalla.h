#pragma once

#include <stdint.h>

#include <lvgl.h>

#include "pantalla_hw.h"

// Una línea: etiqueta a la izquierda y valor a la derecha. Cualquiera de
// los dos puede ser "".
struct LineaDato {
  const char *etiqueta;
  const char *valor;
  // false: el valor no es válido y actual ("--", "...", "N/D") y va en gris
  bool actual;
};

// Puntos de página que caben en la barra
constexpr uint8_t MAX_PAGINAS = 8;

// Pantallas de estado (E3) y el marco de las páginas (E4), con LVGL. Barra
// de estado arriba y debajo hasta 4 líneas, o el área de las páginas. Cada
// función arma una pantalla completa; LVGL la dibuja en el próximo
// PantallaHw::atender(), y solo si algo cambió. Los valores llegan ya como
// texto: quien llama decide si están vigentes.
class Pantalla {
public:
  explicit Pantalla(PantallaHw &hw) : hw(hw) {}

  // Crea los objetos. Si la pantalla no se pudo iniciar, las demás no hacen nada.
  void iniciar();

  void inicio();
  void alimentacionBaja();
  // Se dibuja en el acto: conectar bloquea sin atender la pantalla
  void conectando(bool buscando);
  void sinConexion(const char *motivo, uint32_t segundos);
  void iniciandoElm(const char *comando);
  void elmNoResponde(uint32_t segundos);
  void buscandoEcu(const char *version);
  void sinEcu(const char *version, const LineaDato &voltaje, const char *motivo, uint32_t segundos);
  void descubriendo(uint16_t hechos, uint16_t total);

  // Para Paginas: la barra con el título y un punto por página (numero, de
  // 0 a total - 1, en blanco), y debajo el área de las páginas.
  void pagina(const char *titulo, uint8_t numero, uint8_t total);
  // Donde Paginas arma las suyas: 640x144, debajo de la barra. Se ve solo
  // después de pagina(). nullptr sin pantalla.
  lv_obj_t *areaPaginas() const;

private:
  PantallaHw &hw;
  bool lista = false;
};
