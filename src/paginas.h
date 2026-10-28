#pragma once

#include <stdint.h>

#include <anteriores.h>
#include <dtc.h>
#include <fuerza_g.h>
#include <marcha.h>
#include <registro.h>
#include <viaje.h>

#include "datos.h"
#include "pantalla.h"

// Lo que muestra la página Info y no sale del sondeo
struct DatosInfo {
  const char *version;         // del ELM: "v2.1"
  const char *protocolo;       // de ATDPN: "6", o "A6" si lo eligió en automático
  uint8_t pids;                // PIDs soportados en el servicio 01
  int16_t consultasPorSegundo; // -1: todavía no se midió
  int16_t rssi;                // dBm; 0: sin medir (siempre es negativo)
  uint8_t brillo;              // nivel, de 1 a CANT_NIVELES_BRILLO
  uint16_t cambio;             // las RPM del aviso de cambio (E25); 0 apagado
  EstadoTarjeta tarjeta;       // la microSD (E13); también la marca de la barra
  uint16_t viaje;              // el número del archivo; 0 sin archivo
};

// Lo que muestra la página Fuerza G (E12)
struct DatosFuerzaG {
  bool conSensor; // el IMU contestó al arrancar
  LecturaG lectura;
};

// La alarma que se ve en la barra (E11) y las magnitudes que van en rojo
struct AlarmaALaVista {
  const char *texto;   // "REFRIGERANTE 112 °C"; nullptr si no hay ninguna
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
  BAJAR_CAMBIO, // el aviso de cambio (E25)
  SUBIR_CAMBIO,
  REINICIAR_VIAJE
};

// Magnitudes de una página: entran en una sola consulta de varios PIDs
constexpr uint8_t MAX_VISIBLES = 6;

// Las páginas de LEYENDO, como en la maqueta de E4: Motor (desde E24, con la
// marcha y el reloj del refrigerante), Carga, Mezcla, Fallas e Info, después
// Viaje (E10), Anteriores (E22) y Fuerza G (E12), y al final DS3 (E9), una
// animación a pantalla completa. Aire/Volt salió en E24. Se
// pasa a la siguiente deslizando el dedo hacia la izquierda, y a la anterior
// hacia la derecha (E16), en cualquier parte de la pantalla, también sobre
// los botones de Info, que entonces no se aprietan; después de la última
// vuelve a la primera. Tocar no cambia de página. Sin datos (E22) se ve la
// espera, que arma Pantalla, y deslizando se pasa a Anteriores y a DS3. Los
// toques llegan en eventos de LVGL, dentro de PantallaHw::atender(), que
// también corre mientras se espera al ELM: quedan como pedidos
// (tomarToque(), tomarOtroViaje() y tomarAccion()) y quien llama actúa.
class Paginas {
public:
  explicit Paginas(Pantalla &marco) : pantalla(marco) {}

  // Arma las páginas, después de Pantalla::iniciar(). Sin pantalla, dibujar()
  // no hace nada; lo demás anda igual. Empieza con datos, en Motor.
  void iniciar();
  // Si hay datos (LEYENDO). Al irse los datos queda Anteriores o DS3 si se
  // veía, y si no la espera; al volver, sigue la que se veía sin datos, o
  // si era la espera, la última con datos.
  void conDatos(bool con);
  // +1 la siguiente, -1 la anterior, entre las de ahora (con datos o sin)
  void cambiar(int8_t paso);
  // El de la página ("ANTERIORES"); "ESPERA" sin datos en la espera
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
  // En Fuerza G el punto se mueve: quien llama la dibuja más seguido
  bool muestraFuerzaG() const;
  // Sin datos, en la espera: la dibuja quien llama, con Pantalla::espera()
  bool muestraEspera() const;
  // +1 si se deslizó el dedo hacia la izquierda (la siguiente) desde la
  // última llamada, -1 hacia la derecha (la anterior); 0 si no. Solo cuenta
  // con las páginas o la espera a la vista.
  int8_t tomarToque();
  // Se tocó Anteriores desde la última llamada: ya pasó al viaje anterior, y
  // quien llama la dibuja
  bool tomarOtroViaje();
  // Sin datos, funcion se llama apenas hay un pedido (deslizar o tocar
  // Anteriores), desde el evento de LVGL: puede ser de la otra tarea, si
  // loop() está conectando el BLE (hasta TIMEOUT_CONEXION_MS sin atender la
  // pantalla). Así la página cambia enseguida. Con datos, los pedidos
  // esperan a quien llama.
  void alPedirSinDatos(void (*funcion)());
  // Lo pedido desde la última vez, de a uno y en orden: quien llama repite
  // hasta NINGUNA (desde E25 cada toque cuenta, aunque haya varios entre dos
  // llamadas). Reconectar, el brillo y el aviso de cambio cuentan al soltarlos
  // (si no fue un deslizamiento); Olvidar, una vez por apretada, cuando lleva
  // MANTENER_OLVIDAR_MS apretado, y reiniciar el viaje, cuando la página
  // Viaje lleva MANTENER_VIAJE_MS apretada con el dedo quieto (ahora es la
  // hora de millis()).
  Accion tomarAccion(uint32_t ahora);
  // fallas: la última lectura de la página Fallas. reintento: segundos
  // hasta volver a leerla, que se muestran si falló y no hay códigos. alarma:
  // va en la barra, en lugar del título, también en DS3. info.tarjeta da
  // también la marca "SD" de la barra.
  void dibujar(uint32_t ahora, const DatosInfo &info, const LecturaFallas &fallas,
               uint32_t reintento, const Viaje &viaje, const Marcha &marcha,
               const Anteriores &anteriores, const DatosFuerzaG &fuerzaG,
               const AlarmaALaVista &alarma);
  // Sin datos, Anteriores o DS3; en la espera, nada. tarjeta: para la marca
  // "SD" de la barra.
  void dibujarSinDatos(const Anteriores &anteriores, EstadoTarjeta tarjeta);

private:
  Pantalla &pantalla;
  uint8_t actual = 0;
  uint8_t ultimaConDatos = 0; // la que se veía al irse los datos
  bool armadas = false;
};
