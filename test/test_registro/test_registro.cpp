#include <registro.h>
#include <string.h>
#include <unity.h>

void setUp() {}

void tearDown() {}

static void test_linea() {
  char texto[64];
  LineaCsv linea(texto, sizeof(texto));
  linea.texto("12");
  linea.texto(nullptr);
  linea.texto("");
  linea.texto("+0.31");
  linea.texto("Cerrado");
  TEST_ASSERT_EQUAL(19, linea.terminar());
  TEST_ASSERT_EQUAL_STRING("12,,,+0.31,Cerrado\n", texto);
}

// Vacíos al final también cuentan: todas las líneas tienen las mismas columnas
static void test_linea_vacios_al_final() {
  char texto[16];
  LineaCsv linea(texto, sizeof(texto));
  linea.texto("7");
  linea.texto(nullptr);
  linea.texto(nullptr);
  linea.terminar();
  TEST_ASSERT_EQUAL_STRING("7,,\n", texto);
}

static void test_linea_comillas() {
  char texto[96];
  LineaCsv linea(texto, sizeof(texto));
  linea.texto("Fallas: luz encendida, P0300");
  linea.texto("dijo \"hola\"");
  linea.texto("dos\nrenglones");
  linea.terminar();
  TEST_ASSERT_EQUAL_STRING(
      "\"Fallas: luz encendida, P0300\",\"dijo \"\"hola\"\"\",\"dos\nrenglones\"\n", texto);
}

static void test_linea_no_entra() {
  char texto[8];
  LineaCsv entra(texto, sizeof(texto));
  entra.texto("123");
  entra.texto("45");
  TEST_ASSERT_EQUAL(7, entra.terminar());
  TEST_ASSERT_EQUAL_STRING("123,45\n", texto);

  LineaCsv larga(texto, sizeof(texto));
  larga.texto("123");
  larga.texto("456");
  TEST_ASSERT_EQUAL(0, larga.terminar());
  TEST_ASSERT_EQUAL('\0', texto[sizeof(texto) - 1]);
}

static void test_eventos() {
  Eventos eventos;
  TEST_ASSERT_EQUAL_STRING("", eventos.texto());
  eventos.agregar("SIN_ECU (No responde)");
  eventos.agregar("LEYENDO");
  TEST_ASSERT_EQUAL_STRING("SIN_ECU (No responde) | LEYENDO", eventos.texto());
  eventos.vaciar();
  TEST_ASSERT_EQUAL_STRING("", eventos.texto());
  eventos.agregar("Viaje reiniciado");
  TEST_ASSERT_EQUAL_STRING("Viaje reiniciado", eventos.texto());
}

static void test_eventos_no_entran() {
  Eventos eventos;
  char largo[LARGO_EVENTOS];
  memset(largo, 'x', sizeof(largo) - 1);
  largo[sizeof(largo) - 1] = '\0';
  eventos.agregar("LEYENDO");
  eventos.agregar(largo);
  const char *texto = eventos.texto();
  TEST_ASSERT_EQUAL(LARGO_EVENTOS - 1, strlen(texto));
  TEST_ASSERT_EQUAL(0, strncmp(texto, "LEYENDO | xxx", 13));
  TEST_ASSERT_EQUAL_STRING("...", texto + strlen(texto) - 3);
  // Cortado, no se agrega nada más
  eventos.agregar("SIN_ECU");
  TEST_ASSERT_EQUAL(LARGO_EVENTOS - 1, strlen(eventos.texto()));
  eventos.vaciar();
  eventos.agregar("SIN_ECU");
  TEST_ASSERT_EQUAL_STRING("SIN_ECU", eventos.texto());
}

// El corte no parte el "°" (dos bytes en UTF-8)
static void test_eventos_utf8() {
  Eventos eventos;
  char largo[LARGO_EVENTOS + 8];
  size_t antes = LARGO_EVENTOS - 5; // el "°" queda en los bytes 155 y 156
  memset(largo, 'x', antes);
  strcpy(largo + antes, "°Cxxxxxxxx");
  eventos.agregar(largo);
  const char *texto = eventos.texto();
  TEST_ASSERT_EQUAL(antes + 3, strlen(texto));
  TEST_ASSERT_EQUAL_STRING("...", texto + antes);
}

static void test_nombres() {
  char nombre[16];
  nombreDeViaje(12, nombre, sizeof(nombre));
  TEST_ASSERT_EQUAL_STRING("VIAJE0012.CSV", nombre);
  nombreDeViaje(MAX_VIAJES, nombre, sizeof(nombre));
  TEST_ASSERT_EQUAL_STRING("VIAJE9999.CSV", nombre);

  uint16_t numero = 0;
  TEST_ASSERT_TRUE(numeroDeViaje("VIAJE0012.CSV", &numero));
  TEST_ASSERT_EQUAL(12, numero);
  TEST_ASSERT_TRUE(numeroDeViaje("viaje0345.csv", &numero));
  TEST_ASSERT_EQUAL(345, numero);
  TEST_ASSERT_TRUE(numeroDeViaje("VIAJE9999.CSV", &numero));
  TEST_ASSERT_EQUAL(9999, numero);

  numero = 77;
  TEST_ASSERT_FALSE(numeroDeViaje("VIAJE0000.CSV", &numero));
  TEST_ASSERT_FALSE(numeroDeViaje("VIAJE012.CSV", &numero));
  TEST_ASSERT_FALSE(numeroDeViaje("VIAJE00123.CSV", &numero));
  TEST_ASSERT_FALSE(numeroDeViaje("VIAJE00A2.CSV", &numero));
  TEST_ASSERT_FALSE(numeroDeViaje("VIAJE0012.TXT", &numero));
  TEST_ASSERT_FALSE(numeroDeViaje("RECORR0012.CSV", &numero));
  TEST_ASSERT_FALSE(numeroDeViaje("System Volume Information", &numero));
  TEST_ASSERT_FALSE(numeroDeViaje("", &numero));
  TEST_ASSERT_EQUAL(77, numero);
}

