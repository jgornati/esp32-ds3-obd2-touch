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

static void test_error_al_leer() {
  TEST_ASSERT_FALSE(errorAlLeer(lectura("4300", "4700", "NO DATA")));
  TEST_ASSERT_FALSE(errorAlLeer(lectura("43 01 01 33", "4700", "CAN ERROR")));
  TEST_ASSERT_TRUE(errorAlLeer(lectura("NO DATA", "4700", "NO DATA")));
  TEST_ASSERT_TRUE(errorAlLeer(lectura("4300", "CAN ERROR", "4A00")));
  LecturaFallas cortada = lectura("4300", "4700", "NO DATA");
  cortada.pendientes = interpretarCodigos("4700", "07", false);
  TEST_ASSERT_TRUE(errorAlLeer(cortada));
}

static LecturaFallas conLuz(LecturaFallas l, bool encendida) {
  l.luzLeida = true;
  l.luz = encendida;
  return l;
}

static void verificarVista(VistaFallas esperada, const LecturaFallas &l) {
  TEST_ASSERT_EQUAL_INT((int)esperada, (int)pantallaFallas(l, 0).vista);
}

static void test_vistas() {
  verificarVista(VistaFallas::LEYENDO, LecturaFallas());
  LecturaFallas ninguna = lectura("4300", "4700", "NO DATA");
  verificarVista(VistaFallas::SIN_FALLAS, conLuz(ninguna, false));
  verificarVista(VistaFallas::SIN_FALLAS, ninguna); // 0101 no contestó
  // Con la luz encendida no, aunque 03 y 07 no traigan códigos
  verificarVista(VistaFallas::CODIGOS, conLuz(ninguna, true));
  // Nunca con un error en 03 o 07, aunque lo demás esté vacío
  verificarVista(VistaFallas::CODIGOS, conLuz(lectura("NO DATA", "4700", "NO DATA"), false));
  verificarVista(VistaFallas::CODIGOS, conLuz(lectura("4300", "CAN ERROR", "NO DATA"), false));
  verificarVista(VistaFallas::CODIGOS, lectura("4300", "4700", "4A 01 04 20"));

  PantallaFallas p = pantallaFallas(conLuz(ninguna, true), 0);
  TEST_ASSERT_EQUAL_UINT8(0, p.grupos);
  TEST_ASSERT_EQUAL_UINT8(0, p.cantidad);
}

// Compara los renglones del grupo número vuelta: código y tipo
static void verificarRenglones(const LecturaFallas &l, uint32_t vuelta, uint8_t grupos,
                               const uint16_t *codigos, const TipoDtc *tipos, uint8_t cantidad) {
  PantallaFallas p = pantallaFallas(l, vuelta);
  TEST_ASSERT_EQUAL_INT((int)VistaFallas::CODIGOS, (int)p.vista);
  TEST_ASSERT_EQUAL_UINT8(grupos, p.grupos);
  TEST_ASSERT_EQUAL_UINT8(grupos > 0 ? vuelta % grupos : 0, p.grupo);
  TEST_ASSERT_EQUAL_UINT8(cantidad, p.cantidad);
  for (uint8_t i = 0; i < cantidad; i++) {
    TEST_ASSERT_EQUAL_HEX16(codigos[i], p.renglones[i].codigo);
    TEST_ASSERT_EQUAL_INT((int)tipos[i], (int)p.renglones[i].tipo);
  }
}

static const TipoDtc G = TipoDtc::GUARDADO;
static const TipoDtc PE = TipoDtc::PENDIENTE;
static const TipoDtc PM = TipoDtc::PERMANENTE;

