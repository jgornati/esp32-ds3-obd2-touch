#include <unity.h>
#include <dtc.h>
#include <respuesta.h>

#include <string.h>

void setUp() {}

void tearDown() {}

static void verificarTexto(uint16_t codigo, const char *esperado) {
  char texto[6];
  textoDtc(codigo, texto);
  TEST_ASSERT_EQUAL_STRING(esperado, texto);
}

static void test_texto() {
  verificarTexto(0x0133, "P0133");
  verificarTexto(0x0000, "P0000");
  verificarTexto(0x2A00, "P2A00");
  verificarTexto(0x4123, "C0123");
  verificarTexto(0x8100, "B0100");
  verificarTexto(0xC155, "U0155");
  verificarTexto(0xD001, "U1001");
  verificarTexto(0xFFFF, "U3FFF");
}

static void test_lista() {
  ListaDtc lista = {};
  const uint8_t ninguno[] = {0x00};
  TEST_ASSERT_TRUE(interpretarDtcs(ninguno, 1, &lista));
  TEST_ASSERT_EQUAL_UINT8(0, lista.cantidad);

  const uint8_t dos[] = {0x02, 0x01, 0x33, 0x01, 0x34};
  TEST_ASSERT_TRUE(interpretarDtcs(dos, 5, &lista));
  TEST_ASSERT_EQUAL_UINT8(2, lista.cantidad);
  TEST_ASSERT_EQUAL_HEX16(0x0133, lista.codigos[0]);
  TEST_ASSERT_EQUAL_HEX16(0x0134, lista.codigos[1]);

  // Relleno de más al final: se ignora
  const uint8_t relleno[] = {0x01, 0x01, 0x33, 0x00, 0x00};
  TEST_ASSERT_TRUE(interpretarDtcs(relleno, 5, &lista));
  TEST_ASSERT_EQUAL_UINT8(1, lista.cantidad);
}

static void test_lista_invalida() {
  ListaDtc lista = {};
  const uint8_t faltan[] = {0x02, 0x01, 0x33};
  TEST_ASSERT_FALSE(interpretarDtcs(faltan, 3, &lista));
  TEST_ASSERT_FALSE(interpretarDtcs(faltan, 0, &lista));
  TEST_ASSERT_FALSE(interpretarDtcs(nullptr, 3, &lista));

  // Más códigos de los que entran
  uint8_t muchos[64] = {MAX_DTC + 1};
  TEST_ASSERT_FALSE(interpretarDtcs(muchos, sizeof(muchos), &lista));
}

static void test_desde_respuesta() {
  ListaDtc lista = {};
  Respuesta r = interpretar("43 02 01 33 01 34 \r\r", "03");
  TEST_ASSERT_TRUE(interpretarDtcs(r.datos, r.largo, &lista));
  TEST_ASSERT_EQUAL_UINT8(2, lista.cantidad);
  char texto[6];
  textoDtc(lista.codigos[0], texto);
  TEST_ASSERT_EQUAL_STRING("P0133", texto);
  textoDtc(lista.codigos[1], texto);
  TEST_ASSERT_EQUAL_STRING("P0134", texto);

  r = interpretar("00A\r0: 43 04 01 33 01 34 \r1: 02 00 03 00 00 00 00 \r\r", "03");
  TEST_ASSERT_TRUE(interpretarDtcs(r.datos, r.largo, &lista));
  TEST_ASSERT_EQUAL_UINT8(4, lista.cantidad);
  textoDtc(lista.codigos[3], texto);
  TEST_ASSERT_EQUAL_STRING("P0300", texto);
}

static void verificarEstado(EstadoCodigos esperado, const char *cruda, const char *comando,
                            bool completa) {
  Codigos c = interpretarCodigos(cruda, comando, completa);
  TEST_ASSERT_EQUAL_INT_MESSAGE((int)esperado, (int)c.estado, cruda);
}

