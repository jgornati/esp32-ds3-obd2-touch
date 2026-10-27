#include <reloj.h>
#include <string.h>
#include <unity.h>

void setUp() {}

void tearDown() {}

static void verificar(const FechaHora &f, uint16_t anio, uint8_t mes, uint8_t dia, uint8_t hora,
                      uint8_t minuto, uint8_t segundo) {
  TEST_ASSERT_EQUAL(anio, f.anio);
  TEST_ASSERT_EQUAL(mes, f.mes);
  TEST_ASSERT_EQUAL(dia, f.dia);
  TEST_ASSERT_EQUAL(hora, f.hora);
  TEST_ASSERT_EQUAL(minuto, f.minuto);
  TEST_ASSERT_EQUAL(segundo, f.segundo);
}

// Los registros 0x04 a 0x0A, como los da el PCF85063: martes 27/10/2026 14:35:12
static void test_leer_registros() {
  const uint8_t registros[] = {0x12, 0x35, 0x14, 0x27, 0x02, 0x10, 0x26};
  FechaHora f = {};
  TEST_ASSERT_TRUE(leerRegistrosHora(registros, &f));
  verificar(f, 2026, 10, 27, 14, 35, 12);
}

// Los bits que no son de la hora no cuentan (salvo OS)
static void test_leer_ignora_bits_sueltos() {
  const uint8_t registros[] = {0x12,        0x80 | 0x35, 0xC0 | 0x14, 0xC0 | 0x27,
                               0xF8 | 0x02, 0xE0 | 0x10, 0x26};
  FechaHora f = {};
  TEST_ASSERT_TRUE(leerRegistrosHora(registros, &f));
  verificar(f, 2026, 10, 27, 14, 35, 12);
}

// El oscilador se detuvo: la hora no sirve
static void test_leer_oscilador_parado() {
  const uint8_t registros[] = {0x80 | 0x12, 0x35, 0x14, 0x27, 0x02, 0x10, 0x26};
  FechaHora f = {2000, 1, 1, 0, 0, 0};
  TEST_ASSERT_FALSE(leerRegistrosHora(registros, &f));
  verificar(f, 2000, 1, 1, 0, 0, 0);
}

static void test_leer_invalidos() {
  FechaHora f;
  const uint8_t noBcd[] = {0x1A, 0x35, 0x14, 0x27, 0x02, 0x10, 0x26};
  TEST_ASSERT_FALSE(leerRegistrosHora(noBcd, &f));
  const uint8_t treintaDeFebrero[] = {0x00, 0x00, 0x00, 0x30, 0x00, 0x02, 0x26};
  TEST_ASSERT_FALSE(leerRegistrosHora(treintaDeFebrero, &f));
  const uint8_t hora24[] = {0x00, 0x00, 0x24, 0x01, 0x00, 0x01, 0x26};
  TEST_ASSERT_FALSE(leerRegistrosHora(hora24, &f));
  const uint8_t mes0[] = {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x26};
  TEST_ASSERT_FALSE(leerRegistrosHora(mes0, &f));
}

static void test_escribir_registros() {
  uint8_t registros[REGISTROS_HORA];
  registrosHora({2026, 10, 27, 14, 35, 12}, registros);
  const uint8_t esperados[] = {0x12, 0x35, 0x14, 0x27, 0x02, 0x10, 0x26};
  TEST_ASSERT_EQUAL_UINT8_ARRAY(esperados, registros, REGISTROS_HORA);

  // Ida y vuelta, con OS en 0
  registrosHora({2099, 12, 31, 23, 59, 59}, registros);
  TEST_ASSERT_EQUAL(0, registros[0] & 0x80);
  FechaHora f;
  TEST_ASSERT_TRUE(leerRegistrosHora(registros, &f));
  verificar(f, 2099, 12, 31, 23, 59, 59);
}

