#pragma once

#include <stdint.h>

// La marcha (E24), calculada: la ECU no la da. Sale de la velocidad sobre las
// RPM, en km/h por 1000 rpm, que en cada marcha es fija mientras el embrague
// no patina. Con el embrague apretado, en punto muerto o cambiando, no
// coincide con ninguna: sin marcha. Los momentos son los de millis().

struct ReglasMarcha {
  const float *relaciones; // km/h por 1000 rpm de cada marcha, desde 1ª
  uint8_t cantidad;
  float tolerancia;        // de 2ª en adelante, lo que se puede apartar (0.05 = 5 %)
  float toleranciaPrimera; // 1ª, más: a poca velocidad pesan los km/h enteros
  float velocidadMinima;   // km/h; debajo, sin marcha
  uint8_t lecturasIguales; // seguidas, para cambiar la que se muestra
  uint32_t vigenciaMs;     // sin lecturas más que esto, vencida
};

// La marcha de una lectura, sin esperar otras: de 1 a cantidad, o 0 sin marcha
uint8_t marchaDe(const ReglasMarcha &reglas, float rpm, float kmh);

class Marcha {
public:
  explicit Marcha(const ReglasMarcha &definidas) : reglas(definidas) {}

  // La RPM y la velocidad de una misma consulta. Después de un hueco de más
  // de vigenciaMs empieza de cero, sin marcha.
  void lectura(float rpm, float kmh, uint32_t ahora);
  // Tuvo alguna lectura, y la última es de hace vigenciaMs o menos
  bool actual(uint32_t ahora) const;
  // La que se muestra: cambia cuando una marcha (o ninguna) se repite en
  // lecturasIguales lecturas seguidas, así no salta en el medio de un cambio.
  // 0 sin marcha.
  uint8_t numero() const {
    return mostrada;
  }

private:
  ReglasMarcha reglas;
  bool hay = false;
  uint32_t ultima = 0;
  uint8_t mostrada = 0;
  uint8_t candidata = 0;
  uint8_t iguales = 0;
};
