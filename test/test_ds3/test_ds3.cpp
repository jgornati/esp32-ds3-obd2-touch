#include <unity.h>
#include <dtc.h>
#include <pids.h>
#include <respuesta.h>

// Respuestas reales del DS3 1.6 VTi, copiadas tal cual de
// docs/descubrimiento/2026-10-23.log (ELM327 v1.5 con ATE0 ATL0 ATS0 ATH0).

void setUp() {}

void tearDown() {}

static Respuesta datos(const char *cruda, const char *comando) {
  Respuesta r = interpretar(cruda, comando);
  TEST_ASSERT_EQUAL_STRING_MESSAGE("DATOS", nombreTipo(r.tipo), cruda);
  TEST_ASSERT_EQUAL_UINT8_MESSAGE(1, r.respuestas, cruda); // contesta solo 7E8
  return r;
}

static float valor(const char *cruda, const char *comando, uint8_t pid) {
  Respuesta r = datos(cruda, comando);
  float v = 0;
  TEST_ASSERT_TRUE_MESSAGE(calcularPid(pid, r.datos, r.largo, &v), cruda);
  return v;
}

static void test_textos_at() {
  char texto[40];
  TEST_ASSERT_TRUE(textoRespuesta("\r\rELM327 v1.5\r\r", "ATZ", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("ELM327 v1.5", texto);
  TEST_ASSERT_TRUE(textoRespuesta("ATE0\rOK\r\r", "ATE0", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("OK", texto);
  TEST_ASSERT_TRUE(textoRespuesta("OBDII to RS232 Interpreter\r\r", "AT@1", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("OBDII to RS232 Interpreter", texto);
  TEST_ASSERT_TRUE(textoRespuesta("A6\r\r", "ATDPN", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("A6", texto);
  TEST_ASSERT_EQUAL_STRING("CAN 11b 500k", nombreProtocolo(texto));

  float v = 0;
  TEST_ASSERT_TRUE(interpretarVoltaje("12.4V\r\r", &v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.4f, v);
  TEST_ASSERT_TRUE(interpretarVoltaje("14.1V\r\r", &v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 14.1f, v);
}

static void test_mapas_soportados() {
  const uint8_t mapa00[] = {0xBE, 0x1E, 0xA8, 0x11};
  Respuesta r = datos("SEARCHING...\r4100BE1EA811\r\r", "0100");
  TEST_ASSERT_EQUAL_HEX8_ARRAY(mapa00, r.datos, 4);
  r = datos("4100BE1EA811\r\r", "0100");
  TEST_ASSERT_EQUAL_HEX8_ARRAY(mapa00, r.datos, 4);

  PidsSoportados soportados;
  TEST_ASSERT_TRUE(soportados.cargar(0x00, r.datos, r.largo));
  r = datos("412080001001\r\r", "0120");
  TEST_ASSERT_TRUE(soportados.cargar(0x20, r.datos, r.largo));
  r = datos("414002000000\r\r", "0140");
  TEST_ASSERT_TRUE(soportados.cargar(0x40, r.datos, r.largo));

  const uint8_t si[] = {0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E,
                        0x0F, 0x11, 0x13, 0x15, 0x1C, 0x21, 0x34, 0x47};
  const uint8_t no[] = {0x02, 0x0B, 0x10, 0x14, 0x24, 0x42, 0x44, 0x60};
  for (uint8_t pid : si) {
    TEST_ASSERT_TRUE(soportados.soportado(pid));
  }
  for (uint8_t pid : no) {
    TEST_ASSERT_FALSE(soportados.soportado(pid));
  }
  TEST_ASSERT_EQUAL_UINT8(17, soportados.cantidad());
}

static void test_no_soportados() {
  const char *const comandos[] = {"0160", "0114", "0124", "0142", "0144", "0A"};
  for (const char *comando : comandos) {
    Respuesta r = interpretar("NO DATA\r\r", comando);
    TEST_ASSERT_EQUAL_STRING_MESSAGE("NO_DATA", nombreTipo(r.tipo), comando);
  }
}

static void test_con_encabezados() {
  // ATH1: el ID 7E8 delante. El parser no lo interpreta (se analiza en crudo).
  Respuesta r = interpretar("7E8064100BE1EA811\r\r", "0100");
  TEST_ASSERT_EQUAL_STRING("INVALIDA", nombreTipo(r.tipo));
}

static void test_valores_con_contacto() {
  Respuesta r = datos("410100076100\r\r", "0101");
  bool encendida = true;
  uint8_t cantidad = 99;
  TEST_ASSERT_TRUE(luzFalla(r.datos, r.largo, &encendida, &cantidad));
  TEST_ASSERT_FALSE(encendida);
  TEST_ASSERT_EQUAL_UINT8(0, cantidad);

  r = datos("41030000\r\r", "0103");
  TEST_ASSERT_NULL(textoLazo(r.datos[0])); // motor apagado

  TEST_ASSERT_FLOAT_WITHIN(0.01f, 65, valor("410569\r\r", "0105", 0x05));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, valor("410680\r\r", "0106", 0x06));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, valor("410780\r\r", "0107", 0x07));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, valor("410C0000\r\r", "010C", 0x0C));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0, valor("410D00\r\r", "010D", 0x0D));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 53, valor("410F5D\r\r", "010F", 0x0F));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, valor("413480008000\r\r", "0134", 0x34));

  r = datos("411303\r\r", "0113");
  TEST_ASSERT_TRUE(sondaPresente(r.datos[0], 1, 1));
  TEST_ASSERT_TRUE(sondaPresente(r.datos[0], 1, 2));
  TEST_ASSERT_FALSE(sondaPresente(r.datos[0], 2, 1));
}

static void test_valores_en_ralenti() {
  Respuesta r = datos("41030100\r\r", "0103");
  TEST_ASSERT_EQUAL_STRING("Abierto frio", textoLazo(r.datos[0]));

  TEST_ASSERT_FLOAT_WITHIN(0.01f, 66, valor("41056A\r\r", "0105", 0x05));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 733, valor("410C0B74\r\r", "010C", 0x0C));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 733, valor("410C0B74\r\r", "010C1", 0x0C));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50, valor("410F5A\r\r", "010F", 0x0F));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.455f, valor("41155BFF\r\r", "0115", 0x15));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.9949f, valor("41347F587FFB\r\r", "0134", 0x34));
}

