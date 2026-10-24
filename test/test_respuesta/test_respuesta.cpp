#include <unity.h>
#include <respuesta.h>

#include <stdio.h>
#include <string.h>

// Respuestas armadas con el formato del datasheet del ELM327.
// Las reales del auto se agregan en E4.

void setUp() {}

void tearDown() {}

static void verificarTipo(TipoRespuesta esperado, const char *cruda, const char *comando) {
  Respuesta r = interpretar(cruda, comando);
  TEST_ASSERT_EQUAL_STRING_MESSAGE(nombreTipo(esperado), nombreTipo(r.tipo),
                                   cruda != nullptr ? cruda : "(nullptr)");
}

template <size_t N>
static void verificarDatos(const char *cruda, const char *comando, const uint8_t (&esperados)[N]) {
  Respuesta r = interpretar(cruda, comando);
  TEST_ASSERT_EQUAL_STRING_MESSAGE("DATOS", nombreTipo(r.tipo), cruda);
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(N, r.largo, cruda);
  TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(esperados, r.datos, N, cruda);
}

static void test_texto_at() {
  verificarTipo(TipoRespuesta::TEXTO, "\r\rELM327 v1.5\r\r", "ATZ");
  verificarTipo(TipoRespuesta::TEXTO, "ATE0\rOK\r\r", "ATE0");
  verificarTipo(TipoRespuesta::TEXTO, "OK\r\r", "ATH0");
  verificarTipo(TipoRespuesta::TEXTO, "12.1V\r\r", "ATRV");
  verificarTipo(TipoRespuesta::NO_SOPORTADO, "?\r\r", "ATSP6");
  verificarTipo(TipoRespuesta::VACIA, "", "ATZ");
  verificarTipo(TipoRespuesta::VACIA, "\r\r", "ATZ");
  verificarTipo(TipoRespuesta::VACIA, "ATZ\r\r", "ATZ"); // solo el eco
}

