#include "fuerza_g.h"

#include <math.h>
#include <stdio.h>

// Quieta, el sensor mide 1 g: lejos de eso, la escala o el sensor andan mal y no calibra
static const float TOLERANCIA_GRAVEDAD = 0.2f;
// El eje de la derecha tiene que quedar al menos a 30° de la vertical
// (sen 30° = 0,5): si no, la placa no está montada como dicen las reglas
static const float MINIMO_HORIZONTAL = 0.5f;
// Por debajo de esto el texto dice 0.00
static const float CASI_CERO = 0.005f;

static Vector mas(const Vector &a, const Vector &b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

static Vector menos(const Vector &a, const Vector &b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}

static Vector por(const Vector &a, float k) {
  return {a.x * k, a.y * k, a.z * k};
}

static float punto(const Vector &a, const Vector &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vector cruz(const Vector &a, const Vector &b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

static float largo(const Vector &a) {
  return sqrtf(punto(a, a));
}

static void subir(Extremo &e, float valor) {
  if (!e.hay || valor > e.valor) {
    e.hay = true;
    e.valor = valor;
  }
}

void FuerzaG::medir(const Vector &a, uint32_t momento, bool parado) {
  uint32_t lapso = momento - ultima;
  bool seguida = hayMuestra && lapso <= reglas.vigenciaMs;
  // Primer orden; después de un hueco arranca de la muestra
  if (seguida && reglas.filtroMs + lapso > 0) {
    filtrada = mas(filtrada, por(menos(a, filtrada), (float)lapso / (reglas.filtroMs + lapso)));
  } else {
    filtrada = a;
  }
  hayMuestra = true;
  ultima = momento;
  ultimaCruda = a;

  revisarQuieta(a, momento, seguida, parado);
  if (!calibrada) {
    return;
  }
  lateral = punto(filtrada, ejeDerecha);
  longitudinal = punto(filtrada, ejeAdelante);
  subir(maxFrenada, fmaxf(0, -longitudinal));
  subir(maxAceleracion, fmaxf(0, longitudinal));
  subir(maxIzquierda, fmaxf(0, -lateral));
  subir(maxDerecha, fmaxf(0, lateral));
}

// Cada muestra tiene que quedar cerca del promedio de las anteriores; si
// no, o si hubo un hueco, la cuenta empieza de nuevo desde esa
void FuerzaG::revisarQuieta(const Vector &a, uint32_t momento, bool seguida, bool parado) {
  if (calibrada && !parado) {
    muestrasQuieta = 0;
    return;
  }
  if (!seguida || (muestrasQuieta > 0 && largo(menos(a, por(sumaQuieta, 1.0f / muestrasQuieta))) >
                                             reglas.toleranciaQuieta)) {
    muestrasQuieta = 0;
  }
  if (muestrasQuieta == 0) {
    sumaQuieta = {0, 0, 0};
    quietaDesde = momento;
  }
  sumaQuieta = mas(sumaQuieta, a);
  muestrasQuieta++;
  if (momento - quietaDesde >= reglas.quietaMs) {
    calibrar(por(sumaQuieta, 1.0f / muestrasQuieta));
    muestrasQuieta = 0;
  }
}

void FuerzaG::calibrar(const Vector &promedio) {
  float g = largo(promedio);
  if (fabsf(g - 1) > TOLERANCIA_GRAVEDAD) {
    return;
  }
  Vector arriba = por(promedio, 1 / g);
  // La derecha, sin lo que tenga de vertical
  Vector derecha = menos(reglas.derecha, por(arriba, punto(reglas.derecha, arriba)));
  float horizontal = largo(derecha) / largo(reglas.derecha);
  if (!(horizontal >= MINIMO_HORIZONTAL)) {
    return;
  }
  ejeDerecha = por(derecha, 1 / largo(derecha));
  // Con x adelante, y a la derecha y z arriba
  ejeAdelante = cruz(arriba, ejeDerecha);
  calibrada = true;
  cantidadCalibraciones++;
}

LecturaG FuerzaG::lectura(uint32_t ahora) const {
  LecturaG l;
  l.calibrada = calibrada;
  // Con signo: la muestra puede ser un poco más nueva que ahora (otra tarea)
  l.actual = calibrada && hayMuestra && (int32_t)(ahora - ultima) <= (int32_t)reglas.vigenciaMs;
  l.lateral = lateral;
  l.longitudinal = longitudinal;
  l.frenada = maxFrenada;
  l.aceleracion = maxAceleracion;
  l.curvaIzquierda = maxIzquierda;
  l.curvaDerecha = maxDerecha;
  return l;
}

void FuerzaG::reiniciarMaximos() {
  maxFrenada = Extremo();
  maxAceleracion = Extremo();
  maxIzquierda = Extremo();
  maxDerecha = Extremo();
}

bool FuerzaG::cruda(Vector *a) const {
  *a = ultimaCruda;
  return hayMuestra;
}

void textoFuerzaG(float g, char *salida, size_t tam) {
  snprintf(salida, tam, "%.2f", fabsf(g));
}

const char *nombreLongitudinal(float g) {
  if (fabsf(g) < CASI_CERO) {
    return "Long.";
  }
  return g < 0 ? "Frenada" : "Acel.";
}

const char *nombreLateral(float g) {
  if (fabsf(g) < CASI_CERO) {
    return "Lateral";
  }
  return g < 0 ? "Curva izq" : "Curva der";
}
