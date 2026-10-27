#include <fuerza_g.h>
#include <unity.h>

#include <math.h>

// Las de config.h, con la derecha en el eje x del sensor
static const ReglasFuerzaG REGLAS = {{1, 0, 0}, 300, 0.05f, 2000, 500};

// Una muestra cada PERIODO_IMU_MS
static const uint32_t MUESTRA_MS = 20;

static const float PI_F = 3.14159265f;

// Cómo quedan los ejes del auto en los del sensor
struct Montaje {
  Vector arriba, adelante, derecha;
};

// Derecha, de frente: el lado largo (x) a la derecha, el corto (y) para
// arriba y la cara del sensor (z) hacia atrás
static const Montaje DERECHA = {{0, 1, 0}, {0, 0, -1}, {1, 0, 0}};

// Lo que mide el sensor con el auto acelerando así (en g): la gravedad
// hacia arriba más la aceleración
static Vector medida(const Montaje &m, float lateral, float longitudinal) {
  return {m.arriba.x + m.adelante.x * longitudinal + m.derecha.x * lateral,
          m.arriba.y + m.adelante.y * longitudinal + m.derecha.y * lateral,
          m.arriba.z + m.adelante.z * longitudinal + m.derecha.z * lateral};
}

// La misma muestra desde desde durante ms (las dos puntas incluidas).
// Devuelve el momento de la última.
static uint32_t mantener(FuerzaG &f, const Vector &a, uint32_t desde, uint32_t ms,
                         bool parado = false) {
  uint32_t ahora = desde;
  for (; ahora - desde <= ms; ahora += MUESTRA_MS) {
    f.medir(a, ahora, parado);
  }
  return ahora - MUESTRA_MS;
}

// Quieta y derecha hasta calibrar. Devuelve el momento de la última muestra.
static uint32_t calibrar(FuerzaG &f, const Montaje &m = DERECHA, uint32_t desde = 1000) {
  return mantener(f, medida(m, 0, 0), desde, REGLAS.quietaMs);
}

void setUp() {}

void tearDown() {}

static void test_sin_calibrar() {
  FuerzaG f(REGLAS);
  LecturaG l = f.lectura(1000);
  TEST_ASSERT_FALSE(l.calibrada);
  TEST_ASSERT_FALSE(l.actual);
  TEST_ASSERT_FALSE(l.frenada.hay);
  TEST_ASSERT_FALSE(l.aceleracion.hay);
  TEST_ASSERT_FALSE(l.curvaIzquierda.hay);
  TEST_ASSERT_FALSE(l.curvaDerecha.hay);
  Vector a;
  TEST_ASSERT_FALSE(f.cruda(&a));
}

static void test_calibra_quieta() {
  FuerzaG f(REGLAS);
  // Sin la ECU (parado en false): la primera vez alcanza con quieta
  uint32_t ahora = mantener(f, medida(DERECHA, 0, 0), 1000, REGLAS.quietaMs - MUESTRA_MS);
  TEST_ASSERT_FALSE(f.lectura(ahora).calibrada);
  ahora += MUESTRA_MS;
  f.medir(medida(DERECHA, 0, 0), ahora, false);
  LecturaG l = f.lectura(ahora);
  TEST_ASSERT_TRUE(l.calibrada);
  TEST_ASSERT_TRUE(l.actual);
  TEST_ASSERT_EQUAL_UINT32(1, f.calibraciones());
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0, l.lateral);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0, l.longitudinal);
  // Los máximos arrancan en 0
  TEST_ASSERT_TRUE(l.frenada.hay);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0, l.frenada.valor);
  TEST_ASSERT_TRUE(l.curvaDerecha.hay);
  Vector a;
  TEST_ASSERT_TRUE(f.cruda(&a));
  TEST_ASSERT_EQUAL_FLOAT(1, a.y);
}

