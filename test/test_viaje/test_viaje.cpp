#include <unity.h>
#include <viaje.h>

// Las de config.h: VIGENCIA_MS, RPM_EN_MARCHA y ESPERA_VOLTAJE_VIAJE_MS
static const ReglasViaje REGLAS = {3000, 500, 5000};

// Cada lectura de la ECU, ~6 por segundo (E3)
static const uint32_t CONSULTA_MS = 150;

// Unity compara float, no double (salvo UNITY_INCLUDE_DOUBLE)
static float km(const Viaje &viaje) {
  return (float)viaje.distanciaKm();
}

void setUp() {}

void tearDown() {}

// Velocidad y RPM fijas durante ms, una lectura de cada una por consulta.
// Devuelve el momento de la última.
static uint32_t andar(Viaje &viaje, uint32_t desde, uint32_t ms, float kmh, float rpm) {
  uint32_t ahora = desde;
  for (; ahora - desde <= ms; ahora += CONSULTA_MS) {
    viaje.velocidad(kmh, ahora);
    viaje.rpm(rpm, ahora);
  }
  return ahora - CONSULTA_MS;
}

static void test_al_empezar() {
  Viaje viaje(REGLAS);
  TEST_ASSERT_EQUAL_FLOAT(0, km(viaje));
  TEST_ASSERT_EQUAL_UINT32(0, viaje.tiempoEnMarchaMs());
  float kmh;
  TEST_ASSERT_FALSE(viaje.promedio(&kmh));
  TEST_ASSERT_FALSE(viaje.velocidadMaxima().hay);
  TEST_ASSERT_FALSE(viaje.rpmMaxima().hay);
  TEST_ASSERT_FALSE(viaje.refrigeranteMaximo().hay);
  TEST_ASSERT_FALSE(viaje.voltajeMinimo().hay);
  TEST_ASSERT_FALSE(viaje.voltajeMaximo().hay);
}

static void test_distancia() {
  // 60 km/h durante un minuto: 1 km
  Viaje viaje(REGLAS);
  andar(viaje, 1000, 60000, 60, 2000);
  TEST_ASSERT_FLOAT_WITHIN(0.0001, 1.0, km(viaje));

  // De 0 a 36 km/h (10 m/s) en un segundo: el trapecio da 5 m
  Viaje rampa(REGLAS);
  rampa.velocidad(0, 1000);
  rampa.velocidad(36, 2000);
  TEST_ASSERT_FLOAT_WITHIN(0.000001, 0.005, km(rampa));

  // Una sola lectura no recorre nada
  Viaje una(REGLAS);
  una.velocidad(100, 1000);
  TEST_ASSERT_EQUAL_FLOAT(0, km(una));
}

static void test_huecos() {
  // Hasta el hueco máximo suma; más largo, no
  Viaje viaje(REGLAS);
  viaje.velocidad(36, 1000);
  viaje.velocidad(36, 4000);
  TEST_ASSERT_FLOAT_WITHIN(0.000001, 0.030, km(viaje));
  viaje.velocidad(36, 7001);
  TEST_ASSERT_FLOAT_WITHIN(0.000001, 0.030, km(viaje));
  // Después del hueco sigue sumando
  viaje.velocidad(36, 8001);
  TEST_ASSERT_FLOAT_WITHIN(0.000001, 0.040, km(viaje));

  // Lo mismo con el tiempo en marcha
  Viaje tiempo(REGLAS);
  tiempo.rpm(900, 1000);
  tiempo.rpm(900, 4000);
  tiempo.rpm(900, 7001);
  tiempo.rpm(900, 8001);
  TEST_ASSERT_EQUAL_UINT32(4000, tiempo.tiempoEnMarchaMs());
}

static void test_tiempo_en_marcha() {
  // Con contacto y el motor parado no cuenta; al arrancar (debajo de 500
  // rpm) tampoco; en ralentí, sí
  Viaje viaje(REGLAS);
  uint32_t ahora = andar(viaje, 1000, 10000, 0, 0);
  ahora = andar(viaje, ahora + CONSULTA_MS, 1500, 0, 250);
  TEST_ASSERT_EQUAL_UINT32(0, viaje.tiempoEnMarchaMs());
  ahora = andar(viaje, ahora + CONSULTA_MS, 60000, 0, 950);
  // 401 lecturas en ralentí: el primer lapso es el de la segunda
  TEST_ASSERT_EQUAL_UINT32(60000, viaje.tiempoEnMarchaMs());

  // Se para el motor: deja de contar
  uint32_t enMarcha = viaje.tiempoEnMarchaMs();
  andar(viaje, ahora + CONSULTA_MS, 10000, 0, 0);
  TEST_ASSERT_EQUAL_UINT32(enMarcha, viaje.tiempoEnMarchaMs());
}

