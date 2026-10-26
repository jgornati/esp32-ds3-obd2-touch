#include "alarmas.h"

#include <stdio.h>

CambioAlarma Alarma::revisar(bool hay, float valor, uint32_t momento) {
  if (!hay) {
    contando = false;
    if (prendida) {
      prendida = false;
      return CambioAlarma::SE_APAGO;
    }
    return CambioAlarma::NINGUNO;
  }
  ultimo = valor;
  bool masAlla = regla.porArriba ? valor >= regla.umbral : valor <= regla.umbral;
  bool normal = regla.porArriba ? valor <= regla.normal : valor >= regla.normal;
  // Lo que la cambiaría: prendida, volver a lo normal; apagada, pasar el umbral
  if (prendida ? !normal : !masAlla) {
    contando = false;
    return CambioAlarma::NINGUNO;
  }
  if (!contando) {
    contando = true;
    desde = momento;
  }
  if (momento - desde < (prendida ? regla.paraApagar : regla.paraPrender)) {
    return CambioAlarma::NINGUNO;
  }
  contando = false;
  prendida = !prendida;
  return prendida ? CambioAlarma::SE_PRENDIO : CambioAlarma::SE_APAGO;
}

Alarmas::Alarmas(const ReglaAlarma (&reglas)[CANT_ALARMAS])
    : lista{Alarma(reglas[REFRIGERANTE_ALTO]), Alarma(reglas[VOLTAJE_BAJO]),
            Alarma(reglas[VOLTAJE_ALTO]), Alarma(reglas[RPM_ALTA])} {
  static_assert(CANT_ALARMAS == 4, "una Alarma por tipo, en el orden de TipoAlarma");
}

CambioAlarma Alarmas::revisar(TipoAlarma tipo, bool hay, float valor, uint32_t momento) {
  return lista[tipo].revisar(hay, valor, momento);
}

uint8_t Alarmas::activas() const {
  uint8_t cantidad = 0;
  for (const Alarma &a : lista) {
    cantidad += a.activa() ? 1 : 0;
  }
  return cantidad;
}

bool Alarmas::aLaVista(uint32_t vuelta, TipoAlarma *tipo, uint8_t *numero) const {
  uint8_t cantidad = activas();
  if (cantidad == 0) {
    return false;
  }
  uint8_t buscada = vuelta % cantidad;
  uint8_t vista = 0;
  for (uint8_t i = 0; i < CANT_ALARMAS; i++) {
    if (!lista[i].activa()) {
      continue;
    }
    if (vista == buscada) {
      *tipo = (TipoAlarma)i;
      *numero = vista;
      return true;
    }
    vista++;
  }
  return false;
}

void textoAlarma(TipoAlarma tipo, float valor, char *salida, size_t tam) {
  switch (tipo) {
  case REFRIGERANTE_ALTO:
    snprintf(salida, tam, "REFRIGERANTE %.0f °C", valor);
    break;
  case VOLTAJE_BAJO:
    snprintf(salida, tam, "VOLTAJE BAJO %.1f V", valor);
    break;
  case VOLTAJE_ALTO:
    snprintf(salida, tam, "VOLTAJE ALTO %.1f V", valor);
    break;
  case RPM_ALTA:
    snprintf(salida, tam, "ZONA ROJA %.0f RPM", valor);
    break;
  default:
    snprintf(salida, tam, "ALARMA");
    break;
  }
}
