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

// Descripción corta de los códigos genéricos más comunes: 0x0420 ->
// "Catalizador". nullptr si no está en la tabla, como los propios de cada
// marca (P1xxx). Con " (pendiente)" detrás entra en un renglón de la página.
constexpr size_t LARGO_DESCRIPCION = 14;
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

// Una lectura de la página Fallas.
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

// 03 o 07 no contestaron bien: se reintenta antes y nunca es "Sin fallas"
bool errorAlLeer(const LecturaFallas &lectura);

// Para la pantalla: "..." (sin leer), "--" (0101 falló), "Apagada" o "Encendida".
const char *textoLuz(const LecturaFallas &lectura);

// La página Fallas: a la izquierda la luz y la cantidad de cada tipo, a la
// derecha los códigos, de a RENGLONES_DTC.
enum class VistaFallas : uint8_t {
  LEYENDO,    // todavía no hay lectura
  SIN_FALLAS, // sinFallas() y además la luz no está encendida
  CODIGOS     // la luz, las cantidades y los códigos; también sin ninguno o con un error
};

// En el orden en que se muestran
enum class TipoDtc : uint8_t { GUARDADO, PERMANENTE, PENDIENTE };

struct RenglonDtc {
  uint16_t codigo;
  TipoDtc tipo;
};

constexpr uint8_t RENGLONES_DTC = 4;

struct PantallaFallas {
  VistaFallas vista;
  uint8_t grupos;   // de RENGLONES_DTC códigos; 0 si no hay ninguno
  uint8_t grupo;    // el que se ve, de 0 a grupos - 1
  uint8_t cantidad; // renglones de este grupo
  RenglonDtc renglones[RENGLONES_DTC];
};

// Cada código va una sola vez, con el primer tipo en que aparece:
// guardados (03), permanentes (0A) y pendientes (07). Los de un servicio
// que no contestó bien no están; los de los otros, sí. Si no entran, se
// arman grupos y se ve el número vuelta % grupos.
PantallaFallas pantallaFallas(const LecturaFallas &lectura, uint32_t vuelta);

// Para la columna izquierda: "2 guardadas", "1 pendiente", o "Guardadas:
// error" si el servicio no se leyó. true si se leyó.
bool textoCantidad(const Codigos &codigos, TipoDtc tipo, char *salida, size_t tam);
