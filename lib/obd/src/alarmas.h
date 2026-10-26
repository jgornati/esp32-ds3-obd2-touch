#pragma once

#include <stddef.h>
#include <stdint.h>

// Las alarmas (E11): un valor fuera de lo normal un rato seguido, con
// histéresis para que no se prenda y se apague con cada lectura. Sin
// Arduino: los momentos son los de millis().

struct ReglaAlarma {
  float umbral;         // se prende con el valor en el umbral o más allá
  float normal;         // se apaga con el valor en este o más acá (la histéresis)
  bool porArriba;       // true: más alto es peor (refrigerante); false: más bajo
  uint32_t paraPrender; // ms entre la primera lectura más allá del umbral y la que la prende
  uint32_t paraApagar;  // lo mismo, del lado normal, para apagarla
};

enum class CambioAlarma : uint8_t { NINGUNO, SE_PRENDIO, SE_APAGO };

class Alarma {
public:
  explicit Alarma(const ReglaAlarma &definida) : regla(definida) {}

  // hay: si hay un valor actual (no vencido); momento: el de esa lectura. Un
  // NO DATA suelto no la cambia (el valor anterior sigue vigente), y un dato
  // suelto tampoco: el tiempo se cuenta entre lecturas, así que con
  // paraPrender > 0 hacen falta dos. Sin valor actual se apaga: no se muestra
  // un valor viejo.
  CambioAlarma revisar(bool hay, float valor, uint32_t momento);
  bool activa() const {
    return prendida;
  }
  // El último valor revisado
  float valor() const {
    return ultimo;
  }

private:
  ReglaAlarma regla;
  bool prendida = false;
  bool contando = false; // hay lecturas del otro lado desde...
  uint32_t desde = 0;
  float ultimo = 0;
};

// Las del DS3, en el orden en que se muestran si hay varias
enum TipoAlarma : uint8_t { REFRIGERANTE_ALTO, VOLTAJE_BAJO, VOLTAJE_ALTO, RPM_ALTA, CANT_ALARMAS };

class Alarmas {
public:
  explicit Alarmas(const ReglaAlarma (&reglas)[CANT_ALARMAS]);

  CambioAlarma revisar(TipoAlarma tipo, bool hay, float valor, uint32_t momento);
  const Alarma &alarma(TipoAlarma tipo) const {
    return lista[tipo];
  }
  uint8_t activas() const;
  // La que se muestra: con varias, se alternan (una por vuelta). numero va
  // de 0 a activas() - 1. false si no hay ninguna.
  bool aLaVista(uint32_t vuelta, TipoAlarma *tipo, uint8_t *numero) const;

private:
  Alarma lista[CANT_ALARMAS];
};

// Para la barra: "REFRIGERANTE 106 °C", "VOLTAJE BAJO 12.6 V" (°C en UTF-8)
void textoAlarma(TipoAlarma tipo, float valor, char *salida, size_t tam);
