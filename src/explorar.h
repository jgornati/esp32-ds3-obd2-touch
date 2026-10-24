#pragma once

#include <stdint.h>

// Exploración de adaptadores OBD por BLE (E0): búsqueda, servicios y una
// conexión de prueba para mandarle comandos. Una conexión a la vez y nada
// guardado en la flash.
class Explorador {
public:
  // Llamar una vez en setup()
  bool iniciar();
  // Llamar en cada vuelta de loop(): informa desconexiones y emparejamientos
  void atender();

  // !escanear: búsqueda activa de BUSQUEDA_BLE_S y lista numerada
  void escanear();
  // !explorar N: conecta al N de la lista y muestra servicios y características
  void explorar(int numero);
  // !usar N [S]: se suscribe a la característica de notificación del servicio
  // S (o del que parezca OBD) y anota la de escritura
  void usar(int numero, int servicio = 0);
  void desconectar();

  // Manda un comando al adaptador elegido con !usar y espera el '>'. Es el
  // único camino hacia el adaptador y pasa por comandoPermitido().
  void enviar(const char *comando, uint32_t timeoutMs);
};