static void test_promedio() {
  // 30 s parado en ralentí y 60 s a 90 km/h: 1,5 km en 90 s son 60 km/h
  Viaje viaje(REGLAS);
  uint32_t ahora = andar(viaje, 1000, 30000, 0, 900);
  andar(viaje, ahora + CONSULTA_MS, 60000, 90, 2500);
  float kmh = 0;
  TEST_ASSERT_TRUE(viaje.promedio(&kmh));
  TEST_ASSERT_FLOAT_WITHIN(0.2f, 60, kmh);
}

static void test_voltaje() {
  Viaje viaje(REGLAS);
  // Con contacto, sin motor: no cuenta
  viaje.rpm(0, 1000);
  viaje.voltaje(12.4f, 1100);
  // Arranca a los 2 s: en los primeros 5 s, no cuenta
  viaje.rpm(950, 2000);
  viaje.rpm(950, 3000);
  viaje.voltaje(10.5f, 3000);
  viaje.rpm(950, 5000);
  viaje.rpm(950, 6999);
  viaje.voltaje(13.0f, 6999);
  TEST_ASSERT_FALSE(viaje.voltajeMinimo().hay);
  // Desde ahí, sí
  viaje.voltaje(13.6f, 7000);
  viaje.rpm(950, 9000);
  viaje.voltaje(14.3f, 9000);
  TEST_ASSERT_TRUE(viaje.voltajeMinimo().hay);
  TEST_ASSERT_EQUAL_FLOAT(13.6f, viaje.voltajeMinimo().valor);
  TEST_ASSERT_EQUAL_FLOAT(14.3f, viaje.voltajeMaximo().valor);

  // Sin una RPM reciente no se sabe si sigue en marcha
  viaje.voltaje(11.0f, 12001);
  TEST_ASSERT_EQUAL_FLOAT(13.6f, viaje.voltajeMinimo().valor);

  // Vuelve después del hueco: espera los 5 s de nuevo
  viaje.rpm(950, 13000);
  viaje.voltaje(11.0f, 13100);
  TEST_ASSERT_EQUAL_FLOAT(13.6f, viaje.voltajeMinimo().valor);

  // Se para el motor con contacto: no cuenta
  Viaje parado(REGLAS);
  uint32_t ahora = andar(parado, 1000, 10000, 0, 950);
  parado.rpm(0, ahora + CONSULTA_MS);
  parado.voltaje(12.2f, ahora + 2 * CONSULTA_MS);
  TEST_ASSERT_FALSE(parado.voltajeMinimo().hay);
}

static void test_maximos() {
  Viaje viaje(REGLAS);
  viaje.velocidad(0, 1000);
  TEST_ASSERT_TRUE(viaje.velocidadMaxima().hay);
  TEST_ASSERT_EQUAL_FLOAT(0, viaje.velocidadMaxima().valor);
  viaje.velocidad(118, 1150);
  viaje.velocidad(60, 1300);
  viaje.rpm(5120, 1000);
  viaje.rpm(800, 1150);
  viaje.refrigerante(-5);
  viaje.refrigerante(94);
  viaje.refrigerante(90);
  TEST_ASSERT_EQUAL_FLOAT(118, viaje.velocidadMaxima().valor);
  TEST_ASSERT_EQUAL_FLOAT(5120, viaje.rpmMaxima().valor);
  TEST_ASSERT_EQUAL_FLOAT(94, viaje.refrigeranteMaximo().valor);
}