static void test_frenada_y_curvas() {
  FuerzaG f(REGLAS);
  uint32_t ahora = calibrar(f);
  ahora = mantener(f, medida(DERECHA, 0, -0.5f), ahora + MUESTRA_MS, 3000);
  LecturaG l = f.lectura(ahora);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, -0.5f, l.longitudinal);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0, l.lateral);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.5f, l.frenada.valor);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0, l.aceleracion.valor);

  // Curva a la derecha acelerando, y después a la izquierda
  ahora = mantener(f, medida(DERECHA, 0.3f, 0.2f), ahora + MUESTRA_MS, 3000);
  l = f.lectura(ahora);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.3f, l.lateral);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.2f, l.longitudinal);
  ahora = mantener(f, medida(DERECHA, -0.25f, 0), ahora + MUESTRA_MS, 3000);
  l = f.lectura(ahora);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, -0.25f, l.lateral);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.5f, l.frenada.valor);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.2f, l.aceleracion.valor);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.25f, l.curvaIzquierda.valor);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.3f, l.curvaDerecha.valor);
}

// La pantalla tirada 25° para atrás: el arriba y el adelante del sensor
// cambian, y la cuenta tiene que dar lo mismo
static void test_inclinada() {
  float c = cosf(25 * PI_F / 180);
  float s = sinf(25 * PI_F / 180);
  Montaje m = {{0, c, s}, {0, s, -c}, {1, 0, 0}};
  FuerzaG f(REGLAS);
  uint32_t ahora = calibrar(f, m);
  ahora = mantener(f, medida(m, -0.25f, 0.4f), ahora + MUESTRA_MS, 3000);
  LecturaG l = f.lectura(ahora);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.4f, l.longitudinal);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, -0.25f, l.lateral);
}

// Torcida 10° (el lado derecho más abajo): el eje x del sensor tiene algo
// de vertical, y la calibración lo saca
static void test_torcida() {
  float c = cosf(10 * PI_F / 180);
  float s = sinf(10 * PI_F / 180);
  Montaje m = {{-s, c, 0}, {0, 0, -1}, {c, s, 0}};
  FuerzaG f(REGLAS);
  uint32_t ahora = calibrar(f, m);
  ahora = mantener(f, medida(m, 0.3f, -0.2f), ahora + MUESTRA_MS, 3000);
  LecturaG l = f.lectura(ahora);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.3f, l.lateral);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, -0.2f, l.longitudinal);
}

static void test_filtro() {
  // Una vibración de 0,3 g a 25 Hz (una muestra para cada lado) casi no pasa,
  // ni deja máximos
  FuerzaG f(REGLAS);
  uint32_t ahora = calibrar(f);
  float peor = 0;
  for (int i = 1; i <= 150; i++) {
    ahora += MUESTRA_MS;
    f.medir(medida(DERECHA, i % 2 == 0 ? 0.3f : -0.3f, 0), ahora, false);
    peor = fmaxf(peor, fabsf(f.lectura(ahora).lateral));
  }
  TEST_ASSERT_TRUE(peor < 0.03f);
  LecturaG l = f.lectura(ahora);
  TEST_ASSERT_TRUE(l.curvaIzquierda.valor < 0.03f);
  TEST_ASSERT_TRUE(l.curvaDerecha.valor < 0.03f);

  // Una frenada de golpe llega a ~63 % en filtroMs
  FuerzaG escalon(REGLAS);
  ahora = calibrar(escalon);
  ahora = mantener(escalon, medida(DERECHA, 0, -0.5f), ahora + MUESTRA_MS,
                   REGLAS.filtroMs - MUESTRA_MS);
  float llegada = -escalon.lectura(ahora).longitudinal;
  TEST_ASSERT_TRUE(llegada > 0.29f && llegada < 0.34f);
}

