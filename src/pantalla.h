#pragma once

#include <stdint.h>

#include "pantalla_hw.h"

// Una línea: etiqueta a la izquierda y valor a la derecha. Cualquiera de
// los dos puede ser "".
struct LineaDato {
  const char *etiqueta;
  const char *valor;
  // false: el valor no es válido y actual ("--", "...", "N/D") y va en gris
  bool actual;
};

// Pantallas de estado y la provisoria de datos (E3), con LVGL. Barra de
// estado arriba y debajo hasta 4 líneas. Cada función arma una pantalla
// completa; LVGL la dibuja en el próximo PantallaHw::atender(), y solo si
// algo cambió. Los valores llegan ya como texto: quien llama decide si
// están vigentes.
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
  // Provisoria hasta E4: grande a la izquierda (con la etiqueta como
  // unidad) y hasta 4 líneas a la derecha.
  void datos(const char *version, const LineaDato &principal, const LineaDato *lineas,
             uint8_t cantidad);

private:
  PantallaHw &hw;
  bool lista = false;
};
