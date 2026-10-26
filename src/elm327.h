#pragma once

#include <Arduino.h>

enum class EstadoTransporte { OK, TIMEOUT, DESCONECTADO, RECHAZADO };

struct ResultadoComando {
  EstadoTransporte estado;
  String cruda; // todo lo recibido antes de '>', sin modificar
  uint32_t ms;  // duración del comando
};

enum class FaseConexion { POR_DIRECCION, BUSCANDO };

// Transporte BLE con el ELM327. No interpreta el contenido OBD.
class Elm327 {
public:
  // Inicia el BLE. conectar() lo llama si hace falta; llamarlo antes si
  // otro módulo usa el BLE.
  bool iniciar();
  // Dirección guardada primero; si falla, busca por nombre o servicio.
  // aviso, si no es nullptr, se llama antes de cada fase (para mostrarla
  // en pantalla). Tarda como mucho TIMEOUT_CONEXION_MS por intento más
  // BUSQUEDA_ELM_S de búsqueda.
  bool conectar(void (*aviso)(FaseConexion) = nullptr);
  // Desconecta y borra la dirección guardada y los vínculos BLE.
  void olvidar();
  bool conectado();
  void desconectar();
  // Del adaptador conectado, en mayúsculas ("81:23:45:67:89:BA"); vacía sin conexión
  String direccion();
  // dBm de la conexión, según la radio de la placa (no le pregunta al ELM).
  // 0 sin conexión o si no se pudo leer.
  int rssi();
  // registrar = false: solo deja en el log lo que no salió OK (para el sondeo)
  ResultadoComando enviar(const char *comando, uint32_t timeoutMs, bool registrar = true);
  // funcion se llama mientras enviar() espera la respuesta y mientras se
  // busca el adaptador (para la pantalla). No tiene que usar el ELM.
  void mientrasEspera(void (*funcion)()) {
    espera = funcion;
  }

private:
  bool intentarConexion(void (*aviso)(FaseConexion));
  bool prepararConexion();
  bool buscar(uint8_t *tipo, String *direccion);

  bool iniciado = false;
  void (*espera)() = nullptr;
};

const char *nombreEstado(EstadoTransporte estado);