static void test_interpretar() {
  FechaHora f = {};
  TEST_ASSERT_TRUE(interpretarHora("2026-10-27 14:35", &f));
  verificar(f, 2026, 10, 27, 14, 35, 0);
  TEST_ASSERT_TRUE(interpretarHora("2026-10-27 14:35:12", &f));
  verificar(f, 2026, 10, 27, 14, 35, 12);
  TEST_ASSERT_TRUE(interpretarHora("2026-1-5 7:05  ", &f));
  verificar(f, 2026, 1, 5, 7, 5, 0);
  TEST_ASSERT_TRUE(interpretarHora("2024-02-29 00:00", &f));
  verificar(f, 2024, 2, 29, 0, 0, 0);
}

static void test_interpretar_invalidos() {
  FechaHora f = {2000, 1, 1, 0, 0, 0};
  const char *const MALOS[] = {
      "",
      "2026-10-27",
      "14:35",
      "2026-10-27 14",
      "2026/10/27 14:35",
      "27-10-2026 14:35",
      "2026-10-27 14:35 hs",
      "2026-10-27 14:35:",
      "2026-10-27 24:00",
      "2026-10-27 14:60",
      "2026-10-27 14:35:60",
      "2026-13-01 10:00",
      "2026-00-01 10:00",
      "2026-04-31 10:00",
      "2026-02-29 10:00",
      "1999-12-31 23:59",
      "2100-01-01 00:00",
      "2026-10-27 -1:00",
  };
  for (const char *texto : MALOS) {
    TEST_ASSERT_FALSE_MESSAGE(interpretarHora(texto, &f), texto);
  }
  verificar(f, 2000, 1, 1, 0, 0, 0);
}

static void test_dia_de_la_semana() {
  TEST_ASSERT_EQUAL(6, diaDeLaSemana(2000, 1, 1)); // sábado
  TEST_ASSERT_EQUAL(2, diaDeLaSemana(2026, 10, 27));
  TEST_ASSERT_EQUAL(4, diaDeLaSemana(2024, 2, 29));
  TEST_ASSERT_EQUAL(0, diaDeLaSemana(2026, 11, 1)); // domingo
}

// Contra lo que da Python (calendar.timegm)
static void test_segundos_desde_1970() {
  TEST_ASSERT_EQUAL_INT64(946684800, segundosDesde1970({2000, 1, 1, 0, 0, 0}));
  TEST_ASSERT_EQUAL_INT64(1793111712, segundosDesde1970({2026, 10, 27, 14, 35, 12}));
  TEST_ASSERT_EQUAL_INT64(1709251199, segundosDesde1970({2024, 2, 29, 23, 59, 59}));
  TEST_ASSERT_EQUAL_INT64(4102444799, segundosDesde1970({2099, 12, 31, 23, 59, 59}));
  verificar(fechaHoraDe(1793111712), 2026, 10, 27, 14, 35, 12);
  verificar(fechaHoraDe(1709251199), 2024, 2, 29, 23, 59, 59);
  verificar(fechaHoraDe(1709251200), 2024, 3, 1, 0, 0, 0);
  verificar(fechaHoraDe(0), 1970, 1, 1, 0, 0, 0);
}

// Cada día de 2000 a 2099, ida y vuelta
static void test_ida_y_vuelta() {
  int64_t segundos = segundosDesde1970({2000, 1, 1, 12, 34, 56});
  for (int dia = 0; dia < 36525; dia++, segundos += 86400) {
    FechaHora f = fechaHoraDe(segundos);
    TEST_ASSERT_TRUE(fechaValida(f));
    TEST_ASSERT_EQUAL_INT64(segundos, segundosDesde1970(f));
  }
}

static void test_textos() {
  char texto[24];
  textoHora({2026, 10, 27, 7, 5, 9}, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING("07:05", texto);
  textoFechaHora({2026, 10, 27, 7, 5, 9}, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING("2026-10-27 07:05:09", texto);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_leer_registros);
  RUN_TEST(test_leer_ignora_bits_sueltos);
  RUN_TEST(test_leer_oscilador_parado);
  RUN_TEST(test_leer_invalidos);
  RUN_TEST(test_escribir_registros);
  RUN_TEST(test_interpretar);
  RUN_TEST(test_interpretar_invalidos);
  RUN_TEST(test_dia_de_la_semana);
  RUN_TEST(test_segundos_desde_1970);
  RUN_TEST(test_ida_y_vuelta);
  RUN_TEST(test_textos);
  return UNITY_END();
}
