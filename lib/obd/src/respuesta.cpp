#include "respuesta.h"

#include <string.h>

#include "comandos.h"

struct Linea {
  const char *texto;
  size_t largo;
};

struct Error {
  const char *texto;
  TipoRespuesta tipo;
};

// Se buscan dentro de la línea: "<DATA ERROR" puede venir pegado a los datos.
static const Error ERRORES[] = {
    {"UNABLE TO CONNECT", TipoRespuesta::SIN_ECU}, {"NO DATA", TipoRespuesta::NO_DATA},
    {"BUFFER FULL", TipoRespuesta::BUFFER_LLENO},  {"STOPPED", TipoRespuesta::DETENIDO},
    {"CAN ERROR", TipoRespuesta::ERROR_BUS},       {"BUS ERROR", TipoRespuesta::ERROR_BUS},
    {"BUS BUSY", TipoRespuesta::ERROR_BUS},        {"FB ERROR", TipoRespuesta::ERROR_BUS},
    {"DATA ERROR", TipoRespuesta::ERROR_BUS},      {"RX ERROR", TipoRespuesta::ERROR_BUS},
    {"LV RESET", TipoRespuesta::AVISO_ELM},        {"ACT ALERT", TipoRespuesta::AVISO_ELM},
    {"LP ALERT", TipoRespuesta::AVISO_ELM},
};

// Lo que se pidió: modo y, para 01 y 09, los PIDs (varios solo en 01).
struct Pedido {
  uint8_t modo;
  bool conPid;
  uint8_t pids[MAX_PIDS_POR_PEDIDO];
  uint8_t cantidadPids;
};