static void test_texto_respuesta() {
  char texto[20];
  TEST_ASSERT_TRUE(textoRespuesta("\r\rELM327 v1.5\r\r", "ATZ", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("ELM327 v1.5", texto);

  TEST_ASSERT_TRUE(textoRespuesta("ATE0\rOK\r\r", "ATE0", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("OK", texto);

  TEST_ASSERT_TRUE(textoRespuesta("A6\r\r", "ATDPN", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("A6", texto);

  // Se corta si no entra
  char corto[6];
  TEST_ASSERT_TRUE(textoRespuesta("ELM327 v1.5\r", "ATI", corto, sizeof(corto)));
  TEST_ASSERT_EQUAL_STRING("ELM32", corto);

  TEST_ASSERT_FALSE(textoRespuesta("\r\r", "ATI", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("", texto);
  TEST_ASSERT_FALSE(textoRespuesta(nullptr, "ATI", texto, sizeof(texto)));
}

static void test_voltaje() {
  float v = 0;
  TEST_ASSERT_TRUE(interpretarVoltaje("12.1V\r\r", &v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.1f, v);
  TEST_ASSERT_TRUE(interpretarVoltaje("ATRV\r12.4V\r\r", &v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.4f, v);
  TEST_ASSERT_TRUE(interpretarVoltaje("0.0V", &v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, v);
  TEST_ASSERT_TRUE(interpretarVoltaje("14V", &v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.0f, v);

  const char *const malos[] = {"?", "", "\r\r", "12.4", "V", "1a.2V", "12.V", "-1.0V", "12.4V0"};
  for (const char *malo : malos) {
    TEST_ASSERT_FALSE_MESSAGE(interpretarVoltaje(malo, &v), malo);
  }
  TEST_ASSERT_FALSE(interpretarVoltaje(nullptr, &v));
}

static void test_protocolo() {
  TEST_ASSERT_EQUAL_STRING("CAN 11b 500k", nombreProtocolo("6"));
  TEST_ASSERT_EQUAL_STRING("CAN 11b 500k", nombreProtocolo("A6"));
  TEST_ASSERT_EQUAL_STRING("J1850 PWM", nombreProtocolo("1"));
  TEST_ASSERT_EQUAL_STRING("CAN 29b 250k", nombreProtocolo("A9"));
  TEST_ASSERT_EQUAL_STRING("J1939 250k", nombreProtocolo("A"));
  TEST_ASSERT_EQUAL_STRING("CAN usuario 2", nombreProtocolo("C"));

  const char *const malos[] = {"0", "A0", "", "D", "AA6", "66", "?", "a6"};
  for (const char *malo : malos) {
    TEST_ASSERT_NULL_MESSAGE(nombreProtocolo(malo), malo);
  }
  TEST_ASSERT_NULL(nombreProtocolo(nullptr));
}

static void test_un_frame() {
  const uint8_t rpm[] = {0x1A, 0xF8};
  verificarDatos("41 0C 1A F8 \r\r", "010C", rpm);
  verificarDatos("410C1AF8\r\r", "010C", rpm); // ATS0
  verificarDatos("410c1af8", "010C", rpm);
  verificarDatos("41 0C 1A F8", "010c", rpm);
  verificarDatos("41 0C 1A F8 \r\r", "01 0C", rpm);
  verificarDatos("41 0C 1A F8 \r\r", "010C1", rpm);      // sufijo de cantidad
  verificarDatos("010C\r41 0C 1A F8 \r\r", "010C", rpm); // eco
  verificarDatos("41 0C 1A F8\n", "010C", rpm);

  const uint8_t mapa[] = {0xBE, 0x3E, 0xB8, 0x11};
  verificarDatos("SEARCHING...\r41 00 BE 3E B8 11 \r\r", "0100", mapa);
  verificarDatos("SEARCHING...41 00 BE 3E B8 11", "0100", mapa);
  verificarDatos("BUS INIT: ...OK\r41 00 BE 3E B8 11\r\r", "0100", mapa);
}

static void test_modos_sin_pid() {
  const uint8_t ninguna[] = {0x00};
  verificarDatos("43 00 \r\r", "03", ninguna);
  verificarDatos("47 00\r", "07", ninguna);
  verificarDatos("4A 00\r", "0A", ninguna);

  const uint8_t dos[] = {0x02, 0x01, 0x33, 0x01, 0x34};
  verificarDatos("43 02 01 33 01 34 \r\r", "03", dos);

  verificarTipo(TipoRespuesta::INVALIDA, "47 00\r", "03"); // otro modo
}

static void test_errores() {
  struct Caso {
    const char *cruda;
    TipoRespuesta tipo;
  };
  const Caso casos[] = {
      {"NO DATA\r\r", TipoRespuesta::NO_DATA},
      {"SEARCHING...\rNO DATA\r\r", TipoRespuesta::NO_DATA},
      {"?\r\r", TipoRespuesta::NO_SOPORTADO},
      {"SEARCHING...\rUNABLE TO CONNECT\r\r", TipoRespuesta::SIN_ECU},
      {"CAN ERROR\r\r", TipoRespuesta::ERROR_BUS},
      {"BUS ERROR\r", TipoRespuesta::ERROR_BUS},
      {"BUS BUSY\r", TipoRespuesta::ERROR_BUS},
      {"FB ERROR\r", TipoRespuesta::ERROR_BUS},
      {"DATA ERROR\r", TipoRespuesta::ERROR_BUS},
      {"41 0C 1A F8 <DATA ERROR\r", TipoRespuesta::ERROR_BUS},
      {"<RX ERROR\r", TipoRespuesta::ERROR_BUS},
      {"BUS INIT: ...ERROR\r", TipoRespuesta::ERROR_BUS},
      {"ERR94\r", TipoRespuesta::ERROR_BUS},
      {"BUFFER FULL\r", TipoRespuesta::BUFFER_LLENO},
      {"STOPPED\r", TipoRespuesta::DETENIDO},
      {"LV RESET\r", TipoRespuesta::AVISO_ELM},
      {"ACT ALERT\r", TipoRespuesta::AVISO_ELM},
      {"LP ALERT\r", TipoRespuesta::AVISO_ELM},
      // Un error anula los datos que hayan llegado
      {"41 0C 1A F8\rBUFFER FULL\r", TipoRespuesta::BUFFER_LLENO},
  };
  for (const Caso &caso : casos) {
    verificarTipo(caso.tipo, caso.cruda, "010C");
  }
}

static void test_invalidas_y_vacias() {
  verificarTipo(TipoRespuesta::INVALIDA, "41 0D 32\r", "010C");     // otro PID
  verificarTipo(TipoRespuesta::INVALIDA, "42 0C 1A F8\r", "010C");  // otro modo
  verificarTipo(TipoRespuesta::INVALIDA, "7F 01 12\r", "010C");     // respuesta negativa
  verificarTipo(TipoRespuesta::INVALIDA, "41 0C 1A F\r", "010C");   // dígito suelto
  verificarTipo(TipoRespuesta::INVALIDA, "4 1 0C 1A F8\r", "010C"); // byte partido
  verificarTipo(TipoRespuesta::INVALIDA, "HOLA\r", "010C");
  verificarTipo(TipoRespuesta::INVALIDA, "41\r", "010C");
  // Con encabezados (ATH1) no se interpreta
  verificarTipo(TipoRespuesta::INVALIDA, "7E8 04 41 0C 1A F8\r", "010C");

  verificarTipo(TipoRespuesta::VACIA, "", "010C");
  verificarTipo(TipoRespuesta::VACIA, "\r\r", "010C");
  verificarTipo(TipoRespuesta::VACIA, "SEARCHING...\r\r", "0100");
  verificarTipo(TipoRespuesta::VACIA, nullptr, "010C");

  // Sin un comando válido no se puede validar el encabezado
  verificarTipo(TipoRespuesta::INVALIDA, "41 0C 1A F8\r", nullptr);
  verificarTipo(TipoRespuesta::INVALIDA, "41 0C 1A F8\r", "");
  verificarTipo(TipoRespuesta::INVALIDA, "41 0C 1A F8\r", "01");
}

static void test_varias_ecus() {
  Respuesta r = interpretar("41 00 BE 3E B8 11 \r41 00 80 00 00 01 \r\r", "0100");
  TEST_ASSERT_EQUAL_STRING("DATOS", nombreTipo(r.tipo));
  TEST_ASSERT_EQUAL_UINT8(2, r.respuestas);
  const uint8_t primera[] = {0xBE, 0x3E, 0xB8, 0x11};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(primera, r.datos, 4);

  // La primera válida, aunque antes venga otra que no corresponde
  r = interpretar("41 0D 00\r41 0C 1A F8\r", "010C");
  TEST_ASSERT_EQUAL_STRING("DATOS", nombreTipo(r.tipo));
  TEST_ASSERT_EQUAL_UINT8(1, r.respuestas);
  const uint8_t rpm[] = {0x1A, 0xF8};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(rpm, r.datos, 2);
}

static void verificarVin(const char *cruda) {
  Respuesta r = interpretar(cruda, "0902");
  TEST_ASSERT_EQUAL_STRING_MESSAGE("DATOS", nombreTipo(r.tipo), cruda);
  TEST_ASSERT_EQUAL_UINT8(18, r.largo);
  TEST_ASSERT_EQUAL_HEX8(0x01, r.datos[0]); // cantidad de VIN
  TEST_ASSERT_EQUAL_MEMORY("1D4GP00R55B123456", r.datos + 1, 17);
}

static void test_multiframe() {
  // Ejemplo del datasheet: VIN con 0902
  verificarVin(
      "014\r0: 49 02 01 31 44 34 \r1: 47 50 30 30 52 35 35 \r2: 42 31 32 33 34 35 36 \r\r");
  verificarVin("014\r0:490201314434\r1:47503030523535\r2:42313233343536\r"); // ATS0
  verificarVin(
      "0902\r014\r0: 49 02 01 31 44 34\r1: 47 50 30 30 52 35 35\r2: 42 31 32 33 34 35 36\r");

  // 03 con cuatro códigos: el último frame trae relleno
  const uint8_t codigos[] = {0x04, 0x01, 0x33, 0x01, 0x34, 0x02, 0x00, 0x03, 0x00};
  verificarDatos("00A\r0: 43 04 01 33 01 34 \r1: 02 00 03 00 00 00 00 \r\r", "03", codigos);

  // Incompleto, fuera de orden o sin la línea de cantidad
  verificarTipo(TipoRespuesta::INVALIDA, "014\r0: 49 02 01 31 44 34\r1: 47 50 30 30 52 35 35\r",
                "0902");
  verificarTipo(TipoRespuesta::INVALIDA,
                "014\r0: 49 02 01 31 44 34\r2: 42 31 32 33 34 35 36\r1: 47 50 30 30 52 35 35\r",
                "0902");
  verificarTipo(TipoRespuesta::INVALIDA, "0: 49 02 01 31 44 34\r", "0902");
}

static void test_multiframe_que_no_entra() {
  // 72 bytes: no entran en los 64 de datos
  char cruda[400] = "048\r0: 49 02 01 41 41 41\r";
  for (int i = 1; i <= 10; i++) {
    char linea[40];
    snprintf(linea, sizeof(linea), "%X: 41 41 41 41 41 41 41\r", i);
    strcat(cruda, linea);
  }
  verificarTipo(TipoRespuesta::INVALIDA, cruda, "0902");
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_texto_at);
  RUN_TEST(test_texto_respuesta);
  RUN_TEST(test_voltaje);
  RUN_TEST(test_protocolo);
  RUN_TEST(test_un_frame);
  RUN_TEST(test_modos_sin_pid);
  RUN_TEST(test_errores);
  RUN_TEST(test_invalidas_y_vacias);
  RUN_TEST(test_varias_ecus);
  RUN_TEST(test_multiframe);
  RUN_TEST(test_multiframe_que_no_entra);
  return UNITY_END();
}