static void test_renglones() {
  // La de la maqueta: guardados y después pendientes
  const uint16_t m[] = {0x0420, 0x0171, 0x0300};
  const TipoDtc mt[] = {G, G, PE};
  verificarRenglones(lectura("43 02 04 20 01 71", "47 01 03 00", "NO DATA"), 0, 1, m, mt, 3);

  // Uno que está guardado y pendiente va una vez, como guardado
  const uint16_t r[] = {0x0420, 0x0171, 0x0300};
  const TipoDtc rt[] = {G, G, PE};
  verificarRenglones(lectura("43 02 04 20 01 71", "47 02 04 20 03 00", "NO DATA"), 0, 1, r, rt, 3);

  // Solo pendientes
  const uint16_t pe[] = {0x0300, 0x1234};
  const TipoDtc pet[] = {PE, PE};
  verificarRenglones(lectura("4300", "47 02 03 00 12 34", "NO DATA"), 0, 1, pe, pet, 2);

  // Permanentes entre guardados y pendientes, y sin repetir
  const uint16_t pm[] = {0x0420, 0x0171, 0x0300};
  const TipoDtc pmt[] = {G, PM, PE};
  verificarRenglones(lectura("43 01 04 20", "47 02 03 00 01 71", "4A 02 01 71 04 20"), 0, 1, pm,
                     pmt, 3);

  // Con un error, los del servicio que sí contestó
  const uint16_t e[] = {0x0420};
  const TipoDtc et[] = {G};
  verificarRenglones(lectura("43 01 04 20", "NO DATA", "NO DATA"), 0, 1, e, et, 1);
  verificarRenglones(conLuz(lectura("47 01 04 20", "CAN ERROR", "NO DATA"), true), 0, 0, e, et, 0);
}

static void test_grupos() {
  // 5 guardados y 2 pendientes, uno repetido: 6 renglones en dos grupos
  LecturaFallas seis = lectura("00C\r0: 43 05 12 01 12 02 \r1: 12 03 12 04 12 05 00 \r\r",
                               "47 02 12 05 03 00", "NO DATA");
  const uint16_t g1[] = {0x1201, 0x1202, 0x1203, 0x1204};
  const TipoDtc g1t[] = {G, G, G, G};
  const uint16_t g2[] = {0x1205, 0x0300};
  const TipoDtc g2t[] = {G, PE};
  verificarRenglones(seis, 0, 2, g1, g1t, 4);
  verificarRenglones(seis, 1, 2, g2, g2t, 2);
  verificarRenglones(seis, 2, 2, g1, g1t, 4); // vuelve a empezar

  // Justo 4: un grupo
  const uint16_t c[] = {0x0420, 0x0171, 0x0172, 0x0300};
  const TipoDtc ct[] = {G, G, G, PE};
  verificarRenglones(lectura("43 03 04 20 01 71 01 72", "47 01 03 00", "NO DATA"), 7, 1, c, ct, 4);

  // Lo más que puede llegar: 31 de cada uno, sin repetir
  LecturaFallas llena;
  llena.leida = true;
  Codigos *listas[] = {&llena.guardados, &llena.pendientes, &llena.permanentes};
  for (uint8_t l = 0; l < 3; l++) {
    listas[l]->estado = EstadoCodigos::LEIDOS;
    listas[l]->lista.cantidad = MAX_DTC;
    for (uint8_t i = 0; i < MAX_DTC; i++) {
      listas[l]->lista.codigos[i] = (uint16_t)(0x1000 + l * 0x100 + i);
    }
  }
  PantallaFallas p = pantallaFallas(llena, 23);
  TEST_ASSERT_EQUAL_UINT8(24, p.grupos); // 93 renglones
  TEST_ASSERT_EQUAL_UINT8(1, p.cantidad);
  TEST_ASSERT_EQUAL_HEX16(0x1100 + MAX_DTC - 1, p.renglones[0].codigo); // el último pendiente
}

static void verificarCantidad(const char *esperado, bool leida, const Codigos &c, TipoDtc tipo) {
  char texto[24];
  TEST_ASSERT_EQUAL(leida, textoCantidad(c, tipo, texto, sizeof(texto)));
  TEST_ASSERT_EQUAL_STRING(esperado, texto);
}

static void test_cantidades() {
  LecturaFallas l = lectura("43 02 04 20 01 71", "47 01 03 00", "4A00");
  verificarCantidad("2 guardadas", true, l.guardados, G);
  verificarCantidad("1 pendiente", true, l.pendientes, PE);
  verificarCantidad("0 permanentes", true, l.permanentes, PM);
  l = lectura("4300", "NO DATA", "CAN ERROR");
  verificarCantidad("0 guardadas", true, l.guardados, G);
  verificarCantidad("Pendientes: error", false, l.pendientes, PE);
  verificarCantidad("Permanentes: error", false, l.permanentes, PM);
  verificarCantidad("Guardadas: error", false, Codigos(), G);
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
      TEST_ASSERT_TRUE_MESSAGE(strlen(descripcion) <= LARGO_DESCRIPCION, descripcion);
    }
  }
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
  RUN_TEST(test_error_al_leer);
  RUN_TEST(test_vistas);
  RUN_TEST(test_renglones);
  RUN_TEST(test_grupos);
  RUN_TEST(test_cantidades);
  return UNITY_END();
}