static void test_bufer() {
  char memoria[16];
  BuferTexto bufer;
  bufer.usar(memoria, sizeof(memoria));
  TEST_ASSERT_TRUE(bufer.agregar("0,a\n", 4));
  TEST_ASSERT_TRUE(bufer.agregar("1,bb\n", 5));
  TEST_ASSERT_EQUAL(9, bufer.usados());

  char destino[16];
  TEST_ASSERT_EQUAL(9, bufer.tomar(destino, sizeof(destino)));
  TEST_ASSERT_EQUAL(0, memcmp(destino, "0,a\n1,bb\n", 9));
  TEST_ASSERT_EQUAL(0, bufer.usados());
  TEST_ASSERT_EQUAL(0, bufer.tomar(destino, sizeof(destino)));
  TEST_ASSERT_EQUAL(0, bufer.perdidas());
}

// Lleno, la línea entera queda afuera y se cuenta
static void test_bufer_lleno() {
  char memoria[10];
  BuferTexto bufer;
  bufer.usar(memoria, sizeof(memoria));
  TEST_ASSERT_TRUE(bufer.agregar("0,aaaa\n", 7));
  TEST_ASSERT_FALSE(bufer.agregar("1,b\n", 4));
  TEST_ASSERT_TRUE(bufer.agregar("2,\n", 3));
  TEST_ASSERT_FALSE(bufer.agregar("3\n", 2));
  TEST_ASSERT_EQUAL(10, bufer.usados());
  TEST_ASSERT_EQUAL(2, bufer.perdidas());

  char destino[10];
  TEST_ASSERT_EQUAL(10, bufer.tomar(destino, sizeof(destino)));
  TEST_ASSERT_EQUAL(0, memcmp(destino, "0,aaaa\n2,\n", 10));
  TEST_ASSERT_TRUE(bufer.agregar("4\n", 2));
  TEST_ASSERT_EQUAL(2, bufer.perdidas());
}

// Con un destino más chico, toma lo más viejo y deja el resto en orden
static void test_bufer_de_a_partes() {
  char memoria[16];
  BuferTexto bufer;
  bufer.usar(memoria, sizeof(memoria));
  bufer.agregar("abcdef\n", 7);
  bufer.agregar("ghi\n", 4);
  char destino[5];
  TEST_ASSERT_EQUAL(5, bufer.tomar(destino, sizeof(destino)));
  TEST_ASSERT_EQUAL(0, memcmp(destino, "abcde", 5));
  TEST_ASSERT_EQUAL(6, bufer.usados());
  TEST_ASSERT_EQUAL(5, bufer.tomar(destino, sizeof(destino)));
  TEST_ASSERT_EQUAL(0, memcmp(destino, "f\nghi", 5));
  TEST_ASSERT_EQUAL(1, bufer.tomar(destino, sizeof(destino)));
  TEST_ASSERT_EQUAL('\n', destino[0]);
}

// Sin memoria (no se pudo reservar), todo se pierde
static void test_bufer_sin_memoria() {
  BuferTexto bufer;
  bufer.usar(nullptr, 100);
  TEST_ASSERT_FALSE(bufer.agregar("0\n", 2));
  TEST_ASSERT_EQUAL(1, bufer.perdidas());
  char destino[4];
  TEST_ASSERT_EQUAL(0, bufer.tomar(destino, sizeof(destino)));
}

static void verificarTarjeta(EstadoTarjeta estado, const char *esperado, bool problema) {
  char texto[32];
  textoTarjeta(estado, 12, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING(esperado, texto);
  TEST_ASSERT_EQUAL(problema, problemaTarjeta(estado));
}

static void test_textos_tarjeta() {
  verificarTarjeta(EstadoTarjeta::BUSCANDO, "...", false);
  verificarTarjeta(EstadoTarjeta::SIN_TARJETA, "Sin tarjeta", false);
  verificarTarjeta(EstadoTarjeta::ILEGIBLE, "No se puede leer", true);
  verificarTarjeta(EstadoTarjeta::LISTA, "Lista", false);
  verificarTarjeta(EstadoTarjeta::GRABANDO, "Grabando VIAJE0012", false);
  verificarTarjeta(EstadoTarjeta::ERROR, "Error al escribir", true);
  verificarTarjeta(EstadoTarjeta::LLENA, "Tarjeta llena", true);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_linea);
  RUN_TEST(test_linea_vacios_al_final);
  RUN_TEST(test_linea_comillas);
  RUN_TEST(test_linea_no_entra);
  RUN_TEST(test_eventos);
  RUN_TEST(test_eventos_no_entran);
  RUN_TEST(test_eventos_utf8);
  RUN_TEST(test_nombres);
  RUN_TEST(test_bufer);
  RUN_TEST(test_bufer_lleno);
  RUN_TEST(test_bufer_de_a_partes);
  RUN_TEST(test_bufer_sin_memoria);
  RUN_TEST(test_textos_tarjeta);
  return UNITY_END();
}
