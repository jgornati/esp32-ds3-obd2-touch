#include <alarmas.h>
#include <unity.h>

#include <string.h>

// Las de config.h
static const ReglaAlarma REFRIGERANTE = {110, 108, true, 5000, 5000};
static const ReglaAlarma BAJO = {13.0f, 13.2f, false, 10000, 10000};
static const ReglaAlarma ALTO = {15.0f, 14.8f, true, 10000, 10000};
// Sin espera, como era la de las RPM (ZONA ROJA) hasta E25
static const ReglaAlarma AL_INSTANTE = {6000, 5800, true, 0, 0};

void setUp() {}

void tearDown() {}

static void test_refrigerante() {
  Alarma a(REFRIGERANTE);
  TEST_ASSERT_FALSE(a.activa());
  // Lo normal del EP6C caliente: regula en 103-108 °C
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 108, 1000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 108, 60000));
  TEST_ASSERT_FALSE(a.activa());
  // Un dato suelto más allá del umbral no la prende
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 111, 62000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 108, 62150));
  TEST_ASSERT_FALSE(a.activa());
  // 109 no la prende
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 109, 63000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 109, 70000));
  TEST_ASSERT_FALSE(a.activa());
  // 110 o más, 5 s seguidos, sí
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 110, 80000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 111, 84999));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, a.revisar(true, 111, 85000));
  TEST_ASSERT_TRUE(a.activa());
  TEST_ASSERT_EQUAL_FLOAT(111, a.valor());
  // Revisar otra vez la misma lectura no cambia nada
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 111, 85000));
  // En 109 sigue prendida
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 109, 86000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 109, 100000));
  TEST_ASSERT_TRUE(a.activa());
  // En 108 o menos, 5 s seguidos, se apaga
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 108, 101000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 109, 103000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 107, 104000));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_APAGO, a.revisar(true, 106, 109000));
  TEST_ASSERT_FALSE(a.activa());
}

static void test_por_abajo() {
  // Voltaje bajo: 10 s seguidos en 13.0 o menos; se apaga con 13.2 o más
  Alarma a(BAJO);
  for (uint32_t t = 0; t < 10000; t += 2000) {
    TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 12.6f, t));
  }
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, a.revisar(true, 12.7f, 10000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 13.1f, 12000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 13.2f, 14000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 13.9f, 22000));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_APAGO, a.revisar(true, 14.0f, 24000));

  // 13.1 no la prende
  Alarma justa(BAJO);
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, justa.revisar(true, 13.1f, 0));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, justa.revisar(true, 13.1f, 60000));
  TEST_ASSERT_FALSE(justa.activa());
}

static void test_al_instante() {
  // Sin espera, con histéresis
  Alarma a(AL_INSTANTE);
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 5990, 1000));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, a.revisar(true, 6000, 1150));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 5850, 1300));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_APAGO, a.revisar(true, 5800, 1450));
}

static void test_sin_valor() {
  // Sin un valor actual se apaga, y vuelve a contar desde cero
  Alarma a(REFRIGERANTE);
  a.revisar(true, 111, 0);
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, a.revisar(true, 111, 5000));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_APAGO, a.revisar(false, 0, 6000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(false, 0, 7000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 111, 8000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 111, 12999));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, a.revisar(true, 111, 13000));

  // Contando para prenderse, un rato sin valor vuelve a empezar
  Alarma b(REFRIGERANTE);
  b.revisar(true, 111, 0);
  b.revisar(false, 0, 3000);
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, b.revisar(true, 111, 5000));
  TEST_ASSERT_FALSE(b.activa());
}

static void test_millis_da_la_vuelta() {
  Alarma a(REFRIGERANTE);
  a.revisar(true, 111, 0xFFFFFFFF - 1999);
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 111, 2000));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, a.revisar(true, 111, 3000));
}

static void test_varias() {
  static const ReglaAlarma REGLAS[CANT_ALARMAS] = {REFRIGERANTE, BAJO, ALTO};
  Alarmas alarmas(REGLAS);
  TipoAlarma tipo;
  uint8_t numero;
  TEST_ASSERT_EQUAL_UINT8(0, alarmas.activas());
  TEST_ASSERT_FALSE(alarmas.aLaVista(0, &tipo, &numero));

  alarmas.revisar(REFRIGERANTE_ALTO, true, 112, 0);
  alarmas.revisar(VOLTAJE_ALTO, true, 15.4f, 0);
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, alarmas.revisar(VOLTAJE_ALTO, true, 15.4f, 10000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, alarmas.revisar(REFRIGERANTE_ALTO, true, 112, 4000));
  TEST_ASSERT_EQUAL_UINT8(1, alarmas.activas());
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, alarmas.revisar(REFRIGERANTE_ALTO, true, 112, 5000));
  TEST_ASSERT_EQUAL_UINT8(2, alarmas.activas());
  TEST_ASSERT_TRUE(alarmas.alarma(REFRIGERANTE_ALTO).activa());
  TEST_ASSERT_FALSE(alarmas.alarma(VOLTAJE_BAJO).activa());

  // Se alternan en el orden de TipoAlarma
  TEST_ASSERT_TRUE(alarmas.aLaVista(0, &tipo, &numero));
  TEST_ASSERT_EQUAL(REFRIGERANTE_ALTO, tipo);
  TEST_ASSERT_EQUAL_UINT8(0, numero);
  TEST_ASSERT_TRUE(alarmas.aLaVista(1, &tipo, &numero));
  TEST_ASSERT_EQUAL(VOLTAJE_ALTO, tipo);
  TEST_ASSERT_EQUAL_UINT8(1, numero);
  TEST_ASSERT_TRUE(alarmas.aLaVista(2, &tipo, &numero));
  TEST_ASSERT_EQUAL(REFRIGERANTE_ALTO, tipo);

  // Con una sola, siempre esa
  alarmas.revisar(REFRIGERANTE_ALTO, false, 0, 6000);
  TEST_ASSERT_TRUE(alarmas.aLaVista(7, &tipo, &numero));
  TEST_ASSERT_EQUAL(VOLTAJE_ALTO, tipo);
  TEST_ASSERT_EQUAL_UINT8(0, numero);
}

static void verificarTexto(TipoAlarma tipo, float valor, const char *esperado) {
  char texto[32];
  textoAlarma(tipo, valor, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING(esperado, texto);
}

static void test_textos() {
  verificarTexto(REFRIGERANTE_ALTO, 111.6f, "REFRIGERANTE 112 °C");
  verificarTexto(VOLTAJE_BAJO, 12.64f, "VOLTAJE BAJO 12.6 V");
  verificarTexto(VOLTAJE_ALTO, 15.4f, "VOLTAJE ALTO 15.4 V");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_refrigerante);
  RUN_TEST(test_por_abajo);
  RUN_TEST(test_al_instante);
  RUN_TEST(test_sin_valor);
  RUN_TEST(test_millis_da_la_vuelta);
  RUN_TEST(test_varias);
  RUN_TEST(test_textos);
  return UNITY_END();
}