// Parada en una subida de 3°: la gravedad tira para atrás y se ve como
// acelerar. Recalibra solo con la ECU en 0 km/h.
static void test_recalibra_parado() {
  float s = sinf(3 * PI_F / 180);
  Vector subida = {0, cosf(3 * PI_F / 180), -s};
  FuerzaG f(REGLAS);
  uint32_t ahora = calibrar(f);
  ahora = mantener(f, subida, ahora + MUESTRA_MS, 3000);
  LecturaG l = f.lectura(ahora);
  TEST_ASSERT_EQUAL_UINT32(1, f.calibraciones());
  TEST_ASSERT_FLOAT_WITHIN(0.002f, s, l.longitudinal);

  // Parado, a los quietaMs
  ahora = mantener(f, subida, ahora + MUESTRA_MS, REGLAS.quietaMs - MUESTRA_MS, true);
  TEST_ASSERT_EQUAL_UINT32(1, f.calibraciones());
  ahora += MUESTRA_MS;
  f.medir(subida, ahora, true);
  TEST_ASSERT_EQUAL_UINT32(2, f.calibraciones());
  TEST_ASSERT_FLOAT_WITHIN(0.002f, 0, f.lectura(ahora).longitudinal);

  // En una curva larga y pareja (quieta para el sensor) no recalibra
  FuerzaG curva(REGLAS);
  ahora = calibrar(curva);
  ahora = mantener(curva, medida(DERECHA, 0.3f, 0), ahora + MUESTRA_MS, 10000);
  TEST_ASSERT_EQUAL_UINT32(1, curva.calibraciones());
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.3f, curva.lectura(ahora).lateral);
}

// Parado tiene que durar todo el rato
static void test_parado_todo_el_rato() {
  FuerzaG f(REGLAS);
  uint32_t ahora = calibrar(f);
  Vector quieta = medida(DERECHA, 0, 0);
  ahora = mantener(f, quieta, ahora + MUESTRA_MS, 1500, true);
  ahora += MUESTRA_MS;
  f.medir(quieta, ahora, false);
  ahora = mantener(f, quieta, ahora + MUESTRA_MS, 1500, true);
  TEST_ASSERT_EQUAL_UINT32(1, f.calibraciones());
  mantener(f, quieta, ahora + MUESTRA_MS, 600, true);
  TEST_ASSERT_EQUAL_UINT32(2, f.calibraciones());
}

static void test_se_mueve_no_calibra() {
  // Se mueve cada medio segundo: nunca queda quieta 2 s. Termina corrida.
  FuerzaG f(REGLAS);
  uint32_t ahora = 1000;
  for (int i = 0; i < 13; i++) {
    ahora = mantener(f, medida(DERECHA, i % 2 == 0 ? 0.2f : 0, 0), ahora + MUESTRA_MS, 500);
  }
  TEST_ASSERT_FALSE(f.lectura(ahora).calibrada);
  // Una sacudida a mitad de camino también empieza de nuevo
  ahora = mantener(f, medida(DERECHA, 0, 0), ahora + MUESTRA_MS, 1500);
  ahora += MUESTRA_MS;
  f.medir(medida(DERECHA, 0, 0.1f), ahora, false);
  ahora = mantener(f, medida(DERECHA, 0, 0), ahora + MUESTRA_MS, 1500);
  TEST_ASSERT_FALSE(f.lectura(ahora).calibrada);
  mantener(f, medida(DERECHA, 0, 0), ahora + MUESTRA_MS, 500);
  TEST_ASSERT_TRUE(f.lectura(ahora).calibrada);
}

static void test_vigencia() {
  FuerzaG f(REGLAS);
  uint32_t ahora = calibrar(f);
  TEST_ASSERT_TRUE(f.lectura(ahora + REGLAS.vigenciaMs).actual);
  TEST_ASSERT_FALSE(f.lectura(ahora + REGLAS.vigenciaMs + 1).actual);
  // Una muestra un poco más nueva que ahora (la tomó otra tarea) es actual
  TEST_ASSERT_TRUE(f.lectura(ahora - 5).actual);
  // Vencida, los máximos siguen
  TEST_ASSERT_TRUE(f.lectura(ahora + 10000).frenada.hay);

  // Después de un hueco el filtro arranca de la muestra
  ahora += 1000;
  f.medir(medida(DERECHA, 0, -0.5f), ahora, false);
  TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, f.lectura(ahora).longitudinal);
}

