#include <marcha.h>
#include <unity.h>

// Las de config.h: las 5 marchas del DS3, medidas en los registros del
// 2026-10-27, las tolerancias, la velocidad mínima, dos lecturas iguales y
// VIGENCIA_MS
static const float RELACIONES[] = {8.07f, 14.57f, 20.95f, 27.24f, 33.74f};
static const ReglasMarcha REGLAS = {RELACIONES, 5, 0.05f, 0.15f, 5, 2, 3000};

// Cada lectura de la ECU, ~6 por segundo (E3)
static const uint32_t CONSULTA_MS = 150;

void setUp() {}

void tearDown() {}

// Filas de VIAJE0004.CSV y VIAJE0007.CSV: RPM y km/h de la misma consulta
static void test_marchas_de_los_registros() {
  TEST_ASSERT_EQUAL_UINT8(1, marchaDe(REGLAS, 1175, 9));
  TEST_ASSERT_EQUAL_UINT8(1, marchaDe(REGLAS, 1516, 12));
  TEST_ASSERT_EQUAL_UINT8(2, marchaDe(REGLAS, 1288, 18));
  TEST_ASSERT_EQUAL_UINT8(2, marchaDe(REGLAS, 2098, 30));
  TEST_ASSERT_EQUAL_UINT8(3, marchaDe(REGLAS, 1988, 41));
  TEST_ASSERT_EQUAL_UINT8(3, marchaDe(REGLAS, 4650, 97)); // la pasada a fondo
  TEST_ASSERT_EQUAL_UINT8(4, marchaDe(REGLAS, 2765, 75));
  TEST_ASSERT_EQUAL_UINT8(5, marchaDe(REGLAS, 1812, 60));
  TEST_ASSERT_EQUAL_UINT8(5, marchaDe(REGLAS, 1709, 58));
}

// También de los registros: entre una marcha y otra, el embrague patina
static void test_sin_marcha_entre_marchas() {
  TEST_ASSERT_EQUAL_UINT8(0, marchaDe(REGLAS, 1104, 14)); // 12,7: entre 1ª y 2ª
  TEST_ASSERT_EQUAL_UINT8(0, marchaDe(REGLAS, 1273, 15)); // 11,8
  // Embrague apretado a 80 km/h, el motor en ralentí
  TEST_ASSERT_EQUAL_UINT8(0, marchaDe(REGLAS, 850, 80));
}

static void test_sin_marcha_parado_o_sin_rpm() {
  TEST_ASSERT_EQUAL_UINT8(0, marchaDe(REGLAS, 850, 0));
  TEST_ASSERT_EQUAL_UINT8(0, marchaDe(REGLAS, 600, 4)); // debajo de la mínima
  TEST_ASSERT_EQUAL_UINT8(0, marchaDe(REGLAS, 0, 30));  // motor parado
}

static void test_bordes_de_la_tolerancia() {
  // 4ª: 27,24 km/h por 1000 rpm, ±5 %
  TEST_ASSERT_EQUAL_UINT8(4, marchaDe(REGLAS, 1000, 27.24f * 1.049f));
  TEST_ASSERT_EQUAL_UINT8(0, marchaDe(REGLAS, 1000, 27.24f * 1.051f));
  TEST_ASSERT_EQUAL_UINT8(4, marchaDe(REGLAS, 1000, 27.24f * 0.951f));
  TEST_ASSERT_EQUAL_UINT8(0, marchaDe(REGLAS, 1000, 27.24f * 0.949f));
  // 1ª, ±15 %
  TEST_ASSERT_EQUAL_UINT8(1, marchaDe(REGLAS, 1000, 8.07f * 1.149f));
  TEST_ASSERT_EQUAL_UINT8(0, marchaDe(REGLAS, 1000, 8.07f * 1.151f));
}

static void test_sin_lecturas_no_es_actual() {
  Marcha marcha(REGLAS);
  TEST_ASSERT_FALSE(marcha.actual(1000));
  TEST_ASSERT_EQUAL_UINT8(0, marcha.numero());
}

static void test_cambia_con_dos_lecturas_iguales() {
  Marcha marcha(REGLAS);
  uint32_t ahora = 1000;
  marcha.lectura(2765, 75, ahora); // 4ª, una sola vez
  TEST_ASSERT_TRUE(marcha.actual(ahora));
  TEST_ASSERT_EQUAL_UINT8(0, marcha.numero());
  ahora += CONSULTA_MS;
  marcha.lectura(2728, 74, ahora);
  TEST_ASSERT_EQUAL_UINT8(4, marcha.numero());
  // Una lectura suelta de otra cosa (pisando el embrague) no la cambia
  ahora += CONSULTA_MS;
  marcha.lectura(1500, 74, ahora);
  TEST_ASSERT_EQUAL_UINT8(4, marcha.numero());
  ahora += CONSULTA_MS;
  marcha.lectura(2700, 74, ahora);
  TEST_ASSERT_EQUAL_UINT8(4, marcha.numero());
  // Pasa a 5ª: dos lecturas
  ahora += CONSULTA_MS;
  marcha.lectura(2200, 74, ahora);
  TEST_ASSERT_EQUAL_UINT8(4, marcha.numero());
  ahora += CONSULTA_MS;
  marcha.lectura(2190, 74, ahora);
  TEST_ASSERT_EQUAL_UINT8(5, marcha.numero());
}

static void test_embrague_apretado_queda_sin_marcha() {
  Marcha marcha(REGLAS);
  uint32_t ahora = 1000;
  marcha.lectura(1988, 41, ahora);
  marcha.lectura(2000, 41, ahora += CONSULTA_MS);
  TEST_ASSERT_EQUAL_UINT8(3, marcha.numero());
  marcha.lectura(900, 40, ahora += CONSULTA_MS);
  marcha.lectura(880, 39, ahora += CONSULTA_MS);
  TEST_ASSERT_EQUAL_UINT8(0, marcha.numero());
}

static void test_se_vence_y_empieza_de_cero() {
  Marcha marcha(REGLAS);
  uint32_t ahora = 1000;
  marcha.lectura(1812, 60, ahora);
  marcha.lectura(1800, 60, ahora += CONSULTA_MS);
  TEST_ASSERT_EQUAL_UINT8(5, marcha.numero());
  TEST_ASSERT_TRUE(marcha.actual(ahora + 3000));
  TEST_ASSERT_FALSE(marcha.actual(ahora + 3001));
  // Vuelven los datos en otra marcha: no muestra la vieja mientras confirma
  ahora += 5000;
  marcha.lectura(2765, 75, ahora);
  TEST_ASSERT_TRUE(marcha.actual(ahora));
  TEST_ASSERT_EQUAL_UINT8(0, marcha.numero());
  marcha.lectura(2760, 75, ahora += CONSULTA_MS);
  TEST_ASSERT_EQUAL_UINT8(4, marcha.numero());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_marchas_de_los_registros);
  RUN_TEST(test_sin_marcha_entre_marchas);
  RUN_TEST(test_sin_marcha_parado_o_sin_rpm);
  RUN_TEST(test_bordes_de_la_tolerancia);
  RUN_TEST(test_sin_lecturas_no_es_actual);
  RUN_TEST(test_cambia_con_dos_lecturas_iguales);
  RUN_TEST(test_embrague_apretado_queda_sin_marcha);
  RUN_TEST(test_se_vence_y_empieza_de_cero);
  return UNITY_END();
}
