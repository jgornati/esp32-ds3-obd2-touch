#include <alarmas.h>
#include <unity.h>

#include <string.h>

// Las de config.h
static const ReglaAlarma REFRIGERANTE = {105, 102, true, 5000, 5000};
static const ReglaAlarma BAJO = {13.0f, 13.2f, false, 10000, 10000};
static const ReglaAlarma RPM = {6000, 5800, true, 0, 0};

void setUp() {}

void tearDown() {}

static void test_refrigerante() {
  Alarma a(REFRIGERANTE);
  TEST_ASSERT_FALSE(a.activa());
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 98, 1000));
  // Un dato suelto más allá del umbral no la prende
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 106, 2000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 98, 2150));
  TEST_ASSERT_FALSE(a.activa());
  // 5 s seguidos, sí
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 105, 10000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 106, 14999));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, a.revisar(true, 106, 15000));
  TEST_ASSERT_TRUE(a.activa());
  TEST_ASSERT_EQUAL_FLOAT(106, a.valor());
  // Revisar otra vez la misma lectura no cambia nada
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 106, 15000));
  // Entre 102 y 105 sigue prendida
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 103, 16000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 103, 30000));
  TEST_ASSERT_TRUE(a.activa());
  // En 102 o menos, 5 s seguidos, se apaga
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 102, 31000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 103, 33000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 101, 34000));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_APAGO, a.revisar(true, 100, 39000));
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
  // RPM: sin espera, con histéresis
  Alarma a(RPM);
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 5990, 1000));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, a.revisar(true, 6000, 1150));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 5850, 1300));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_APAGO, a.revisar(true, 5800, 1450));
}

static void test_sin_valor() {
  // Sin un valor actual se apaga, y vuelve a contar desde cero
  Alarma a(REFRIGERANTE);
  a.revisar(true, 106, 0);
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, a.revisar(true, 106, 5000));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_APAGO, a.revisar(false, 0, 6000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(false, 0, 7000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 106, 8000));
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 106, 12999));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, a.revisar(true, 106, 13000));

  // Contando para prenderse, un rato sin valor vuelve a empezar
  Alarma b(REFRIGERANTE);
  b.revisar(true, 106, 0);
  b.revisar(false, 0, 3000);
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, b.revisar(true, 106, 5000));
  TEST_ASSERT_FALSE(b.activa());
}

static void test_millis_da_la_vuelta() {
  Alarma a(REFRIGERANTE);
  a.revisar(true, 106, 0xFFFFFFFF - 1999);
  TEST_ASSERT_EQUAL(CambioAlarma::NINGUNO, a.revisar(true, 106, 2000));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, a.revisar(true, 106, 3000));
}

static void test_varias() {
  static const ReglaAlarma REGLAS[CANT_ALARMAS] = {
      REFRIGERANTE, BAJO, {15.0f, 14.8f, true, 0, 0}, RPM};
  Alarmas alarmas(REGLAS);
  TipoAlarma tipo;
  uint8_t numero;
  TEST_ASSERT_EQUAL_UINT8(0, alarmas.activas());
  TEST_ASSERT_FALSE(alarmas.aLaVista(0, &tipo, &numero));

  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, alarmas.revisar(RPM_ALTA, true, 6200, 0));
  TEST_ASSERT_EQUAL(CambioAlarma::SE_PRENDIO, alarmas.revisar(VOLTAJE_ALTO, true, 15.4f, 0));
  TEST_ASSERT_EQUAL_UINT8(2, alarmas.activas());
  TEST_ASSERT_TRUE(alarmas.alarma(RPM_ALTA).activa());
  TEST_ASSERT_FALSE(alarmas.alarma(VOLTAJE_BAJO).activa());

  // Se alternan en el orden de TipoAlarma
  TEST_ASSERT_TRUE(alarmas.aLaVista(0, &tipo, &numero));
  TEST_ASSERT_EQUAL(VOLTAJE_ALTO, tipo);
  TEST_ASSERT_EQUAL_UINT8(0, numero);
  TEST_ASSERT_TRUE(alarmas.aLaVista(1, &tipo, &numero));
  TEST_ASSERT_EQUAL(RPM_ALTA, tipo);
  TEST_ASSERT_EQUAL_UINT8(1, numero);
  TEST_ASSERT_TRUE(alarmas.aLaVista(2, &tipo, &numero));
  TEST_ASSERT_EQUAL(VOLTAJE_ALTO, tipo);

  // Con una sola, siempre esa
  alarmas.revisar(VOLTAJE_ALTO, true, 14.0f, 100);
  TEST_ASSERT_TRUE(alarmas.aLaVista(7, &tipo, &numero));
  TEST_ASSERT_EQUAL(RPM_ALTA, tipo);
  TEST_ASSERT_EQUAL_UINT8(0, numero);
}

static void verificarTexto(TipoAlarma tipo, float valor, const char *esperado) {
  char texto[32];
  textoAlarma(tipo, valor, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING(esperado, texto);
}

static void test_textos() {
  verificarTexto(REFRIGERANTE_ALTO, 106, "REFRIGERANTE 106 °C");
  verificarTexto(VOLTAJE_BAJO, 12.64f, "VOLTAJE BAJO 12.6 V");
  verificarTexto(VOLTAJE_ALTO, 15.4f, "VOLTAJE ALTO 15.4 V");
  verificarTexto(RPM_ALTA, 6212.25f, "ZONA ROJA 6212 RPM");
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