static void test_no_calibra_mal() {
  // La escala mal: quieta mide 0,5 g
  FuerzaG escala(REGLAS);
  uint32_t ahora = mantener(escala, {0, 0.5f, 0}, 1000, 5000);
  TEST_ASSERT_FALSE(escala.lectura(ahora).calibrada);

  // El eje de la derecha para arriba: la placa no está como dicen las reglas
  FuerzaG parada(REGLAS);
  ahora = mantener(parada, {1, 0, 0}, 1000, 5000);
  TEST_ASSERT_FALSE(parada.lectura(ahora).calibrada);
}

static void test_reiniciar_maximos() {
  FuerzaG f(REGLAS);
  uint32_t ahora = calibrar(f);
  ahora = mantener(f, medida(DERECHA, 0.4f, -0.5f), ahora + MUESTRA_MS, 3000);
  ahora = mantener(f, medida(DERECHA, 0, 0), ahora + MUESTRA_MS, 3000);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, 0.5f, f.lectura(ahora).frenada.valor);
  f.reiniciarMaximos();
  LecturaG l = f.lectura(ahora);
  TEST_ASSERT_FALSE(l.frenada.hay);
  TEST_ASSERT_FALSE(l.curvaDerecha.hay);
  TEST_ASSERT_TRUE(l.calibrada);
  ahora += MUESTRA_MS;
  f.medir(medida(DERECHA, 0, 0), ahora, false);
  l = f.lectura(ahora);
  TEST_ASSERT_TRUE(l.frenada.hay);
  TEST_ASSERT_TRUE(l.frenada.valor < 0.01f);
  TEST_ASSERT_TRUE(l.curvaDerecha.valor < 0.01f);
}

static void test_millis_da_la_vuelta() {
  FuerzaG f(REGLAS);
  uint32_t ahora = calibrar(f, DERECHA, 0xFFFFFFFF - 1000);
  TEST_ASSERT_TRUE(f.lectura(ahora).calibrada);
  TEST_ASSERT_TRUE(ahora < 2000);
  ahora = mantener(f, medida(DERECHA, 0, -0.3f), ahora + MUESTRA_MS, 3000);
  TEST_ASSERT_TRUE(f.lectura(ahora).actual);
  TEST_ASSERT_FLOAT_WITHIN(0.005f, -0.3f, f.lectura(ahora).longitudinal);
}

static void verificarTexto(float g, const char *esperado) {
  char texto[8];
  textoFuerzaG(g, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING(esperado, texto);
}

static void test_textos() {
  verificarTexto(-0.414f, "0.41");
  verificarTexto(0.28f, "0.28");
  verificarTexto(1.236f, "1.24");
  verificarTexto(-0.004f, "0.00");
  verificarTexto(0, "0.00");

  TEST_ASSERT_EQUAL_STRING("Frenada", nombreLongitudinal(-0.41f));
  TEST_ASSERT_EQUAL_STRING("Acel.", nombreLongitudinal(0.12f));
  TEST_ASSERT_EQUAL_STRING("Long.", nombreLongitudinal(-0.004f));
  TEST_ASSERT_EQUAL_STRING("Curva der", nombreLateral(0.28f));
  TEST_ASSERT_EQUAL_STRING("Curva izq", nombreLateral(-0.62f));
  TEST_ASSERT_EQUAL_STRING("Lateral", nombreLateral(0.004f));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_sin_calibrar);
  RUN_TEST(test_calibra_quieta);
  RUN_TEST(test_frenada_y_curvas);
  RUN_TEST(test_inclinada);
  RUN_TEST(test_torcida);
  RUN_TEST(test_filtro);
  RUN_TEST(test_recalibra_parado);
  RUN_TEST(test_parado_todo_el_rato);
  RUN_TEST(test_se_mueve_no_calibra);
  RUN_TEST(test_vigencia);
  RUN_TEST(test_no_calibra_mal);
  RUN_TEST(test_reiniciar_maximos);
  RUN_TEST(test_millis_da_la_vuelta);
  RUN_TEST(test_textos);
  return UNITY_END();
}
