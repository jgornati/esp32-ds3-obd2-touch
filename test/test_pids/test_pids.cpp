#include <unity.h>
#include <pids.h>

#include <stdio.h>

void setUp() {}

void tearDown() {}

template <size_t N>
static void verificarPid(uint8_t pid, const uint8_t (&datos)[N], float esperado) {
  float valor = 0;
  TEST_ASSERT_TRUE_MESSAGE(calcularPid(pid, datos, N, &valor), buscarPid(pid)->nombre);
  TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, esperado, valor, buscarPid(pid)->nombre);
}

static void test_tabla_minima() {
  // Los que pide el plan, más los mapas de sondas completos y los de la
  // página Carga (E4)
  const uint8_t pids[] = {0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E, 0x0F,
                          0x11, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B,
                          0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x34, 0x35,
                          0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x42, 0x44};
  for (uint8_t pid : pids) {
    char mensaje[16];
    snprintf(mensaje, sizeof(mensaje), "PID %02X", pid);
    TEST_ASSERT_NOT_NULL_MESSAGE(buscarPid(pid), mensaje);
  }
  TEST_ASSERT_NULL(buscarPid(0x00));
  TEST_ASSERT_NULL(buscarPid(0x99));

  const Pid *rpm = buscarPid(0x0C);
  TEST_ASSERT_EQUAL_STRING("RPM", rpm->nombre);
  TEST_ASSERT_EQUAL_STRING("rpm", rpm->unidad);
  TEST_ASSERT_EQUAL_UINT8(2, rpm->bytes);
}

static void test_formulas() {
  verificarPid(0x04, {0xFF}, 100);
  verificarPid(0x04, {0x00}, 0);
  verificarPid(0x05, {0x7B}, 83);
  verificarPid(0x05, {0x00}, -40);
  verificarPid(0x06, {0x80}, 0);
  verificarPid(0x06, {0x00}, -100);
  verificarPid(0x06, {0xFF}, 99.22f);
  verificarPid(0x07, {0x90}, 12.5f);
  verificarPid(0x0B, {0x65}, 101);
  verificarPid(0x0C, {0x1A, 0xF8}, 1726);
  verificarPid(0x0C, {0x0B, 0xB8}, 750);
  verificarPid(0x0D, {0x32}, 50);
  verificarPid(0x0E, {0x80}, 0);
  verificarPid(0x0E, {0x98}, 12);
  verificarPid(0x0E, {0x6C}, -10);
  verificarPid(0x0E, {0xFF}, 63.5f);
  verificarPid(0x0E, {0x00}, -64);
  verificarPid(0x0F, {0x41}, 25);
  verificarPid(0x10, {0x01, 0xF4}, 5);
  verificarPid(0x11, {0x80}, 50.2f);
  verificarPid(0x14, {0x5A, 0x80}, 0.45f);
  verificarPid(0x1B, {0xC8, 0xFF}, 1.0f);
  verificarPid(0x24, {0x80, 0x00, 0x80, 0x00}, 1.0f);
  verificarPid(0x2B, {0x66, 0x66, 0x00, 0x00}, 0.8f);
  verificarPid(0x34, {0x80, 0x00, 0x80, 0x00}, 1.0f);
  verificarPid(0x42, {0x30, 0x39}, 12.345f);
  verificarPid(0x44, {0x80, 0x00}, 1.0f);
}

static void test_calcular_falla() {
  float valor = 0;
  const uint8_t corto[] = {0x1A};
  TEST_ASSERT_FALSE(calcularPid(0x0C, corto, 1, &valor)); // faltan bytes
  TEST_ASSERT_FALSE(calcularPid(0x99, corto, 1, &valor)); // no está en la tabla
  const uint8_t cuatro[] = {0x83, 0x07, 0xE5, 0x00};
  TEST_ASSERT_FALSE(calcularPid(0x01, cuatro, 4, &valor)); // no es un número
  TEST_ASSERT_FALSE(calcularPid(0x03, cuatro, 4, &valor));
  TEST_ASSERT_FALSE(calcularPid(0x13, cuatro, 4, &valor));
  TEST_ASSERT_FALSE(calcularPid(0x0C, nullptr, 2, &valor));
}