static void test_reiniciar() {
  Viaje viaje(REGLAS);
  uint32_t ahora = andar(viaje, 1000, 20000, 50, 2000);
  viaje.refrigerante(90);
  viaje.voltaje(14, ahora);
  viaje.reiniciar();
  TEST_ASSERT_EQUAL_FLOAT(0, km(viaje));
  TEST_ASSERT_FALSE(viaje.velocidadMaxima().hay);
  TEST_ASSERT_FALSE(viaje.voltajeMaximo().hay);

  // La primera lectura después no suma lo de antes de reiniciar
  viaje.velocidad(50, ahora + CONSULTA_MS);
  viaje.rpm(2000, ahora + CONSULTA_MS);
  TEST_ASSERT_EQUAL_FLOAT(0, km(viaje));
  TEST_ASSERT_EQUAL_UINT32(0, viaje.tiempoEnMarchaMs());
  // Y sigue con las mismas reglas: el voltaje espera los 5 s
  viaje.voltaje(14, ahora + 2 * CONSULTA_MS);
  TEST_ASSERT_FALSE(viaje.voltajeMaximo().hay);
}

static void test_en_marcha_desde_hace() {
  // Lo que usa la alarma de voltaje (E11)
  Viaje viaje(REGLAS);
  TEST_ASSERT_FALSE(viaje.enMarchaDesdeHace(1000, 0));
  viaje.rpm(0, 1000);
  TEST_ASSERT_FALSE(viaje.enMarchaDesdeHace(1000, 0));
  viaje.rpm(950, 2000);
  TEST_ASSERT_TRUE(viaje.enMarchaDesdeHace(2000, 0));
  TEST_ASSERT_FALSE(viaje.enMarchaDesdeHace(2000, 5000));
  viaje.rpm(950, 4000);
  viaje.rpm(950, 7000);
  TEST_ASSERT_FALSE(viaje.enMarchaDesdeHace(6999, 5000));
  TEST_ASSERT_TRUE(viaje.enMarchaDesdeHace(7000, 5000));
  // Sin una RPM reciente, no
  TEST_ASSERT_TRUE(viaje.enMarchaDesdeHace(10000, 5000));
  TEST_ASSERT_FALSE(viaje.enMarchaDesdeHace(10001, 5000));
  // Se para el motor: no
  viaje.rpm(0, 8000);
  TEST_ASSERT_FALSE(viaje.enMarchaDesdeHace(8000, 0));
}

static void test_millis_da_la_vuelta() {
  // millis() vuelve a 0 a los ~49 días
  Viaje viaje(REGLAS);
  uint32_t antes = 0xFFFFFFFF - 499;
  viaje.velocidad(36, antes);
  viaje.rpm(900, antes);
  viaje.velocidad(36, 500);
  viaje.rpm(900, 500);
  TEST_ASSERT_FLOAT_WITHIN(0.000001, 0.010, km(viaje));
  TEST_ASSERT_EQUAL_UINT32(1000, viaje.tiempoEnMarchaMs());
}

static void verificarDistancia(double km, const char *esperado) {
  char texto[12];
  textoDistancia(km, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING(esperado, texto);
}

static void verificarTiempo(uint32_t ms, const char *esperado, const char *unidad) {
  char texto[12];
  TEST_ASSERT_EQUAL_STRING(unidad, textoTiempo(ms, texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING(esperado, texto);
}

static void test_textos() {
  verificarDistancia(0, "0.0");
  verificarDistancia(23.74, "23.7");
  verificarDistancia(99.94, "99.9");
  verificarDistancia(99.96, "100");
  verificarDistancia(123.4, "123");
  verificarDistancia(1234.6, "1235");

  verificarTiempo(0, "0", "min");
  verificarTiempo(59999, "0", "min");
  verificarTiempo(38 * 60000, "38", "min");
  verificarTiempo(60 * 60000 - 1, "59", "min");
  verificarTiempo(60 * 60000, "1:00", "h");
  verificarTiempo(67 * 60000 + 59999, "1:07", "h");
  verificarTiempo(754 * 60000, "12:34", "h");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_al_empezar);
  RUN_TEST(test_distancia);
  RUN_TEST(test_huecos);
  RUN_TEST(test_tiempo_en_marcha);
  RUN_TEST(test_promedio);
  RUN_TEST(test_voltaje);
  RUN_TEST(test_maximos);
  RUN_TEST(test_reiniciar);
  RUN_TEST(test_en_marcha_desde_hace);
  RUN_TEST(test_millis_da_la_vuelta);
  RUN_TEST(test_textos);
  return UNITY_END();
}
