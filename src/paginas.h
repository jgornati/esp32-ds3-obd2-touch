#pragma once

#include <stdint.h>

#include <dtc.h>
#include <viaje.h>

#include "datos.h"
#include "pantalla.h"

// Lo que muestra la página Info y no sale del sondeo
struct DatosInfo {
  const char *version;         // del ELM: "v2.1"
  const char *protocolo;       // de ATDPN: "6", o "A6" si lo eligió en automático
  uint8_t pids;                // PIDs soportados en el servicio 01
  int16_t consultasPorSegundo; // -1: todavía no se midió
  const char *direccion;       // del adaptador: "81:23:45:67:89:BA"
  int16_t rssi;                // dBm; 0: sin medir (siempre es negativo)
  uint8_t brillo;              // nivel, de 1 a CANT_NIVELES_BRILLO
};

// La alarma que se ve en la barra (E11) y las magnitudes que van en rojo
struct AlarmaALaVista {
  const char *texto;   // "REFRIGERANTE 106 °C"; nullptr si no hay ninguna
  uint16_t magnitudes; // un bit por Magnitud (1 << REFRIGERANTE)
};
static_assert(CANT_MAGNITUDES <= 16, "un bit por Magnitud en AlarmaALaVista");

// Lo que se pide con los botones de Info y manteniendo apretada Viaje
enum class Accion : uint8_t {
  NINGUNA,
  RECONECTAR,
  OLVIDAR,
  BAJAR_BRILLO,
  SUBIR_BRILLO,
  REINICIAR_VIAJE
};

// Magnitudes de una página: entran en una sola consulta de varios PIDs
constexpr uint8_t MAX_VISIBLES = 6;

// Las páginas de LEYENDO, como en la maqueta de E4: Motor, Aire/Volt,
// Carga, Mezcla, Fallas e Info, después Viaje (E10) y al final DS3 (E9), una
// animación a pantalla completa. Se pasa a la anterior o a la siguiente
// tocando la mitad izquierda o derecha de la pantalla; después de la última
// vuelve a la primera. Los botones de Info se quedan con su toque y no
// cambian la página; en Viaje, soltarla después de mantenerla apretada
// tampoco. Los toques llegan en eventos de LVGL, dentro de
// PantallaHw::atender(), que también corre mientras se espera al ELM:
// quedan como pedidos (tomarToque() y tomarAccion()) y quien llama actúa.
class Paginas {
public:
  explicit Paginas(Pantalla &marco) : pantalla(marco) {}

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
  // En Info, quien llama mide el RSSI
  bool muestraInfo() const;
  // -1 o +1 si se tocó la mitad izquierda o derecha desde la última
  // llamada; 0 si no. Solo cuenta con las páginas a la vista. En Viaje, un
  // toque de AVISO_MANTENER_MS o más no cuenta: es para reiniciar.
  int8_t tomarToque();
  // Lo que se pidió desde la última llamada. Reconectar y el brillo cuentan
  // al soltarlos;
  // Olvidar, una vez por apretada, cuando lleva MANTENER_OLVIDAR_MS
  // apretado, y reiniciar el viaje, cuando la página Viaje lleva
  // MANTENER_VIAJE_MS apretada (ahora es la hora de millis()).
  Accion tomarAccion(uint32_t ahora);
  // fallas: la última lectura de la página Fallas. reintento: segundos
  // hasta volver a leerla, que se muestran si falló y no hay códigos. alarma:
  // va en la barra, en lugar del título, también en DS3.
  void dibujar(uint32_t ahora, const DatosInfo &info, const LecturaFallas &fallas,
               uint32_t reintento, const Viaje &viaje, const AlarmaALaVista &alarma);

private:
  Pantalla &pantalla;
  uint8_t actual = 0;
  bool armadas = false;
};