static void test_luz_falla() {
  bool encendida = false;
  uint8_t cantidad = 0;
  const uint8_t tres[] = {0x83, 0x07, 0xE5, 0x00};
  TEST_ASSERT_TRUE(luzFalla(tres, 4, &encendida, &cantidad));
  TEST_ASSERT_TRUE(encendida);
  TEST_ASSERT_EQUAL_UINT8(3, cantidad);

  const uint8_t ninguna[] = {0x00, 0x07, 0xE5, 0x00};
  TEST_ASSERT_TRUE(luzFalla(ninguna, 4, &encendida, &cantidad));
  TEST_ASSERT_FALSE(encendida);
  TEST_ASSERT_EQUAL_UINT8(0, cantidad);

  TEST_ASSERT_FALSE(luzFalla(tres, 1, &encendida, &cantidad));
}

static void test_lazo() {
  TEST_ASSERT_EQUAL_STRING("Abierto frio", textoLazo(0x01));
  TEST_ASSERT_EQUAL_STRING("Cerrado", textoLazo(0x02));
  TEST_ASSERT_EQUAL_STRING("Abierto carga", textoLazo(0x04));
  TEST_ASSERT_EQUAL_STRING("Abierto falla", textoLazo(0x08));
  TEST_ASSERT_EQUAL_STRING("Cerrado falla", textoLazo(0x10));
  TEST_ASSERT_NULL(textoLazo(0x00));
  TEST_ASSERT_NULL(textoLazo(0x03));
}

static void test_sondas() {
  // 0x03: banco 1, sensores 1 y 2 (antes y después del catalizador)
  TEST_ASSERT_TRUE(sondaPresente(0x03, 1, 1));
  TEST_ASSERT_TRUE(sondaPresente(0x03, 1, 2));
  TEST_ASSERT_FALSE(sondaPresente(0x03, 1, 3));
  TEST_ASSERT_FALSE(sondaPresente(0x03, 2, 1));
  TEST_ASSERT_TRUE(sondaPresente(0x10, 2, 1));
  TEST_ASSERT_TRUE(sondaPresente(0x80, 2, 4));
  TEST_ASSERT_FALSE(sondaPresente(0xFF, 0, 1));
  TEST_ASSERT_FALSE(sondaPresente(0xFF, 3, 1));
  TEST_ASSERT_FALSE(sondaPresente(0xFF, 1, 0));
  TEST_ASSERT_FALSE(sondaPresente(0xFF, 1, 5));
}

static void test_soportados() {
  PidsSoportados soportados;
  TEST_ASSERT_FALSE(soportados.soportado(0x0C));
  TEST_ASSERT_TRUE(soportados.soportado(0x00));

  // BE 3E B8 11: 01 03-07 0B-0F 11 13-15 1C 20
  const uint8_t mapa00[] = {0xBE, 0x3E, 0xB8, 0x11};
  TEST_ASSERT_TRUE(soportados.cargar(0x00, mapa00, 4));
  const uint8_t si[] = {0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x0B, 0x0C, 0x0D,
                        0x0E, 0x0F, 0x11, 0x13, 0x14, 0x15, 0x1C, 0x20};
  const uint8_t no[] = {0x02, 0x08, 0x09, 0x0A, 0x10, 0x12, 0x16, 0x17,
                        0x18, 0x19, 0x1A, 0x1B, 0x1D, 0x1E, 0x1F, 0x21};
  for (uint8_t pid : si) {
    TEST_ASSERT_TRUE(soportados.soportado(pid));
  }
  for (uint8_t pid : no) {
    TEST_ASSERT_FALSE(soportados.soportado(pid));
  }
  TEST_ASSERT_EQUAL_UINT8(16, soportados.cantidad()); // el 20 no cuenta

  const uint8_t mapa20[] = {0x80, 0x00, 0x00, 0x01};
  TEST_ASSERT_TRUE(soportados.cargar(0x20, mapa20, 4));
  TEST_ASSERT_TRUE(soportados.soportado(0x21));
  TEST_ASSERT_TRUE(soportados.soportado(0x40));
  TEST_ASSERT_FALSE(soportados.soportado(0x22));
  TEST_ASSERT_EQUAL_UINT8(17, soportados.cantidad());

  TEST_ASSERT_FALSE(soportados.cargar(0x10, mapa00, 4)); // base inválida
  TEST_ASSERT_FALSE(soportados.cargar(0x40, mapa00, 3)); // faltan bytes

  soportados.limpiar();
  TEST_ASSERT_FALSE(soportados.soportado(0x0C));
  TEST_ASSERT_EQUAL_UINT8(0, soportados.cantidad());
}

