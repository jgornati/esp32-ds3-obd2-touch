#pragma once

#include <stddef.h>
#include <stdint.h>

// El registro del viaje en la microSD (E13): un CSV por encendido,
// VIAJE0001.CSV, con una línea por segundo. Acá lo que no depende de la
// tarjeta: armar las líneas, los nombres de los archivos y el búfer donde
// esperan a la tarea que las escribe.

// Un renglón del CSV estándar: campos separados por coma, con punto
// decimal. Un texto con coma, comillas o salto de línea va entre comillas.
class LineaCsv {
public:
  LineaCsv(char *destino, size_t tam) : salida(destino), capacidad(tam) {}
  // Un campo; nullptr o "" lo deja vacío
  void texto(const char *t);
  // Cierra el renglón con '\n' y devuelve su largo; 0 si no entró
  size_t terminar();

private:
  void poner(char c);

  char *salida;
  size_t capacidad;
  size_t largo = 0;
  bool primero = true;
  bool desbordada = false;
};

// Lo que pasó desde la última línea, en un solo campo: "SIN_ECU | LEYENDO".
// Lo que no entra se corta con "...".
constexpr size_t LARGO_EVENTOS = 160;

class Eventos {
public:
  void agregar(const char *texto);
  const char *texto() const {
    return actuales;
  }
  void vaciar() {
    actuales[0] = '\0';
    cortado = false;
  }

private:
  char actuales[LARGO_EVENTOS] = "";
  bool cortado = false;
};

// VIAJE0001.CSV a VIAJE9999.CSV, en la raíz de la tarjeta
constexpr uint16_t MAX_VIAJES = 9999;

// El número de un archivo de viaje (el nombre, sin la carpeta), sin mirar
// mayúsculas. false si no es uno.
bool numeroDeViaje(const char *nombre, uint16_t *numero);
// "VIAJE0012.CSV"
void nombreDeViaje(uint16_t numero, char *salida, size_t tam);

// Las líneas que esperan a la tarea de la tarjeta. Cada una entra entera o
// no entra, y entonces se cuenta como perdida.
class BuferTexto {
public:
  void usar(char *memoria, size_t tam);
  bool agregar(const char *texto, size_t largo);
  // Pasa a destino lo más viejo, hasta tam bytes, y lo saca del búfer.
  // Devuelve cuántos pasó.
  size_t tomar(char *destino, size_t tam);
  size_t usados() const {
    return ocupados;
  }
  uint32_t perdidas() const {
    return lineasPerdidas;
  }

private:
  char *datos = nullptr;
  size_t capacidad = 0;
  size_t ocupados = 0;
  uint32_t lineasPerdidas = 0;
};

enum class EstadoTarjeta : uint8_t {
  BUSCANDO,    // todavía no se probó
  SIN_TARJETA, // no contesta
  ILEGIBLE,    // contesta, pero no tiene FAT o está dañada
  LISTA,       // el archivo se abre con la primera lectura de la ECU
  GRABANDO,
  ERROR, // falló al abrir el archivo o al escribir; se reintenta
  LLENA,
};

// Para Info: "Grabando VIAJE0012", "Sin tarjeta" o "..." (viaje: el número
// del archivo)
void textoTarjeta(EstadoTarjeta estado, uint16_t viaje, char *salida, size_t tam);
// Hay tarjeta pero no se puede grabar: va en ámbar
bool problemaTarjeta(EstadoTarjeta estado);
