#include "comandos.h"

#include <stddef.h>
#include <string.h>

// Sin el prefijo "AT". ATSTxx se valida aparte.
static const char *const COMANDOS_AT[] = {
    "Z",  "WS", "D",  "I",  "@1", "RV",  "DP",  "DPN", "E0",  "E1",  "L0",
    "L1", "S0", "S1", "H0", "H1", "SP0", "SP6", "AT0", "AT1", "AT2",
};

// El más largo permitido tiene 6 ("ATSTxx"); lo que no entra se rechaza.
static const size_t LARGO_MAXIMO = 8;

static bool esHex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F');
}

static bool todosHex(const char *s) {
  for (; *s; s++) {
    if (!esHex(*s)) {
      return false;
    }
  }
  return true;
}

bool normalizarComando(const char *cmd, char *salida, size_t tam) {
  if (cmd == nullptr || tam == 0) {
    return false;
  }
  size_t largo = 0;
  for (const char *p = cmd; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == ' ') {
      continue;
    }
    if (c < 0x20 || c > 0x7E || largo + 1 >= tam) {
      return false;
    }
    if (c >= 'a' && c <= 'z') {
      c = c - 'a' + 'A';
    }
    salida[largo++] = (char)c;
  }
  salida[largo] = '\0';
  return largo > 0;
}

static bool atPermitido(const char *resto) {
  for (const char *permitido : COMANDOS_AT) {
    if (strcmp(resto, permitido) == 0) {
      return true;
    }
  }
  // ATSTxx: timeout del ELM, xx en hex
  return strlen(resto) == 4 && strncmp(resto, "ST", 2) == 0 && todosHex(resto + 2);
}

static bool obdPermitido(const char *cmd) {
  size_t largo = strlen(cmd);
  if (largo == 2) {
    return strcmp(cmd, "03") == 0 || strcmp(cmd, "07") == 0 || strcmp(cmd, "0A") == 0;
  }
  // 01PP y 09PP, con o sin cantidad de respuestas (1 dígito hex)
  if (largo == 4 || largo == 5) {
    bool modo = strncmp(cmd, "01", 2) == 0 || strncmp(cmd, "09", 2) == 0;
    return modo && todosHex(cmd + 2);
  }
  return false;
}

bool comandoPermitido(const char *cmd) {
  char normal[LARGO_MAXIMO + 1];
  if (!normalizarComando(cmd, normal, sizeof(normal))) {
    return false;
  }

  if (strncmp(normal, "AT", 2) == 0) {
    return atPermitido(normal + 2);
  }
  return obdPermitido(normal);
}
