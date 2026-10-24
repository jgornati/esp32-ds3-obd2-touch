#pragma once

#include <stddef.h>
#include <stdint.h>

#include "respuesta.h"

// Lo que entra en Respuesta::datos: 1 byte de cantidad y 2 por código.
constexpr uint8_t MAX_DTC = 31;

struct ListaDtc {
  uint8_t cantidad;
  uint16_t codigos[MAX_DTC]; // primer byte << 8 | segundo
};

// 0x0133 -> "P0133". salida tiene que tener lugar para 6 caracteres.
void textoDtc(uint16_t codigo, char *salida);

// Descripción corta (hasta 14 caracteres) de los códigos genéricos más
// comunes: 0x0420 -> "Catalizador". nullptr si no está en la tabla, como
// los propios de cada marca (P1xxx).
const char *descripcionDtc(uint16_t codigo);

// Datos de 03, 07 o 0A en CAN, ya sin el 43/47/4A: el primer byte es la
// cantidad y siguen 2 bytes por código. false si faltan bytes o no entran.
bool interpretarDtcs(const uint8_t *datos, uint8_t largo, ListaDtc *lista);

// Lo que se sabe de un servicio de códigos de falla (03, 07 o 0A).
enum class EstadoCodigos : uint8_t {
  SIN_LEER,
  LEIDOS,
  ERROR,        // no contestó bien
  NO_DISPONIBLE // 0A sin datos: la ECU no tiene códigos permanentes
};

struct Codigos {
  EstadoCodigos estado = EstadoCodigos::SIN_LEER;
  TipoRespuesta tipo = TipoRespuesta::VACIA; // lo que contestó el ELM
  ListaDtc lista = {};
};

// Respuesta a 03, 07 o 0A. completa: llegó el '>'; si no, es ERROR aunque
// haya datos. NO DATA es NO_DISPONIBLE en 0A y ERROR en 03 y 07.
Codigos interpretarCodigos(const char *cruda, const char *comando, bool completa);

// Una lectura de la página Fallas (E7).
struct LecturaFallas {
  bool leida = false;      // ya se consultó todo
  bool luzLeida = false;   // 0101 contestó bien
  bool luz = false;        // luz de falla encendida
  uint8_t cantidadLuz = 0; // códigos guardados según 0101
  Codigos guardados;       // 03
  Codigos pendientes;      // 07
  Codigos permanentes;     // 0A
};

// "Sin fallas" solo si 03 y 07 se leyeron con cero códigos y 0A no trajo
// ninguno. Un error en 0A no lo impide: es opcional y el DS3 no lo tiene.
bool sinFallas(const LecturaFallas &lectura);

// Para la pantalla: "..." (sin leer), "--" (0101 falló), "Apagada" o "Encendida".
const char *textoLuz(const LecturaFallas &lectura);

// Las líneas 2 a 4 de la página Fallas, de hasta 14 caracteres.
constexpr uint8_t RENGLONES_FALLAS = 3;
constexpr size_t LARGO_RENGLON = 15; // con el '\0'

struct PantallaFallas {
  uint8_t cantidad;
  char renglones[RENGLONES_FALLAS][LARGO_RENGLON];
};

// "Leyendo...", "Error al leer" (si 03 o 07 fallaron), "Sin fallas" o los
// códigos: cada tipo con su encabezado ("2 guardadas") y cada código en su
// renglón, con la descripción abajo si se conoce. Si no entran en una
// pantalla se arman varias; un tipo que sigue en otra pantalla repite el
// encabezado. Deja en salida la pantalla número vuelta % total y devuelve
// el total.
uint8_t pantallaFallas(const LecturaFallas &lectura, uint32_t vuelta, PantallaFallas *salida);
