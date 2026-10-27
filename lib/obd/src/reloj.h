#pragma once

#include <stddef.h>
#include <stdint.h>

// El reloj (E14): la hora del RTC PCF85063, en la hora local (sin zona
// horaria) y en 24 h. Acá lo que no depende del chip: sus registros, el
// texto de !hora y los textos que se muestran.

struct FechaHora {
  uint16_t anio; // 2000 a 2099: el RTC guarda solo las dos últimas cifras
  uint8_t mes;   // 1 a 12
  uint8_t dia;   // 1 a 31
  uint8_t hora;  // 0 a 23
  uint8_t minuto;
  uint8_t segundo;
};

// Los 7 registros de la hora, de 0x04 (segundos) a 0x0A (año), en BCD
constexpr uint8_t REGISTROS_HORA = 7;

// false si el oscilador se detuvo desde que se puso en hora (el bit OS de los
// segundos: por ejemplo, se quedó sin batería) o si los registros no son una
// fecha válida
bool leerRegistrosHora(const uint8_t registros[REGISTROS_HORA], FechaHora *f);
// Los registros para escribir f, con el día de la semana y el bit OS en 0
void registrosHora(const FechaHora &f, uint8_t registros[REGISTROS_HORA]);

// "2026-10-27 14:35" o "2026-10-27 14:35:12", de !hora. false si no es una
// fecha y hora válidas, entre 2000 y 2099.
bool interpretarHora(const char *texto, FechaHora *f);
bool fechaValida(const FechaHora &f);

// 0 domingo a 6 sábado
uint8_t diaDeLaSemana(uint16_t anio, uint8_t mes, uint8_t dia);

// Segundos desde 1970-01-01 00:00, como si la hora local fuera UTC: para la
// hora del sistema (time()), de la que la FAT toma la fecha de los archivos
int64_t segundosDesde1970(const FechaHora &f);
FechaHora fechaHoraDe(int64_t segundos);

// "14:35"
void textoHora(const FechaHora &f, char *salida, size_t tam);
// "2026-10-27 14:35:12"
void textoFechaHora(const FechaHora &f, char *salida, size_t tam);
