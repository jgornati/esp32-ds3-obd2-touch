#pragma once

#include <stddef.h>
#include <stdint.h>

// El aviso de cambio (E25): al llegar las RPM al umbral, un aviso corto (la
// pantalla parpadea y suena un pitido), una vez por marcha. Vuelve a avisar
// cuando las RPM bajan rearme del umbral, que es lo que pasa al poner la
// siguiente. No avisa sin marcha (punto muerto, embrague apretado: así no
// suena acelerando parado) ni en la última. Sin Arduino: los momentos son los
// de millis().

struct ReglasAvisoCambio {
  float rearme;        // rpm debajo del umbral para volver a avisar
  uint32_t duracionMs; // lo que dura el aviso como mucho
  uint8_t marchas;     // cuántas hay: en la última no avisa
  // Los umbrales que se pueden elegir, en rpm: de minimo a maximo, de a paso
  uint16_t minimo;
  uint16_t maximo;
  uint16_t paso;
};

class AvisoCambio {
public:
  explicit AvisoCambio(const ReglasAvisoCambio &definidas) : reglas(definidas) {}

  // En rpm; 0 lo apaga. Termina el aviso que hubiera y queda listo para avisar.
  void ponerUmbral(uint16_t rpm);
  uint16_t umbral() const {
    return umbralRpm;
  }
  // hay: si la RPM tiene un valor actual; marcha: la que se muestra (0 sin
  // marcha o sin un valor actual); momento: el de la lectura. true si con
  // esta lectura empieza un aviso (para el pitido). Revisar otra vez la misma
  // lectura no cambia nada. Sin un valor actual el aviso termina, pero no
  // vuelve a avisar hasta que las RPM bajen.
  bool revisar(bool hay, float rpm, uint8_t marcha, uint32_t momento);
  // Hay un aviso: empezó hace menos de duracionMs y desde entonces las RPM
  // siguen en el umbral o más, con una marcha en la que avisa
  bool activo(uint32_t ahora) const;

private:
  ReglasAvisoCambio reglas;
  uint16_t umbralRpm = 0;
  bool listo = true;    // puede avisar: las RPM bajaron desde el último
  bool enCurso = false; // el último aviso, mientras sigan las condiciones
  uint32_t inicio = 0;
};

// Para - y + de Info: el umbral siguiente (paso 1) o el anterior (-1), de
// minimo a maximo de a paso; debajo del mínimo, apagado (0). En los extremos
// queda donde está.
uint16_t otroUmbralCambio(const ReglasAvisoCambio &reglas, uint16_t actual, int8_t paso);
// 0 (apagado), o de minimo a maximo de a paso
bool umbralCambioValido(const ReglasAvisoCambio &reglas, uint16_t rpm);
// Para Info y la consola: "6000 rpm", o "Apagado"
void textoUmbralCambio(uint16_t rpm, char *salida, size_t tam);