static void test_codigos() {
  Codigos c = interpretarCodigos("43 02 01 33 04 20 \r\r", "03", true);
  TEST_ASSERT_EQUAL_INT((int)EstadoCodigos::LEIDOS, (int)c.estado);
  TEST_ASSERT_EQUAL_UINT8(2, c.lista.cantidad);
  TEST_ASSERT_EQUAL_HEX16(0x0420, c.lista.codigos[1]);

  verificarEstado(EstadoCodigos::LEIDOS, "4A00\r\r", "0A", true);
  // NO DATA: en 0A es que no hay permanentes; en 03 y 07, un error
  verificarEstado(EstadoCodigos::NO_DISPONIBLE, "NO DATA\r\r", "0A", true);
  verificarEstado(EstadoCodigos::ERROR, "NO DATA\r\r", "03", true);
  verificarEstado(EstadoCodigos::ERROR, "NO DATA\r\r", "07", true);
  // Sin el '>' no se confía, aunque hayan llegado datos
  verificarEstado(EstadoCodigos::ERROR, "4300\r\r", "03", false);
  verificarEstado(EstadoCodigos::ERROR, "", "0A", false);
  verificarEstado(EstadoCodigos::ERROR, "CAN ERROR\r\r", "07", true);
  verificarEstado(EstadoCodigos::ERROR, "43 02 01 33\r\r", "03", true); // faltan bytes
  verificarEstado(EstadoCodigos::ERROR, "4700\r\r", "03", true);        // otro servicio

  c = interpretarCodigos("CAN ERROR\r\r", "07", true);
  TEST_ASSERT_EQUAL_STRING("ERROR_BUS", nombreTipo(c.tipo));
}

// Lectura completa con las respuestas crudas de 03, 07 y 0A
static LecturaFallas lectura(const char *guardados, const char *pendientes,
                             const char *permanentes) {
  LecturaFallas l;
  l.leida = true;
  l.guardados = interpretarCodigos(guardados, "03", true);
  l.pendientes = interpretarCodigos(pendientes, "07", true);
  l.permanentes = interpretarCodigos(permanentes, "0A", true);
  return l;
}

static void test_sin_fallas() {
  TEST_ASSERT_FALSE(sinFallas(LecturaFallas()));
  TEST_ASSERT_TRUE(sinFallas(lectura("4300", "4700", "NO DATA")));
  TEST_ASSERT_TRUE(sinFallas(lectura("4300", "4700", "4A00")));
  // Un error en 0A no lo impide; uno en 03 o 07, sí
  TEST_ASSERT_TRUE(sinFallas(lectura("4300", "4700", "CAN ERROR")));
  TEST_ASSERT_FALSE(sinFallas(lectura("NO DATA", "4700", "NO DATA")));
  TEST_ASSERT_FALSE(sinFallas(lectura("4300", "CAN ERROR", "NO DATA")));
  // Cualquier código, de cualquier tipo
  TEST_ASSERT_FALSE(sinFallas(lectura("43 01 01 33", "4700", "NO DATA")));
  TEST_ASSERT_FALSE(sinFallas(lectura("4300", "47 01 01 33", "NO DATA")));
  TEST_ASSERT_FALSE(sinFallas(lectura("4300", "4700", "4A 01 04 20")));
}

static void test_luz() {
  LecturaFallas l;
  TEST_ASSERT_EQUAL_STRING("...", textoLuz(l));
  l.leida = true;
  TEST_ASSERT_EQUAL_STRING("--", textoLuz(l));
  l.luzLeida = true;
  TEST_ASSERT_EQUAL_STRING("Apagada", textoLuz(l));
  l.luz = true;
  TEST_ASSERT_EQUAL_STRING("Encendida", textoLuz(l));
}

// Compara la pantalla número vuelta con los renglones esperados
static void verificarPantalla(const LecturaFallas &l, uint32_t vuelta, uint8_t total,
                              const char *const *esperados, uint8_t cantidad) {
  PantallaFallas p = {};
  TEST_ASSERT_EQUAL_UINT8(total, pantallaFallas(l, vuelta, &p));
  TEST_ASSERT_EQUAL_UINT8(cantidad, p.cantidad);
  for (uint8_t i = 0; i < cantidad; i++) {
    TEST_ASSERT_EQUAL_STRING(esperados[i], p.renglones[i]);
  }
}

static void test_pantallas_de_estado() {
  const char *const leyendo[] = {"Leyendo..."};
  verificarPantalla(LecturaFallas(), 0, 1, leyendo, 1);

  const char *const ninguna[] = {"Sin fallas"};
  verificarPantalla(lectura("4300", "4700", "NO DATA"), 7, 1, ninguna, 1);

  // Con un error, aunque el otro servicio tenga códigos
  const char *const error[] = {"Error al leer", "los codigos"};
  verificarPantalla(lectura("43 01 01 33", "NO DATA", "NO DATA"), 0, 1, error, 2);
  LecturaFallas cortada = lectura("4300", "4700", "NO DATA");
  cortada.pendientes = interpretarCodigos("4700", "07", false);
  verificarPantalla(cortada, 0, 1, error, 2);
}

