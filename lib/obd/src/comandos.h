#pragma once

#include <stddef.h>
#include <stdint.h>

// PIDs que entran en un pedido del servicio 01 (010C0D050F...), según
// SAE J1979 para CAN
constexpr uint8_t MAX_PIDS_POR_PEDIDO = 6;

// Lista blanca de comandos para el ELM327: solo pasan los de lectura.
//
// Antes de comparar se pasa a mayúsculas y se quitan los espacios, igual
// que hace el ELM. Un carácter de control (incluidos \r y \n) o no ASCII
// rechaza el comando entero.
bool comandoPermitido(const char *cmd);

// Copia cmd a salida en mayúsculas y sin espacios. false si cmd tiene
// caracteres de control o no ASCII, si queda vacío o si no entra en salida.
bool normalizarComando(const char *cmd, char *salida, size_t tam);
