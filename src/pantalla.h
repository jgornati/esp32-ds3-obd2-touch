#pragma once

#include <stdint.h>

#include <lvgl.h>

#include "pantalla_hw.h"

// Puntos de página que caben en la barra: las 8 de hoy y FUERZA G (E12)
constexpr uint8_t MAX_PAGINAS = 9;

// La pantalla de espera (E9), las de estado que quedan (E3) y el marco de
// las páginas (E4), con LVGL. Cada función arma una pantalla completa; LVGL
// la dibuja en el próximo PantallaHw::atender(), y solo si algo cambió. Los
// valores llegan ya como texto: quien llama decide si están vigentes.
class Pantalla {
public:
  explicit Pantalla(PantallaHw &hardware) : hw(hardware) {}

  // Crea los objetos. Si la pantalla no se pudo iniciar, las demás no hacen nada.
  void iniciar();

  // Mientras no hay datos (adaptador, ELM y ECU): negra, con el estado abajo
  // a la izquierda, en gris o, si es un problema, en ámbar. A los
  // ESPERA_ENTRADA_MS de iniciar() entra el DS3, una sola vez, y queda quieto
  // con "Toca para reintentar" abajo a la derecha. Si en ese momento no se ve
  // la espera, después aparece directamente quieto.
  void espera(const char *estado, bool problema = false);
  // Un toque en la espera desde la última llamada (reintentar ya)
  bool tomarReintento();
  // funcion se llama cuando la espera lleva MANTENER_REINICIO_MS apretada
  // (reiniciar la placa). Corre dentro de PantallaHw::atender(), que también
  // atiende la otra tarea mientras se conecta.
  void alMantenerApretada(void (*funcion)());
  void alimentacionBaja();
  void descubriendo(uint16_t hechos, uint16_t total);

  // Para Paginas: la barra con el título y un punto por página (numero, de
  // 0 a total - 1, en blanco), y debajo el área de las páginas. mantenida,
  // de 0 a 1, es lo que lleva la página apretada para borrar algo: con más
  // de 0, la barra se llena de rojo hasta ahí y el título va en rojo claro.
  void pagina(const char *titulo, uint8_t numero, uint8_t total, float mantenida = 0);
  // Donde Paginas arma las suyas: 640x144, debajo de la barra. Se ve solo
  // después de pagina(). nullptr sin pantalla.
  lv_obj_t *areaPaginas() const;
  // Una página de 640x172, sin la barra (DS3). Se ve solo después de
  // paginaCompleta(). nullptr sin pantalla.
  void paginaCompleta();
  lv_obj_t *areaCompleta() const;

private:
  PantallaHw &hw;
  bool lista = false;
};
