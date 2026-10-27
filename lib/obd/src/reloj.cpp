#include "reloj.h"

#include <stdio.h>

// Bit 7 de los segundos: el oscilador se detuvo (Oscillator Stop)
static const uint8_t PARADO = 0x80;

static bool deBcd(uint8_t bcd, uint8_t *valor) {
  uint8_t decenas = bcd >> 4;
  uint8_t unidades = bcd & 0x0F;
  if (decenas > 9 || unidades > 9) {
    return false;
  }
  *valor = decenas * 10 + unidades;
  return true;
}

static uint8_t aBcd(uint8_t valor) {
  return (uint8_t)((valor / 10) << 4 | valor % 10);
}

static bool bisiesto(uint16_t anio) {
  return (anio % 4 == 0 && anio % 100 != 0) || anio % 400 == 0;
}

static uint8_t diasDelMes(uint16_t anio, uint8_t mes) {
  static const uint8_t DIAS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return mes == 2 && bisiesto(anio) ? 29 : DIAS[mes - 1];
}

bool fechaValida(const FechaHora &f) {
  return f.anio >= 2000 && f.anio <= 2099 && f.mes >= 1 && f.mes <= 12 && f.dia >= 1 &&
         f.dia <= diasDelMes(f.anio, f.mes) && f.hora < 24 && f.minuto < 60 && f.segundo < 60;
}

// En 24 h (el firmware lo deja así): la hora usa los bits 0 a 5
bool leerRegistrosHora(const uint8_t registros[REGISTROS_HORA], FechaHora *f) {
  if (registros[0] & PARADO) {
    return false;
  }
  uint8_t segundo, minuto, hora, dia, mes, anio;
  if (!deBcd(registros[0] & 0x7F, &segundo) || !deBcd(registros[1] & 0x7F, &minuto) ||
      !deBcd(registros[2] & 0x3F, &hora) || !deBcd(registros[3] & 0x3F, &dia) ||
      !deBcd(registros[5] & 0x1F, &mes) || !deBcd(registros[6], &anio)) {
    return false;
  }
  FechaHora leida = {(uint16_t)(2000 + anio), mes, dia, hora, minuto, segundo};
  if (!fechaValida(leida)) {
    return false;
  }
  *f = leida;
  return true;
}

void registrosHora(const FechaHora &f, uint8_t registros[REGISTROS_HORA]) {
  registros[0] = aBcd(f.segundo);
  registros[1] = aBcd(f.minuto);
  registros[2] = aBcd(f.hora);
  registros[3] = aBcd(f.dia);
  registros[4] = diaDeLaSemana(f.anio, f.mes, f.dia);
  registros[5] = aBcd(f.mes);
  registros[6] = aBcd(f.anio % 100);
}

bool interpretarHora(const char *texto, FechaHora *f) {
  int anio, mes, dia, hora, minuto;
  int segundo = 0;
  int leidos = 0;
  if (sscanf(texto, "%d-%d-%d %d:%d%n", &anio, &mes, &dia, &hora, &minuto, &leidos) != 5) {
    return false;
  }
  const char *resto = texto + leidos;
  if (*resto == ':') {
    int mas = 0;
    if (sscanf(resto, ":%d%n", &segundo, &mas) != 1) {
      return false;
    }
    resto += mas;
  }
  while (*resto == ' ') {
    resto++;
  }
  if (*resto != '\0' || anio < 2000 || anio > 2099 || mes < 1 || mes > 12 || dia < 1 || dia > 31 ||
      hora < 0 || hora > 23 || minuto < 0 || minuto > 59 || segundo < 0 || segundo > 59) {
    return false;
  }
  FechaHora leida = {(uint16_t)anio, (uint8_t)mes,    (uint8_t)dia,
                     (uint8_t)hora,  (uint8_t)minuto, (uint8_t)segundo};
  if (!fechaValida(leida)) {
    return false;
  }
  *f = leida;
  return true;
}

// Sakamoto
uint8_t diaDeLaSemana(uint16_t anio, uint8_t mes, uint8_t dia) {
  static const uint8_t DESDE_ENERO[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  uint16_t a = mes < 3 ? anio - 1 : anio;
  return (a + a / 4 - a / 100 + a / 400 + DESDE_ENERO[mes - 1] + dia) % 7;
}

// Los días desde 1970 de una fecha, y al revés (los algoritmos de Howard
// Hinnant, con años que empiezan en marzo)
static int64_t diasDesde1970(int32_t anio, uint32_t mes, uint32_t dia) {
  anio -= mes <= 2;
  int64_t era = (anio >= 0 ? anio : anio - 399) / 400;
  uint32_t anioDeEra = (uint32_t)(anio - era * 400);
  uint32_t diaDelAnio = (153 * (mes > 2 ? mes - 3 : mes + 9) + 2) / 5 + dia - 1;
  uint32_t diaDeEra = anioDeEra * 365 + anioDeEra / 4 - anioDeEra / 100 + diaDelAnio;
  return era * 146097 + diaDeEra - 719468;
}

int64_t segundosDesde1970(const FechaHora &f) {
  return diasDesde1970(f.anio, f.mes, f.dia) * 86400 + f.hora * 3600 + f.minuto * 60 + f.segundo;
}

FechaHora fechaHoraDe(int64_t segundos) {
  int64_t dias = segundos / 86400;
  int64_t resto = segundos % 86400;
  if (resto < 0) {
    resto += 86400;
    dias--;
  }
  int64_t z = dias + 719468;
  int64_t era = (z >= 0 ? z : z - 146096) / 146097;
  uint32_t diaDeEra = (uint32_t)(z - era * 146097);
  uint32_t anioDeEra = (diaDeEra - diaDeEra / 1460 + diaDeEra / 36524 - diaDeEra / 146096) / 365;
  uint32_t diaDelAnio = diaDeEra - (365 * anioDeEra + anioDeEra / 4 - anioDeEra / 100);
  uint32_t mesDesdeMarzo = (5 * diaDelAnio + 2) / 153;
  uint8_t mes = mesDesdeMarzo < 10 ? mesDesdeMarzo + 3 : mesDesdeMarzo - 9;
  FechaHora f;
  f.anio = (uint16_t)(anioDeEra + era * 400 + (mes <= 2));
  f.mes = mes;
  f.dia = (uint8_t)(diaDelAnio - (153 * mesDesdeMarzo + 2) / 5 + 1);
  f.hora = (uint8_t)(resto / 3600);
  f.minuto = (uint8_t)(resto % 3600 / 60);
  f.segundo = (uint8_t)(resto % 60);
  return f;
}

void textoHora(const FechaHora &f, char *salida, size_t tam) {
  snprintf(salida, tam, "%02u:%02u", (unsigned)f.hora, (unsigned)f.minuto);
}

void textoFechaHora(const FechaHora &f, char *salida, size_t tam) {
  snprintf(salida, tam, "%04u-%02u-%02u %02u:%02u:%02u", (unsigned)f.anio, (unsigned)f.mes,
           (unsigned)f.dia, (unsigned)f.hora, (unsigned)f.minuto, (unsigned)f.segundo);
}
