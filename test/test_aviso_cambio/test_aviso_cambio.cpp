#include <aviso_cambio.h>
#include <unity.h>

#include <string.h>

// Las de config.h: vuelve a avisar 300 rpm debajo, dura hasta 2 s, 5
// marchas, y se elige de 2000 a 6500 de a 250
static const ReglasAvisoCambio REGLAS = {300, 2000, 5, 2000, 6500, 250};

// Cada lectura de la ECU, ~6 por segundo (E3)
static const uint32_t CONSULTA_MS = 150;

void setUp() {}

void tearDown() {}

static void test_avisa_una_vez_al_llegar() {
  AvisoCambio aviso(REGLAS);
  aviso.ponerUmbral(6000);
  uint32_t t = 1000;
  TEST_ASSERT_FALSE(aviso.revisar(true, 5900, 2, t));
  TEST_ASSERT_FALSE(aviso.activo(t));
  TEST_ASSERT_TRUE(aviso.revisar(true, 6000, 2, t += CONSULTA_MS));
  TEST_ASSERT_TRUE(aviso.activo(t));
  // Revisar otra vez la misma lectura no vuelve a avisar
  TEST_ASSERT_FALSE(aviso.revisar(true, 6000, 2, t));
  // Sigue mientras las RPM estén en el umbral o más
  TEST_ASSERT_FALSE(aviso.revisar(true, 6150, 2, t + CONSULTA_MS));
  TEST_ASSERT_TRUE(aviso.activo(t + CONSULTA_MS));
  // Hasta 2 s, aunque sigan arriba
  TEST_ASSERT_FALSE(aviso.revisar(true, 6300, 2, t + 1900));
  TEST_ASSERT_TRUE(aviso.activo(t + 1999));
  TEST_ASSERT_FALSE(aviso.activo(t + 2000));
  TEST_ASSERT_FALSE(aviso.revisar(true, 6400, 2, t + 2100));
  TEST_ASSERT_FALSE(aviso.activo(t + 2100));
}

static void test_termina_al_bajar_del_umbral() {
  AvisoCambio aviso(REGLAS);
  aviso.ponerUmbral(6000);
  TEST_ASSERT_TRUE(aviso.revisar(true, 6050, 3, 1000));
  TEST_ASSERT_FALSE(aviso.revisar(true, 5990, 3, 1150));
  TEST_ASSERT_FALSE(aviso.activo(1150));
}

static void test_vuelve_a_avisar_cuando_bajan() {
  AvisoCambio aviso(REGLAS);
  aviso.ponerUmbral(6000);
  TEST_ASSERT_TRUE(aviso.revisar(true, 6000, 2, 1000));
  // Bajar a 5800 y volver a subir no avisa otra vez (sin pasar el cambio)
  TEST_ASSERT_FALSE(aviso.revisar(true, 5800, 2, 5000));
  TEST_ASSERT_FALSE(aviso.revisar(true, 6100, 2, 5150));
  TEST_ASSERT_FALSE(aviso.activo(5150));
  // 300 debajo del umbral, sí
  TEST_ASSERT_FALSE(aviso.revisar(true, 5700, 2, 9000));
  TEST_ASSERT_TRUE(aviso.revisar(true, 6000, 2, 9150));
}

// VIAJE0007.CSV, segundos 1031 a 1036: la pasada a fondo en 3ª, que llegó a
// 6215 rpm, y la 4ª. Con 6000, avisa al llegar, y no en 4ª, que entra a 4767.
static void test_la_pasada_de_los_registros() {
  AvisoCambio aviso(REGLAS);
  aviso.ponerUmbral(6000);
  TEST_ASSERT_FALSE(aviso.revisar(true, 5438, 3, 1031000));
  TEST_ASSERT_FALSE(aviso.revisar(true, 5730, 3, 1032000));
  TEST_ASSERT_FALSE(aviso.revisar(true, 5912, 3, 1033000));
  TEST_ASSERT_TRUE(aviso.revisar(true, 6215, 3, 1034000));
  TEST_ASSERT_FALSE(aviso.revisar(true, 4767, 4, 1035000));
  TEST_ASSERT_FALSE(aviso.activo(1035000));
  TEST_ASSERT_FALSE(aviso.revisar(true, 4838, 4, 1036000));
}

// Con 2500, como para probarlo: en cada marcha una vez (VIAJE0004.CSV, 2ª a
// 36 km/h); las RPM bajan al pasar la siguiente
static void test_en_cada_marcha() {
  AvisoCambio aviso(REGLAS);
  aviso.ponerUmbral(2500);
  TEST_ASSERT_FALSE(aviso.revisar(true, 2088, 2, 175000));
  TEST_ASSERT_TRUE(aviso.revisar(true, 2529, 2, 176000));
  TEST_ASSERT_FALSE(aviso.revisar(true, 2700, 2, 177000));
  // Embrague apretado: sin marcha, las RPM caen
  TEST_ASSERT_FALSE(aviso.revisar(true, 1500, 0, 178000));
  // En 3ª, al llegar otra vez
  TEST_ASSERT_FALSE(aviso.revisar(true, 1800, 3, 179000));
  TEST_ASSERT_TRUE(aviso.revisar(true, 2510, 3, 183000));
}

