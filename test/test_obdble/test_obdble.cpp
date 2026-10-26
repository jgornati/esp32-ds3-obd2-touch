#include <unity.h>
#include <pids.h>
#include <respuesta.h>

// El adaptador nuevo (OBDBLE, "ELM327 v2.1"). Antes de ATL0 las líneas
// terminan en \r\n y después, en \r (las de datos, en \r\r); tras ATZ el
// eco queda prendido. Copiadas de la prueba de banco de E3 y de
// docs/descubrimiento/2026-10-24.log (en el DS3), salvo donde se indica.

void setUp() {}

void tearDown() {}

static void test_textos_at() {
  char texto[40];
  TEST_ASSERT_TRUE(textoRespuesta("ELM327 v2.1\r\n", "ATZ", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("ELM327 v2.1", texto);
  // Con eco: el ELM recién prendido (log de E2)
  TEST_ASSERT_TRUE(textoRespuesta("ATZ\rELM327 v2.1\r\n", "ATZ", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("ELM327 v2.1", texto);
  TEST_ASSERT_TRUE(textoRespuesta("ATE0\rOK\r\n", "ATE0", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("OK", texto);
  TEST_ASSERT_TRUE(textoRespuesta("OK\r", "ATSP6", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("OK", texto);
  TEST_ASSERT_TRUE(textoRespuesta("ELM327 v2.1\r", "ATI", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("ELM327 v2.1", texto);

  float v = 0;
  TEST_ASSERT_TRUE(interpretarVoltaje("12.0V\r", &v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 12.0f, v);
}

static void test_sin_ecu() {
  // Con ATSP6, en ~133 ms
  Respuesta r = interpretar("UNABLE TO CONNECT\r", "0100");
  TEST_ASSERT_EQUAL_STRING("SIN_ECU", nombreTipo(r.tipo));
  // Con ATSP0, en 4,4 s (armada con lo anotado en E0)
  r = interpretar("SEARCHING...\r\nUNABLE TO CONNECT\r\n", "0100");
  TEST_ASSERT_EQUAL_STRING("SIN_ECU", nombreTipo(r.tipo));
}

static void test_en_el_auto() {
  const uint8_t mapa00[] = {0xBE, 0x1E, 0xA8, 0x11};
  Respuesta r = interpretar("4100BE1EA811\r\r", "0100");
  TEST_ASSERT_EQUAL_STRING("DATOS", nombreTipo(r.tipo));
  TEST_ASSERT_EQUAL_UINT8(1, r.respuestas);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(mapa00, r.datos, 4);

  // Una vez, recién encontrada la ECU (en el anterior contestaba 412080001001)
  TEST_ASSERT_EQUAL_STRING("NO_DATA", nombreTipo(interpretar("NO DATA\r", "0120").tipo));

  char texto[8];
  TEST_ASSERT_TRUE(textoRespuesta("6\r", "ATDPN", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("6", texto);
  TEST_ASSERT_EQUAL_STRING("CAN 11b 500k", nombreProtocolo(texto));
}

// !descubrir del 2026-10-25 (docs/descubrimiento/2026-10-25.log)
static void test_descubrir() {
  char texto[40];
  TEST_ASSERT_TRUE(textoRespuesta("OBDII to RS232 Interpreter\r", "AT@1", texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING("OBDII to RS232 Interpreter", texto);

  // Con ATH1 este adaptador muestra el byte de relleno del frame (AA); el
  // anterior no. El parser no interpreta encabezados.
  TEST_ASSERT_EQUAL_STRING("INVALIDA",
                           nombreTipo(interpretar("7E8064100BE1EA811AA\r\r", "0100").tipo));

  // Los mismos mapas que el anterior: 17 PIDs
  PidsSoportados soportados;
  Respuesta r = interpretar("4100BE1EA811\r\r", "0100");
  TEST_ASSERT_TRUE(soportados.cargar(0x00, r.datos, r.largo));
  r = interpretar("412080001001\r\r", "0120");
  TEST_ASSERT_TRUE(soportados.cargar(0x20, r.datos, r.largo));
  r = interpretar("414002000000\r\r", "0140");
  TEST_ASSERT_TRUE(soportados.cargar(0x40, r.datos, r.largo));
  TEST_ASSERT_EQUAL_UINT8(17, soportados.cantidad());
  TEST_ASSERT_EQUAL_STRING("NO_DATA", nombreTipo(interpretar("NO DATA\r", "0160").tipo));

  // En ralentí, con el motor frío
  float v = 0;
  r = interpretar("410C1202\r\r", "010C");
  TEST_ASSERT_TRUE(calcularPid(0x0C, r.datos, r.largo, &v));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1152.5f, v);
  r = interpretar("41053D\r\r", "0105");
  TEST_ASSERT_TRUE(calcularPid(0x05, r.datos, r.largo, &v));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 21, v);
  r = interpretar("41347F687FFB\r\r", "0134");
  TEST_ASSERT_TRUE(calcularPid(0x34, r.datos, r.largo, &v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.9954f, v);
  r = interpretar("41030100\r\r", "0103");
  TEST_ASSERT_EQUAL_STRING("Abierto frio", textoLazo(r.datos[0]));

  // Uno de los NO DATA sueltos, en medio de 010C1 que contestaban
  TEST_ASSERT_EQUAL_STRING("NO_DATA", nombreTipo(interpretar("NO DATA\r", "010C1").tipo));
}

// Pedidos de varios PIDs, con contacto (docs/descubrimiento/2026-10-25-varios-pids.log).
// Más de 7 bytes vienen en varios frames; este adaptador no muestra el
// relleno del último.
static void test_varios_pids() {
  DatoPid pids[6] = {};
  float v = 0;

  Respuesta r = interpretar("00A\r0:410C00000543\r1:0D000F41\r\r", "010C050D0F");
  TEST_ASSERT_EQUAL_STRING("DATOS", nombreTipo(r.tipo));
  TEST_ASSERT_EQUAL_UINT8(4, separarPids(r.datos, r.largo, pids, 6));
  TEST_ASSERT_EQUAL_HEX8(0x05, pids[1].pid);
  TEST_ASSERT_TRUE(calcularPid(0x05, pids[1].datos, pids[1].largo, &v));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 27, v);
  TEST_ASSERT_TRUE(calcularPid(0x0F, pids[3].datos, pids[3].largo, &v));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 25, v);

  r = interpretar("00D\r0:410C00000543\r1:0D000F41155BFF\r\r", "010C050D0F15");
  TEST_ASSERT_EQUAL_UINT8(5, separarPids(r.datos, r.largo, pids, 6));
  TEST_ASSERT_TRUE(calcularPid(0x15, pids[4].datos, pids[4].largo, &v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.455f, v);

  r = interpretar("00E\r0:410C00000543\r1:0D000F42068007\r2:80\r\r", "010C050D0F0607");
  TEST_ASSERT_EQUAL_UINT8(6, separarPids(r.datos, r.largo, pids, 6));
  TEST_ASSERT_EQUAL_HEX8(0x07, pids[5].pid);

  // Lazo (2 bytes) y lambda (4): tres frames
  r = interpretar("012\r0:410C00000543\r1:0D000F41030000\r2:3480008000\r\r", "010C050D0F0334");
  TEST_ASSERT_EQUAL_UINT8(6, separarPids(r.datos, r.largo, pids, 6));
  TEST_ASSERT_EQUAL_HEX8(0x34, pids[5].pid);
  TEST_ASSERT_TRUE(calcularPid(0x34, pids[5].datos, pids[5].largo, &v));
  TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, v);
}

// Respuestas del DS3 con \r\n (armadas): si el ELM se reinicia solo, vuelve
// a los saltos de línea hasta que se lo inicializa de nuevo.
static void test_datos_con_crlf() {
  const uint8_t mapa00[] = {0xBE, 0x1E, 0xA8, 0x11};
  Respuesta r = interpretar("4100BE1EA811\r\n", "0100");
  TEST_ASSERT_EQUAL_STRING("DATOS", nombreTipo(r.tipo));
  TEST_ASSERT_EQUAL_UINT8(4, r.largo);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(mapa00, r.datos, 4);

  r = interpretar("410C0B74\r\n", "010C1");
  TEST_ASSERT_EQUAL_STRING("DATOS", nombreTipo(r.tipo));
  float rpm = 0;
  TEST_ASSERT_TRUE(calcularPid(0x0C, r.datos, r.largo, &rpm));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 733, rpm);

  TEST_ASSERT_EQUAL_STRING("NO_DATA", nombreTipo(interpretar("NO DATA\r\n", "01421").tipo));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_textos_at);
  RUN_TEST(test_sin_ecu);
  RUN_TEST(test_en_el_auto);
  RUN_TEST(test_descubrir);
  RUN_TEST(test_varios_pids);
  RUN_TEST(test_datos_con_crlf);
  return UNITY_END();
}
