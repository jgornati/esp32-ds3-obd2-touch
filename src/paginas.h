#pragma once

#include <stdint.h>

#include <dtc.h>

#include "datos.h"
#include "pantalla.h"

// Lo que muestra la página Info y no sale del sondeo
struct InfoConexion {
  const char *version;         // del ELM: "v2.1"
  const char *protocolo;       // de ATDPN: "6", o "A6" si lo eligió en automático
  uint8_t pids;                // PIDs soportados en el servicio 01
  int16_t consultasPorSegundo; // -1: todavía no se midió
};

// Magnitudes de una página: entran en una sola consulta de varios PIDs
constexpr uint8_t MAX_VISIBLES = 6;

// Las páginas de LEYENDO, como en la maqueta de E4: Motor, Aire/Volt,
// Carga, Mezcla, Fallas e Info. Se pasa a la anterior o a la siguiente
// tocando la mitad izquierda o derecha de la pantalla; después de la última
// vuelve a la primera. El toque llega en un evento de LVGL, dentro de
// PantallaHw::atender(), que también corre mientras se espera al ELM: queda
// como pedido (tomarToque()) y quien llama cambia la página.
class Paginas {
public:
  explicit Paginas(Pantalla &pantalla) : pantalla(pantalla) {}

  // Arma las páginas, después de Pantalla::iniciar(). Sin pantalla, dibujar()
  // no hace nada; lo demás anda igual.
  void iniciar();
  // +1 la siguiente, -1 la anterior
  void cambiar(int8_t paso);
  const char *titulo() const;
  // Las magnitudes que se ven en la página actual, para que el sondeo las
  // consulte en cada vuelta. Devuelve cuántas son.
  uint8_t visibles(Magnitud lista[MAX_VISIBLES]) const;
  // Las de la primera página (Motor)
  uint8_t primeras(Magnitud lista[MAX_VISIBLES]) const;
  // La página Fallas no sale del sondeo: quien llama la lee aparte
  bool muestraFallas() const;
  // -1 o +1 si se tocó la mitad izquierda o derecha desde la última
  // llamada; 0 si no. Solo cuenta con las páginas a la vista.
  int8_t tomarToque();
  // fallas: la última lectura de la página Fallas. reintento: segundos
  // hasta volver a leerla, que se muestran si falló y no hay códigos.
  void dibujar(uint32_t ahora, const InfoConexion &info, const LecturaFallas &fallas,
               uint32_t reintento);

private:
  Pantalla &pantalla;
  uint8_t actual = 0;
  bool armadas = false;
};