static void test_descripcion() {
  TEST_ASSERT_EQUAL_STRING("Catalizador", descripcionDtc(0x0420));
  TEST_ASSERT_EQUAL_STRING("Mezcla pobre", descripcionDtc(0x0171));
  TEST_ASSERT_EQUAL_STRING("Falla cil. 2", descripcionDtc(0x0302));
  TEST_ASSERT_EQUAL_STRING("Distribucion", descripcionDtc(0x0016));
  TEST_ASSERT_EQUAL_STRING("Pedal acelerad", descripcionDtc(0x2138));
  // Los extremos de un rango y el código siguiente
  TEST_ASSERT_EQUAL_STRING("Sonda lambda 1", descripcionDtc(0x0130));
  TEST_ASSERT_EQUAL_STRING("Sonda lambda 1", descripcionDtc(0x0134));
  TEST_ASSERT_EQUAL_STRING("Calef. sonda 1", descripcionDtc(0x0135));
  // Propios de la marca y de otros sistemas: sin descripción
  TEST_ASSERT_NULL(descripcionDtc(0x1234)); // P1234
  TEST_ASSERT_NULL(descripcionDtc(0x0000)); // P0000
  TEST_ASSERT_NULL(descripcionDtc(0x4420)); // C0420
  TEST_ASSERT_NULL(descripcionDtc(0xC155)); // U0155

  // Todas entran en un renglón de la pantalla
  for (uint32_t codigo = 0; codigo <= 0xFFFF; codigo++) {
    const char *descripcion = descripcionDtc((uint16_t)codigo);
    if (descripcion != nullptr) {
      TEST_ASSERT_TRUE_MESSAGE(strlen(descripcion) < LARGO_RENGLON, descripcion);
    }
  }
}

static void test_pantallas_con_codigos() {
  // Sin descripción, el código ocupa un renglón y entra todo
  const char *const una[] = {"1 guardada", "P1234", "0 pendientes"};
  verificarPantalla(lectura("43 01 12 34", "4700", "NO DATA"), 0, 1, una, 3);

  // Con descripción ocupa dos; lo que no entra sigue en otra pantalla con el
  // mismo encabezado, y los pendientes van en la suya
  LecturaFallas dos = lectura("43 02 04 20 12 34", "47 01 03 01", "NO DATA");
  const char *const d1[] = {"2 guardadas", "P0420", "Catalizador"};
  const char *const d2[] = {"2 guardadas", "P1234"};
  const char *const d3[] = {"1 pendiente", "P0301", "Falla cil. 1"};
  verificarPantalla(dos, 0, 3, d1, 3);
  verificarPantalla(dos, 1, 3, d2, 2);
  verificarPantalla(dos, 2, 3, d3, 3);
  verificarPantalla(dos, 3, 3, d1, 3); // vuelve a empezar

  // Si la descripción no entra debajo del código anterior, código y
  // descripción pasan juntos a la pantalla siguiente
  LecturaFallas alReves = lectura("43 02 12 34 04 20", "4700", "NO DATA");
  const char *const r1[] = {"2 guardadas", "P1234"};
  const char *const r2[] = {"2 guardadas", "P0420", "Catalizador"};
  const char *const r3[] = {"0 pendientes"};
  verificarPantalla(alReves, 0, 3, r1, 2);
  verificarPantalla(alReves, 1, 3, r2, 3);
  verificarPantalla(alReves, 2, 3, r3, 1);

  // Muchos códigos de un tipo, y los permanentes si 0A contestó
  LecturaFallas cinco =
      lectura("00C\r0: 43 05 12 01 12 02 \r1: 12 03 12 04 12 05 00 \r\r", "4700", "4A 01 04 20");
  const char *const c1[] = {"5 guardadas", "P1201", "P1202"};
  const char *const c2[] = {"5 guardadas", "P1203", "P1204"};
  const char *const c3[] = {"5 guardadas", "P1205", "0 pendientes"};
  const char *const c4[] = {"1 permanente", "P0420", "Catalizador"};
  verificarPantalla(cinco, 0, 4, c1, 3);
  verificarPantalla(cinco, 1, 4, c2, 3);
  verificarPantalla(cinco, 2, 4, c3, 3);
  verificarPantalla(cinco, 3, 4, c4, 3);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_texto);
  RUN_TEST(test_lista);
  RUN_TEST(test_lista_invalida);
  RUN_TEST(test_desde_respuesta);
  RUN_TEST(test_descripcion);
  RUN_TEST(test_codigos);
  RUN_TEST(test_sin_fallas);
  RUN_TEST(test_luz);
  RUN_TEST(test_pantallas_de_estado);
  RUN_TEST(test_pantallas_con_codigos);
  return UNITY_END();
}
