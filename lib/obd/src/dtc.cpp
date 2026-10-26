#include "dtc.h"

#include <stdio.h>
#include <string.h>

#include "comandos.h"

void textoDtc(uint16_t codigo, char *salida) {
  static const char LETRAS[] = "PCBU";
  static const char DIGITOS[] = "0123456789ABCDEF";
  salida[0] = LETRAS[codigo >> 14];
  salida[1] = DIGITOS[(codigo >> 12) & 0x3];
  salida[2] = DIGITOS[(codigo >> 8) & 0xF];
  salida[3] = DIGITOS[(codigo >> 4) & 0xF];
  salida[4] = DIGITOS[codigo & 0xF];
  salida[5] = '\0';
}

struct Descripcion {
  uint16_t desde;
  uint16_t hasta;
  const char *texto; // hasta 14 caracteres
};

// Códigos P0/P2 genéricos (SAE J2012) comunes en un motor nafta. Los rangos
// juntan circuito, rango, señal baja y alta del mismo componente.
static const Descripcion DESCRIPCIONES[] = {
    {0x0010, 0x0014, "Variador levas"}, {0x0016, 0x0016, "Distribucion"},
    {0x0030, 0x0032, "Calef. sonda 1"}, {0x0036, 0x0038, "Calef. sonda 2"},
    {0x0100, 0x0103, "Caudalimetro"},   {0x0105, 0x0108, "Sensor MAP"},
    {0x0110, 0x0113, "Sensor T. aire"}, {0x0115, 0x0118, "Sensor refrig."},
    {0x0120, 0x0123, "Sens. mariposa"}, {0x0125, 0x0125, "Calienta lento"},
    {0x0128, 0x0128, "Termostato"},     {0x0130, 0x0134, "Sonda lambda 1"},
    {0x0135, 0x0135, "Calef. sonda 1"}, {0x0136, 0x0140, "Sonda lambda 2"},
    {0x0141, 0x0141, "Calef. sonda 2"}, {0x0171, 0x0171, "Mezcla pobre"},
    {0x0172, 0x0172, "Mezcla rica"},    {0x0201, 0x0201, "Inyector 1"},
    {0x0202, 0x0202, "Inyector 2"},     {0x0203, 0x0203, "Inyector 3"},
    {0x0204, 0x0204, "Inyector 4"},     {0x0217, 0x0217, "Motor caliente"},
    {0x0230, 0x0230, "Bomba nafta"},    {0x0300, 0x0300, "Falla encend."},
    {0x0301, 0x0301, "Falla cil. 1"},   {0x0302, 0x0302, "Falla cil. 2"},
    {0x0303, 0x0303, "Falla cil. 3"},   {0x0304, 0x0304, "Falla cil. 4"},
    {0x0325, 0x0334, "Sens. detonac."}, {0x0335, 0x0339, "Sens. ciguenal"},
    {0x0340, 0x0344, "Sensor levas"},   {0x0351, 0x0351, "Bobina 1"},
    {0x0352, 0x0352, "Bobina 2"},       {0x0353, 0x0353, "Bobina 3"},
    {0x0354, 0x0354, "Bobina 4"},       {0x0420, 0x0421, "Catalizador"},
    {0x0440, 0x0457, "Sist. canister"}, {0x0480, 0x0480, "Electrovent."},
    {0x0500, 0x0503, "Sensor veloc."},  {0x0505, 0x0505, "Ralenti"},
    {0x0506, 0x0506, "Ralenti bajo"},   {0x0507, 0x0507, "Ralenti alto"},
    {0x0560, 0x0561, "Tension bat."},   {0x0562, 0x0562, "Bateria baja"},
    {0x0563, 0x0563, "Bateria alta"},   {0x0600, 0x0606, "Falla ECU"},
    {0x0627, 0x0627, "Bomba nafta"},    {0x0638, 0x0638, "Motor mariposa"},
    {0x0700, 0x0700, "Caja automat."},  {0x2100, 0x2105, "Motor mariposa"},
    {0x2135, 0x2135, "Sens. mariposa"}, {0x2138, 0x2138, "Pedal acelerad"},
    {0x2177, 0x2177, "Mezcla pobre"},   {0x2178, 0x2178, "Mezcla rica"},
    {0x2187, 0x2187, "Mezcla pobre"},   {0x2188, 0x2188, "Mezcla rica"},
    {0x2195, 0x2196, "Sonda lambda 1"}, {0x2270, 0x2271, "Sonda lambda 2"},
};

const char *descripcionDtc(uint16_t codigo) {
  for (const Descripcion &d : DESCRIPCIONES) {
    if (codigo >= d.desde && codigo <= d.hasta) {
      return d.texto;
    }
  }
  return nullptr;
}