static void test_fallas_y_modo_09() {
  ListaDtc lista = {};
  Respuesta r = datos("4300\r\r", "03");
  TEST_ASSERT_TRUE(interpretarDtcs(r.datos, r.largo, &lista));
  TEST_ASSERT_EQUAL_UINT8(0, lista.cantidad);
  r = datos("4700\r\r", "07");
  TEST_ASSERT_TRUE(interpretarDtcs(r.datos, r.largo, &lista));
  TEST_ASSERT_EQUAL_UINT8(0, lista.cantidad);

  // La lectura de la página Fallas con esas respuestas
  LecturaFallas fallas;
  fallas.leida = true;
  fallas.guardados = interpretarCodigos("4300\r\r", "03", true);
  fallas.pendientes = interpretarCodigos("4700\r\r", "07", true);
  fallas.permanentes = interpretarCodigos("NO DATA\r\r", "0A", true);
  TEST_ASSERT_TRUE(sinFallas(fallas));
  TEST_ASSERT_FALSE(errorAlLeer(fallas));
  TEST_ASSERT_EQUAL_INT((int)VistaFallas::SIN_FALLAS, (int)pantallaFallas(fallas, 0).vista);

  // 0101 (con el sufijo 1, como lo pide la página): luz apagada, 0 códigos
  r = datos("410100076100\r\r", "01011");
  bool luz = true;
  uint8_t cantidad = 99;
  TEST_ASSERT_TRUE(luzFalla(r.datos, r.largo, &luz, &cantidad));
  TEST_ASSERT_FALSE(luz);
  TEST_ASSERT_EQUAL_UINT8(0, cantidad);

  // 0900: soporta 0904 y 0908; el VIN (0902) no
  const uint8_t mapa09[] = {0x11, 0x00, 0x00, 0x00};
  r = datos("490011000000\r\r", "0900");
  TEST_ASSERT_EQUAL_HEX8_ARRAY(mapa09, r.datos, 4);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_textos_at);
  RUN_TEST(test_mapas_soportados);
  RUN_TEST(test_no_soportados);
  RUN_TEST(test_con_encabezados);
  RUN_TEST(test_valores_con_contacto);
  RUN_TEST(test_valores_en_ralenti);
  RUN_TEST(test_fallas_y_modo_09);
  return UNITY_END();
}