static char mayuscula(char c) {
  return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

static int valorHex(char c) {
  c = mayuscula(c);
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

static bool esEspacio(char c) {
  return c == ' ' || c == '\t';
}

static bool esSeparador(char c) {
  return c == '\r' || c == '\n' || c == '>';
}

// Próxima línea, sin espacios en los extremos (puede quedar vacía).
static bool siguienteLinea(const char *&p, Linea &linea) {
  while (*p != '\0' && esSeparador(*p)) {
    p++;
  }
  if (*p == '\0') {
    return false;
  }
  const char *inicio = p;
  while (*p != '\0' && !esSeparador(*p)) {
    p++;
  }
  const char *fin = p;
  while (inicio < fin && esEspacio(*inicio)) {
    inicio++;
  }
  while (fin > inicio && esEspacio(fin[-1])) {
    fin--;
  }
  linea = {inicio, (size_t)(fin - inicio)};
  return true;
}

static bool empiezaCon(const Linea &l, const char *prefijo) {
  size_t n = strlen(prefijo);
  if (l.largo < n) {
    return false;
  }
  for (size_t i = 0; i < n; i++) {
    if (mayuscula(l.texto[i]) != prefijo[i]) {
      return false;
    }
  }
  return true;
}

static bool contiene(const Linea &l, const char *buscado) {
  for (size_t i = 0; i < l.largo; i++) {
    Linea resto = {l.texto + i, l.largo - i};
    if (empiezaCon(resto, buscado)) {
      return true;
    }
  }
  return false;
}

// "SEARCHING..." puede venir sola o delante de la respuesta.
static void quitarBuscando(Linea &l) {
  if (!empiezaCon(l, "SEARCHING")) {
    return;
  }
  size_t i = strlen("SEARCHING");
  while (i < l.largo && (l.texto[i] == '.' || esEspacio(l.texto[i]))) {
    i++;
  }
  l.texto += i;
  l.largo -= i;
}

// Compara contra el comando normalizado, sin espacios ni mayúsculas.
static bool esEco(const Linea &l, const char *comando) {
  size_t j = 0;
  for (size_t i = 0; i < l.largo; i++) {
    if (esEspacio(l.texto[i])) {
      continue;
    }
    if (comando[j] == '\0' || mayuscula(l.texto[i]) != comando[j]) {
      return false;
    }
    j++;
  }
  return j > 0 && comando[j] == '\0';
}

static bool buscarError(const Linea &l, TipoRespuesta &tipo) {
  if (l.largo == 1 && l.texto[0] == '?') {
    tipo = TipoRespuesta::NO_SOPORTADO;
    return true;
  }
  for (const Error &error : ERRORES) {
    if (contiene(l, error.texto)) {
      tipo = error.tipo;
      return true;
    }
  }
  // ERRxx: error interno del ELM
  if (l.largo == 5 && empiezaCon(l, "ERR") && valorHex(l.texto[3]) >= 0 &&
      valorHex(l.texto[4]) >= 0) {
    tipo = TipoRespuesta::ERROR_BUS;
    return true;
  }
  return false;
}

// Pares de dígitos hex, con o sin espacios entre bytes.
static bool leerBytes(const Linea &l, uint8_t *bytes, size_t max, size_t &cantidad) {
  cantidad = 0;
  int alto = -1;
  for (size_t i = 0; i < l.largo; i++) {
    if (esEspacio(l.texto[i])) {
      // Un espacio en medio de un byte ("4 1") no es una respuesta del ELM
      if (alto >= 0) {
        return false;
      }
      continue;
    }
    int valor = valorHex(l.texto[i]);
    if (valor < 0) {
      return false;
    }
    if (alto < 0) {
      alto = valor;
      continue;
    }
    if (cantidad == max) {
      return false;
    }
    bytes[cantidad++] = (uint8_t)(alto << 4 | valor);
    alto = -1;
  }
  return alto < 0 && cantidad > 0;
}

// Primera línea de un multi-frame CAN: cantidad de bytes en 3 dígitos ("014").
static bool esLineaCantidad(const Linea &l, size_t &cantidad) {
  if (l.largo != 3) {
    return false;
  }
  cantidad = 0;
  for (size_t i = 0; i < 3; i++) {
    int valor = valorHex(l.texto[i]);
    if (valor < 0) {
      return false;
    }
    cantidad = cantidad * 16 + valor;
  }
  return true;
}

// Una línea de un multi-frame: "0: 49 02 01 ..." o "0:490201...".
static bool esLineaFrame(const Linea &l, int &indice, Linea &resto) {
  if (l.largo < 2 || l.texto[1] != ':') {
    return false;
  }
  indice = valorHex(l.texto[0]);
  if (indice < 0) {
    return false;
  }
  resto = {l.texto + 2, l.largo - 2};
  while (resto.largo > 0 && esEspacio(resto.texto[0])) {
    resto.texto++;
    resto.largo--;
  }
  return true;
}

static bool leerPedido(const char *comando, Pedido &pedido) {
  size_t largo = strlen(comando);
  if (largo < 2 || valorHex(comando[0]) < 0 || valorHex(comando[1]) < 0) {
    return false;
  }
  pedido.modo = (uint8_t)(valorHex(comando[0]) << 4 | valorHex(comando[1]));
  pedido.conPid = pedido.modo == 0x01 || pedido.modo == 0x09;
  if (!pedido.conPid) {
    return largo == 2;
  }
  // 01PP, 01PPn o varios PIDs (010C0D...): el dígito de cantidad no
  // cambia la respuesta
  size_t n = (largo - 2) / 2;
  if (n == 0 || n > MAX_PIDS_POR_PEDIDO || (pedido.modo == 0x09 && n > 1)) {
    return false;
  }
  if ((largo - 2) % 2 == 1 && valorHex(comando[largo - 1]) < 0) {
    return false;
  }
  for (size_t i = 0; i < n; i++) {
    int alto = valorHex(comando[2 + 2 * i]);
    int bajo = valorHex(comando[3 + 2 * i]);
    if (alto < 0 || bajo < 0) {
      return false;
    }
    pedido.pids[i] = (uint8_t)(alto << 4 | bajo);
  }
  pedido.cantidadPids = (uint8_t)n;
  return true;
}

// Con varios PIDs solo se saca el 41: cada PID queda delante de sus bytes.
static size_t largoEncabezado(const Pedido &pedido) {
  return pedido.conPid && pedido.cantidadPids == 1 ? 2 : 1;
}

static bool encabezadoValido(const uint8_t *bytes, size_t n, const Pedido &pedido) {
  size_t encabezado = largoEncabezado(pedido);
  if (n < encabezado || n - encabezado > MAX_DATOS) {
    return false;
  }
  if (bytes[0] != 0x40 + pedido.modo) {
    return false;
  }
  if (!pedido.conPid) {
    return true;
  }
  // Con varios, la ECU puede contestarlos en otro orden
  for (uint8_t i = 0; i < pedido.cantidadPids; i++) {
    if (n >= 2 && bytes[1] == pedido.pids[i]) {
      return true;
    }
  }
  return false;
}

// Se queda con los datos de la primera respuesta válida y cuenta las demás.
static void registrar(Respuesta &r, const uint8_t *bytes, size_t n, const Pedido &pedido) {
  if (r.respuestas == 0) {
    size_t encabezado = largoEncabezado(pedido);
    r.largo = (uint8_t)(n - encabezado);
    memcpy(r.datos, bytes + encabezado, r.largo);
  }
  if (r.respuestas < 255) {
    r.respuestas++;
  }
}

Respuesta interpretar(const char *cruda, const char *comando) {
  Respuesta r = {};
  r.tipo = TipoRespuesta::INVALIDA;

  char normal[16];
  if (!normalizarComando(comando, normal, sizeof(normal))) {
    return r;
  }
  bool esAt = strncmp(normal, "AT", 2) == 0;
  Pedido pedido = {};
  if (!esAt && !leerPedido(normal, pedido)) {
    return r;
  }

  bool hayError = false;
  TipoRespuesta error = TipoRespuesta::INVALIDA;
  bool hayTexto = false;

  // Multi-frame CAN en armado. siguiente es -1 si no hay uno en curso.
  uint8_t mensaje[MAX_DATOS + 2];
  size_t esperados = 0;
  size_t recibidos = 0;
  int siguiente = -1;

  const char *p = cruda != nullptr ? cruda : "";
  Linea l;
  while (siguienteLinea(p, l)) {
    quitarBuscando(l);
    if (l.largo == 0 || esEco(l, normal)) {
      continue;
    }

    TipoRespuesta tipo = TipoRespuesta::ERROR_BUS;
    bool esError;
    if (empiezaCon(l, "BUS INIT")) {
      // "BUS INIT: ...OK" no aporta nada
      esError = contiene(l, "ERROR");
      if (!esError) {
        continue;
      }
    } else {
      esError = buscarError(l, tipo);
    }
    if (esError) {
      if (!hayError) {
        hayError = true;
        error = tipo;
      }
      continue;
    }

    hayTexto = true;
    if (esAt) {
      continue;
    }

    size_t cantidad;
    int indice;
    Linea resto;
    if (esLineaCantidad(l, cantidad)) {
      esperados = cantidad;
      recibidos = 0;
      siguiente = (cantidad > 0 && cantidad <= sizeof(mensaje)) ? 0 : -1;
      continue;
    }
    if (esLineaFrame(l, indice, resto)) {
      uint8_t bytes[8];
      size_t n;
      if (siguiente < 0 || indice != (siguiente & 0x0F) ||
          !leerBytes(resto, bytes, sizeof(bytes), n)) {
        // Fuera de orden o mal formado: el mensaje queda incompleto
        siguiente = -1;
        continue;
      }
      siguiente++;
      for (size_t i = 0; i < n && recibidos < esperados; i++) {
        mensaje[recibidos++] = bytes[i];
      }
      if (recibidos == esperados) {
        if (encabezadoValido(mensaje, recibidos, pedido)) {
          registrar(r, mensaje, recibidos, pedido);
        }
        siguiente = -1;
      }
      continue;
    }

    uint8_t bytes[MAX_DATOS + 2];
    size_t n;
    if (leerBytes(l, bytes, sizeof(bytes), n) && encabezadoValido(bytes, n, pedido)) {
      registrar(r, bytes, n, pedido);
    }
  }

  if (hayError) {
    r.tipo = error;
    r.largo = 0;
    r.respuestas = 0;
  } else if (esAt) {
    r.tipo = hayTexto ? TipoRespuesta::TEXTO : TipoRespuesta::VACIA;
  } else if (r.respuestas > 0) {
    r.tipo = TipoRespuesta::DATOS;
  } else {
    r.tipo = hayTexto ? TipoRespuesta::INVALIDA : TipoRespuesta::VACIA;
  }
  return r;
}

bool textoRespuesta(const char *cruda, const char *comando, char *salida, size_t tam) {
  if (salida == nullptr || tam == 0) {
    return false;
  }
  salida[0] = '\0';

  char normal[16] = "";
  if (!normalizarComando(comando, normal, sizeof(normal))) {
    normal[0] = '\0';
  }

  const char *p = cruda != nullptr ? cruda : "";
  Linea l;
  while (siguienteLinea(p, l)) {
    quitarBuscando(l);
    if (l.largo == 0 || esEco(l, normal)) {
      continue;
    }
    size_t n = l.largo < tam - 1 ? l.largo : tam - 1;
    memcpy(salida, l.texto, n);
    salida[n] = '\0';
    return true;
  }
  return false;
}

// "12.4V": dígitos, punto y decimales opcionales, y una V al final.
static bool leerVoltaje(const Linea &l, float &voltios) {
  size_t i = 0;
  float entero = 0;
  while (i < l.largo && l.texto[i] >= '0' && l.texto[i] <= '9') {
    entero = entero * 10 + (l.texto[i] - '0');
    i++;
  }
  if (i == 0) {
    return false;
  }
  float fraccion = 0;
  float escala = 1;
  if (i < l.largo && l.texto[i] == '.') {
    i++;
    size_t inicio = i;
    while (i < l.largo && l.texto[i] >= '0' && l.texto[i] <= '9') {
      fraccion = fraccion * 10 + (l.texto[i] - '0');
      escala *= 10;
      i++;
    }
    if (i == inicio) {
      return false;
    }
  }
  if (i + 1 != l.largo || mayuscula(l.texto[i]) != 'V') {
    return false;
  }
  voltios = entero + fraccion / escala;
  return true;
}

bool interpretarVoltaje(const char *cruda, float *voltios) {
  if (cruda == nullptr || voltios == nullptr) {
    return false;
  }
  const char *p = cruda;
  Linea l;
  while (siguienteLinea(p, l)) {
    if (leerVoltaje(l, *voltios)) {
      return true;
    }
  }
  return false;
}

const char *nombreProtocolo(const char *dpn) {
  // Del 1 al C, según ATSPn en el datasheet. Hasta 14 caracteres (pantalla).
  static const char *const NOMBRES[] = {"J1850 PWM",    "J1850 VPW",     "ISO 9141-2",
                                        "KWP 5 baud",   "KWP rapido",    "CAN 11b 500k",
                                        "CAN 29b 500k", "CAN 11b 250k",  "CAN 29b 250k",
                                        "J1939 250k",   "CAN usuario 1", "CAN usuario 2"};
  if (dpn == nullptr) {
    return nullptr;
  }
  // "A6": automático, eligió el 6. Una "A" sola es el protocolo A (J1939).
  if (dpn[0] == 'A' && dpn[1] != '\0') {
    dpn++;
  }
  if (dpn[0] == '\0' || dpn[1] != '\0') {
    return nullptr;
  }
  char c = dpn[0];
  if (c >= '1' && c <= '9') {
    return NOMBRES[c - '1'];
  }
  if (c >= 'A' && c <= 'C') {
    return NOMBRES[9 + (c - 'A')];
  }
  return nullptr;
}

const char *nombreTipo(TipoRespuesta tipo) {
  switch (tipo) {
  case TipoRespuesta::DATOS:
    return "DATOS";
  case TipoRespuesta::TEXTO:
    return "TEXTO";
  case TipoRespuesta::NO_DATA:
    return "NO_DATA";
  case TipoRespuesta::NO_SOPORTADO:
    return "NO_SOPORTADO";
  case TipoRespuesta::SIN_ECU:
    return "SIN_ECU";
  case TipoRespuesta::ERROR_BUS:
    return "ERROR_BUS";
  case TipoRespuesta::BUFFER_LLENO:
    return "BUFFER_LLENO";
  case TipoRespuesta::DETENIDO:
    return "DETENIDO";
  case TipoRespuesta::AVISO_ELM:
    return "AVISO_ELM";
  case TipoRespuesta::VACIA:
    return "VACIA";
  case TipoRespuesta::INVALIDA:
    return "INVALIDA";
  }
  return "?";
}