bool interpretarDtcs(const uint8_t *datos, uint8_t largo, ListaDtc *lista) {
  if (datos == nullptr || lista == nullptr || largo < 1) {
    return false;
  }
  uint8_t cantidad = datos[0];
  if (cantidad > MAX_DTC || largo < 1 + 2 * cantidad) {
    return false;
  }
  lista->cantidad = cantidad;
  for (uint8_t i = 0; i < cantidad; i++) {
    lista->codigos[i] = (uint16_t)(datos[1 + 2 * i] << 8 | datos[2 + 2 * i]);
  }
  return true;
}

Codigos interpretarCodigos(const char *cruda, const char *comando, bool completa) {
  Codigos codigos;
  Respuesta r = interpretar(cruda, comando);
  codigos.tipo = r.tipo;
  if (completa && r.tipo == TipoRespuesta::DATOS &&
      interpretarDtcs(r.datos, r.largo, &codigos.lista)) {
    codigos.estado = EstadoCodigos::LEIDOS;
    return codigos;
  }
  char normal[8];
  bool permanentes =
      normalizarComando(comando, normal, sizeof(normal)) && strcmp(normal, "0A") == 0;
  codigos.estado = completa && permanentes && r.tipo == TipoRespuesta::NO_DATA
                       ? EstadoCodigos::NO_DISPONIBLE
                       : EstadoCodigos::ERROR;
  return codigos;
}

static bool sinCodigos(const Codigos &codigos) {
  return codigos.estado == EstadoCodigos::LEIDOS && codigos.lista.cantidad == 0;
}

bool sinFallas(const LecturaFallas &lectura) {
  bool permanentes =
      lectura.permanentes.estado == EstadoCodigos::LEIDOS && lectura.permanentes.lista.cantidad > 0;
  return sinCodigos(lectura.guardados) && sinCodigos(lectura.pendientes) && !permanentes;
}

bool errorAlLeer(const LecturaFallas &lectura) {
  return lectura.guardados.estado != EstadoCodigos::LEIDOS ||
         lectura.pendientes.estado != EstadoCodigos::LEIDOS;
}

const char *textoLuz(const LecturaFallas &lectura) {
  if (!lectura.leida) {
    return "...";
  }
  if (!lectura.luzLeida) {
    return "--";
  }
  return lectura.luz ? "Encendida" : "Apagada";
}

// Agrega los códigos de la lista que todavía no están
static void agregarTipo(const Codigos &codigos, TipoDtc tipo, RenglonDtc *todos,
                        uint8_t &cantidad) {
  if (codigos.estado != EstadoCodigos::LEIDOS) {
    return;
  }
  for (uint8_t i = 0; i < codigos.lista.cantidad; i++) {
    uint16_t codigo = codigos.lista.codigos[i];
    bool repetido = false;
    for (uint8_t j = 0; j < cantidad && !repetido; j++) {
      repetido = todos[j].codigo == codigo;
    }
    if (!repetido) {
      todos[cantidad++] = {codigo, tipo};
    }
  }
}

PantallaFallas pantallaFallas(const LecturaFallas &lectura, uint32_t vuelta) {
  PantallaFallas p = {};
  if (!lectura.leida) {
    p.vista = VistaFallas::LEYENDO;
    return p;
  }
  // Con la luz encendida no es "Sin fallas", aunque no haya códigos
  bool luzEncendida = lectura.luzLeida && lectura.luz;
  p.vista = sinFallas(lectura) && !luzEncendida ? VistaFallas::SIN_FALLAS : VistaFallas::CODIGOS;

  RenglonDtc todos[3 * MAX_DTC];
  uint8_t cantidad = 0;
  agregarTipo(lectura.guardados, TipoDtc::GUARDADO, todos, cantidad);
  agregarTipo(lectura.permanentes, TipoDtc::PERMANENTE, todos, cantidad);
  agregarTipo(lectura.pendientes, TipoDtc::PENDIENTE, todos, cantidad);
  if (cantidad == 0) {
    return p;
  }
  p.grupos = (cantidad + RENGLONES_DTC - 1) / RENGLONES_DTC;
  p.grupo = vuelta % p.grupos;
  uint8_t desde = p.grupo * RENGLONES_DTC;
  while (p.cantidad < RENGLONES_DTC && desde + p.cantidad < cantidad) {
    p.renglones[p.cantidad] = todos[desde + p.cantidad];
    p.cantidad++;
  }
  return p;
}

bool textoCantidad(const Codigos &codigos, TipoDtc tipo, char *salida, size_t tam) {
  static const char *const NOMBRES[][3] = {{"guardada", "guardadas", "Guardadas"},
                                           {"permanente", "permanentes", "Permanentes"},
                                           {"pendiente", "pendientes", "Pendientes"}};
  const char *const *nombre = NOMBRES[(uint8_t)tipo];
  if (codigos.estado != EstadoCodigos::LEIDOS) {
    snprintf(salida, tam, "%s: error", nombre[2]);
    return false;
  }
  uint8_t cantidad = codigos.lista.cantidad;
  snprintf(salida, tam, "%u %s", cantidad, cantidad == 1 ? nombre[0] : nombre[1]);
  return true;
}
