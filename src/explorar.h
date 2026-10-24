#pragma once

// Exploración de adaptadores BLE (E0): búsqueda y servicios. No le manda
// comandos al ELM: eso es solo de Elm327::enviar(). Usa su propia conexión
// y la corta al terminar. El BLE lo inicia Elm327::iniciar().
class Explorador {
public:
  // !escanear: búsqueda activa de BUSQUEDA_BLE_S y lista numerada
  void escanear();
  // !explorar N: conecta al N de la lista, muestra servicios y
  // características, y desconecta
  void explorar(int numero);
};