static void test_no_avisa_sin_marcha_ni_en_quinta() {
  AvisoCambio aviso(REGLAS);
  aviso.ponerUmbral(2500);
  // Acelerando parado (punto muerto), y en 5ª
  TEST_ASSERT_FALSE(aviso.revisar(true, 3500, 0, 1000));
  TEST_ASSERT_FALSE(aviso.activo(1000));
  TEST_ASSERT_FALSE(aviso.revisar(true, 2800, 5, 2000));
  TEST_ASSERT_FALSE(aviso.activo(2000));
  // Sigue listo: en 4ª, sí
  TEST_ASSERT_TRUE(aviso.revisar(true, 2800, 4, 3000));
  // Si pasa a 5ª con las RPM arriba, termina
  TEST_ASSERT_FALSE(aviso.revisar(true, 2600, 5, 3150));
  TEST_ASSERT_FALSE(aviso.activo(3150));
  // Y si se aprieta el embrague, también
  aviso.ponerUmbral(2500);
  TEST_ASSERT_TRUE(aviso.revisar(true, 2600, 1, 4000));
  TEST_ASSERT_FALSE(aviso.revisar(true, 2700, 0, 4150));
  TEST_ASSERT_FALSE(aviso.activo(4150));
}

static void test_sin_valor_actual() {
  AvisoCambio aviso(REGLAS);
  aviso.ponerUmbral(6000);
  TEST_ASSERT_TRUE(aviso.revisar(true, 6100, 2, 1000));
  // La ECU dejó de contestar: el aviso termina
  TEST_ASSERT_FALSE(aviso.revisar(false, 0, 0, 1500));
  TEST_ASSERT_FALSE(aviso.activo(1500));
  // Vuelve con las RPM arriba: no avisa de nuevo hasta que bajen
  TEST_ASSERT_FALSE(aviso.revisar(true, 6100, 2, 5000));
  TEST_ASSERT_FALSE(aviso.revisar(true, 5600, 2, 6000));
  TEST_ASSERT_TRUE(aviso.revisar(true, 6000, 2, 7000));
}

static void test_apagado_y_otro_umbral() {
  AvisoCambio aviso(REGLAS);
  TEST_ASSERT_EQUAL_UINT16(0, aviso.umbral());
  TEST_ASSERT_FALSE(aviso.revisar(true, 6500, 2, 1000));
  aviso.ponerUmbral(6000);
  TEST_ASSERT_TRUE(aviso.revisar(true, 6500, 2, 2000));
  // Apagarlo termina el aviso
  aviso.ponerUmbral(0);
  TEST_ASSERT_FALSE(aviso.activo(2100));
  TEST_ASSERT_FALSE(aviso.revisar(true, 6500, 2, 2150));
  // Otro umbral queda listo, aunque las RPM no hayan bajado
  aviso.ponerUmbral(3000);
  TEST_ASSERT_EQUAL_UINT16(3000, aviso.umbral());
  TEST_ASSERT_TRUE(aviso.revisar(true, 6500, 2, 2300));
}

static void test_millis_da_la_vuelta() {
  AvisoCambio aviso(REGLAS);
  aviso.ponerUmbral(6000);
  TEST_ASSERT_TRUE(aviso.revisar(true, 6000, 2, 0xFFFFFFFF - 999));
  TEST_ASSERT_TRUE(aviso.activo(500));
  TEST_ASSERT_FALSE(aviso.activo(1000));
}

static void test_umbrales_para_info() {
  // + desde apagado: el mínimo; - desde el mínimo: apagado
  TEST_ASSERT_EQUAL_UINT16(2000, otroUmbralCambio(REGLAS, 0, 1));
  TEST_ASSERT_EQUAL_UINT16(0, otroUmbralCambio(REGLAS, 2000, -1));
  TEST_ASSERT_EQUAL_UINT16(2250, otroUmbralCambio(REGLAS, 2000, 1));
  TEST_ASSERT_EQUAL_UINT16(5750, otroUmbralCambio(REGLAS, 6000, -1));
  TEST_ASSERT_EQUAL_UINT16(6250, otroUmbralCambio(REGLAS, 6000, 1));
  // En los extremos queda
  TEST_ASSERT_EQUAL_UINT16(0, otroUmbralCambio(REGLAS, 0, -1));
  TEST_ASSERT_EQUAL_UINT16(6500, otroUmbralCambio(REGLAS, 6500, 1));
  TEST_ASSERT_EQUAL_UINT16(6000, otroUmbralCambio(REGLAS, 6000, 0));

  TEST_ASSERT_TRUE(umbralCambioValido(REGLAS, 0));
  TEST_ASSERT_TRUE(umbralCambioValido(REGLAS, 2000));
  TEST_ASSERT_TRUE(umbralCambioValido(REGLAS, 2500));
  TEST_ASSERT_TRUE(umbralCambioValido(REGLAS, 6500));
  TEST_ASSERT_FALSE(umbralCambioValido(REGLAS, 1750));
  TEST_ASSERT_FALSE(umbralCambioValido(REGLAS, 2600));
  TEST_ASSERT_FALSE(umbralCambioValido(REGLAS, 6750));

  char texto[16];
  textoUmbralCambio(6000, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING("6000 rpm", texto);
  textoUmbralCambio(0, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING("Apagado", texto);
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_avisa_una_vez_al_llegar);
  RUN_TEST(test_termina_al_bajar_del_umbral);
  RUN_TEST(test_vuelve_a_avisar_cuando_bajan);
  RUN_TEST(test_la_pasada_de_los_registros);
  RUN_TEST(test_en_cada_marcha);
  RUN_TEST(test_no_avisa_sin_marcha_ni_en_quinta);
  RUN_TEST(test_sin_valor_actual);
  RUN_TEST(test_apagado_y_otro_umbral);
  RUN_TEST(test_millis_da_la_vuelta);
  RUN_TEST(test_umbrales_para_info);
  return UNITY_END();
}