static void test_separar_pids() {
  // 010C0D050F0607: RPM 733, 0 km/h, 21 y 21 °C, ajustes 0 y 1,6 %
  const uint8_t datos[] = {0x0C, 0x0B, 0x74, 0x0D, 0x00, 0x05, 0x3D,
                           0x0F, 0x3D, 0x06, 0x80, 0x07, 0x82};
  DatoPid pids[6] = {};
  TEST_ASSERT_EQUAL_UINT8(6, separarPids(datos, sizeof(datos), pids, 6));
  const uint8_t esperados[] = {0x0C, 0x0D, 0x05, 0x0F, 0x06, 0x07};
  float v = 0;
  for (uint8_t i = 0; i < 6; i++) {
    TEST_ASSERT_EQUAL_HEX8(esperados[i], pids[i].pid);
  }
  TEST_ASSERT_TRUE(calcularPid(pids[0].pid, pids[0].datos, pids[0].largo, &v));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 733, v);
  TEST_ASSERT_TRUE(calcularPid(pids[3].pid, pids[3].datos, pids[3].largo, &v));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 21, v);
  TEST_ASSERT_TRUE(calcularPid(pids[5].pid, pids[5].datos, pids[5].largo, &v));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.5625f, v);

  // Lazo (2 bytes), lambda (4) y sonda post (2)
  const uint8_t mezcla[] = {0x03, 0x02, 0x00, 0x34, 0x7F, 0x68, 0x7F, 0xFB, 0x15, 0x5B, 0xFF};
  TEST_ASSERT_EQUAL_UINT8(3, separarPids(mezcla, sizeof(mezcla), pids, 6));
  TEST_ASSERT_EQUAL_HEX8(0x34, pids[1].pid);
  TEST_ASSERT_EQUAL_UINT8(4, pids[1].largo);
  TEST_ASSERT_EQUAL_HEX8(0x5B, pids[2].datos[0]);

  // Carga, avance y acelerador (página Carga, E4): un byte cada uno
  const uint8_t carga[] = {0x04, 0x3B, 0x0E, 0x98, 0x11, 0x24};
  TEST_ASSERT_EQUAL_UINT8(3, separarPids(carga, sizeof(carga), pids, 6));
  TEST_ASSERT_TRUE(calcularPid(pids[1].pid, pids[1].datos, pids[1].largo, &v));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 12, v);
  TEST_ASSERT_EQUAL_HEX8(0x24, pids[2].datos[0]);

  // Hasta max
  TEST_ASSERT_EQUAL_UINT8(2, separarPids(datos, sizeof(datos), pids, 2));

  // Se detiene en un PID que no está en la tabla o al que le faltan bytes
  const uint8_t desconocido[] = {0x0D, 0x00, 0x99, 0x01, 0x05, 0x3D};
  TEST_ASSERT_EQUAL_UINT8(1, separarPids(desconocido, sizeof(desconocido), pids, 6));
  const uint8_t corto[] = {0x0D, 0x00, 0x0C, 0x0B};
  TEST_ASSERT_EQUAL_UINT8(1, separarPids(corto, sizeof(corto), pids, 6));
  TEST_ASSERT_EQUAL_UINT8(0, separarPids(nullptr, 4, pids, 6));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_tabla_minima);
  RUN_TEST(test_formulas);
  RUN_TEST(test_calcular_falla);
  RUN_TEST(test_luz_falla);
  RUN_TEST(test_lazo);
  RUN_TEST(test_sondas);
  RUN_TEST(test_soportados);
  RUN_TEST(test_separar_pids);
  return UNITY_END();
}
