#pragma once

#include <stddef.h>
#include <stdint.h>

#include "viaje.h"

// La fuerza G (E12): la aceleración del auto, lateral y longitudinal, con
// el acelerómetro de la placa. "Arriba" sale de la gravedad con la placa
// quieta; "derecha", del eje del sensor que va a lo largo de la pantalla,
// llevado a la horizontal; y "adelante", de los dos. Así la inclinación de
// la pantalla no importa. Los momentos son los de millis().

// En g, en los ejes del sensor
struct Vector {
  float x, y, z;
};

struct ReglasFuerzaG {
  Vector derecha;         // el eje del sensor que apunta a la derecha del auto
  uint32_t filtroMs;      // constante de tiempo del filtro de las vibraciones
  float toleranciaQuieta; // g: lo que se puede apartar cada muestra del promedio estando quieta
  uint32_t quietaMs;      // quieta este rato, calibra
  uint32_t vigenciaMs;    // sin muestras por más que esto, no hay valor actual
};

// Lo que se muestra. Es una copia: pasa de la tarea del IMU a loop().
struct LecturaG {
  bool calibrada = false;
  bool actual = false;    // calibrada y con una muestra de hace menos de vigenciaMs
  float lateral = 0;      // g: + el auto acelera hacia la derecha (curva a la derecha)
  float longitudinal = 0; // g: + acelerando, - frenando
  // Los máximos desde reiniciarMaximos(), en g y positivos
  Extremo frenada, aceleracion, curvaIzquierda, curvaDerecha;
};

class FuerzaG {
public:
  explicit FuerzaG(const ReglasFuerzaG &definidas) : reglas(definidas) {}

  // Una muestra del acelerómetro. Calibra con la placa quieta quietaMs: la
  // primera vez alcanza con eso (al poner el contacto el auto está
  // estacionado); después, además, parado (la ECU da 0 km/h) todo ese rato.
  void medir(const Vector &a, uint32_t momento, bool parado);
  LecturaG lectura(uint32_t ahora) const;
  void reiniciarMaximos();
  uint32_t calibraciones() const {
    return cantidadCalibraciones;
  }
  // La última muestra, sin filtrar. false si no hubo ninguna.
  bool cruda(Vector *a) const;

private:
  void revisarQuieta(const Vector &a, uint32_t momento, bool seguida, bool parado);
  void calibrar(const Vector &promedio);

  ReglasFuerzaG reglas;
  bool hayMuestra = false;
  uint32_t ultima = 0;
  Vector ultimaCruda = {0, 0, 0};
  Vector filtrada = {0, 0, 0};
  // La placa quieta: desde cuándo y la suma de las muestras
  uint32_t muestrasQuieta = 0;
  uint32_t quietaDesde = 0;
  Vector sumaQuieta = {0, 0, 0};
  bool calibrada = false;
  uint32_t cantidadCalibraciones = 0;
  Vector ejeDerecha = {0, 0, 0};
  Vector ejeAdelante = {0, 0, 0};
  float lateral = 0;
  float longitudinal = 0;
  Extremo maxFrenada, maxAceleracion, maxIzquierda, maxDerecha;
};

// El valor sin el signo, con dos decimales: "0.41"
void textoFuerzaG(float g, char *salida, size_t tam);
// El nombre de cada una según el lado: "Frenada" o "Acel.", y "Curva der" o
// "Curva izq". Con 0.00 a la vista, sin lado: "Long." y "Lateral".
const char *nombreLongitudinal(float g);
const char *nombreLateral(float g);
