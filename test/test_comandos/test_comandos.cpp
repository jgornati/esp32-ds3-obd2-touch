#include <unity.h>
#include <comandos.h>

#include <string.h>

void setUp() {}

void tearDown() {}

template <size_t N>
static void aceptados(const char *const (&casos)[N]) {
  for (const char *caso : casos) {
    TEST_ASSERT_TRUE_MESSAGE(comandoPermitido(caso), caso);
  }
}

template <size_t N>
static void rechazados(const char *const (&casos)[N]) {
  for (const char *caso : casos) {
    TEST_ASSERT_FALSE_MESSAGE(comandoPermitido(caso), caso);
  }
}

static void test_at_de_la_lista() {
  const char *const casos[] = {
      "ATZ",  "ATWS", "ATD",  "ATI",  "AT@1", "ATRV",  "ATDP",  "ATDPN", "ATE0",  "ATE1",  "ATL0",
      "ATL1", "ATS0", "ATS1", "ATH0", "ATH1", "ATSP0", "ATSP6", "ATAT0", "ATAT1", "ATAT2",
  };
  aceptados(casos);
}

static void test_at_minusculas_y_espacios() {
  const char *const casos[] = {
      "atz",  "at z",    "AT Z",   " ATZ ",   "AtRv", " at rv ",     "a t d p n",
      "at@1", "AT SP 0", "at sp6", "AT AT 1", "ate0", "  A T   Z  ",
  };
  aceptados(casos);
}

static void test_atst() {
  const char *const buenos[] = {"ATST32", "ATST00", "ATSTFF", "atst0a", "AT ST 19"};
  aceptados(buenos);

  const char *const malos[] = {"ATST", "ATST3", "ATST123", "ATSTG0", "ATST0X", "ATST 3"};
  rechazados(malos);
}

static void test_at_fuera_de_la_lista() {
  // Los que el plan nombra explícitamente
  const char *const delPlan[] = {
      "ATPP", "ATPP0CSV",     "ATPPS", "ATSH",    "ATSH7E0", "ATMA",
      "ATWM", "ATWMC133F13E", "ATBRD", "ATBRD23", "atbrd23",
  };
  rechazados(delPlan);

  // Escriben en el ELM, cambian el bus o no son de la lista
  const char *const otros[] = {
      "AT@2", "AT@3", "AT@3ABC", "ATSP1",  "ATSP7",  "ATSPA6", "ATSP",     "ATTP6",
      "ATE2", "ATI0", "ATZ1",    "ATRV0",  "ATDPNN", "ATAT3",  "ATCRA7E8", "ATCAF0",
      "ATLP", "ATPC", "ATMR00",  "ATSW00", "ATBI",   "AT",     "A",        "ATT",
  };
  rechazados(otros);
}

static void test_obd_permitidos() {
  const char *const casos[] = {
      "0100",    "0120",  "0140",  "0160",    "0101", "010C",  "010c", "01 0C",
      " 01 0d ", "010C1", "010cf", "01 0C 1", "0142", "0900",  "0902", "09021",
      "09 02",   "03",    "07",    "0A",      "0a",   " 0 3 ",
  };
  aceptados(casos);
}

static void test_borrar_fallas() {
  const char *const casos[] = {"04", " 04", "0 4", "04 ", " 0 4 ", "0400", "04\r"};
  rechazados(casos);
}

static void test_obd_rechazados() {
  const char *const otrosModos[] = {
      "02", "020C00", "05", "0500", "06", "0600", "08", "0800", "0300", "0700", "0A00", "0900AB",
  };
  rechazados(otrosModos);

  const char *const malFormados[] = {
      "0", "01", "09", "010", "090", "01GG", "010X", "01 0", "0G",
  };
  rechazados(malFormados);

  const char *const variosPids[] = {
      "010104", "01 01 04", "010C0D", "0101040", "0100000000",
  };
  rechazados(variosPids);
}

static void test_uds_rechazados() {
  const char *const casos[] = {
      "10",   "1001", "1003",   "11", "1101", "14",   "14FFFFFF", "19", "1902FF", "22F190", "27",
      "2701", "2E",   "2EF190", "2F", "31",   "3101", "34",       "36", "3E",     "3E00",   "85",
  };
  rechazados(casos);
}

static void test_vacios_y_control() {
  TEST_ASSERT_FALSE(comandoPermitido(nullptr));

  const char *const casos[] = {
      "",
      " ",
      "     ",
      "\r",
      "\n",
      "\r\n",
      "ATZ\r",
      "ATZ\r04",
      "03\n04",
      "0100\r",
      "\t03",
      "AT\tZ",
      "\x1B",
      "0\x7F"
      "3",
      "\xC1TZ",
      "0\xA0"
      "3",
  };
  rechazados(casos);
}

static void test_largo() {
  // Los espacios no cuentan para el largo
  TEST_ASSERT_TRUE(comandoPermitido("        0    1    0    C        "));

  char largo[200];
  memset(largo, '0', sizeof(largo) - 1);
  largo[sizeof(largo) - 1] = '\0';
  TEST_ASSERT_FALSE(comandoPermitido(largo));

  // Un comando permitido seguido de basura larga
  TEST_ASSERT_FALSE(comandoPermitido("ATZ0000000000000000000000"));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_at_de_la_lista);
  RUN_TEST(test_at_minusculas_y_espacios);
  RUN_TEST(test_atst);
  RUN_TEST(test_at_fuera_de_la_lista);
  RUN_TEST(test_obd_permitidos);
  RUN_TEST(test_borrar_fallas);
  RUN_TEST(test_obd_rechazados);
  RUN_TEST(test_uds_rechazados);
  RUN_TEST(test_vacios_y_control);
  RUN_TEST(test_largo);
  return UNITY_END();
}
