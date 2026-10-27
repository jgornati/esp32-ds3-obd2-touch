#include <anteriores.h>
#include <string.h>
#include <unity.h>

// Las de config.h: VIGENCIA_MS, RPM_EN_MARCHA y ESPERA_VOLTAJE_VIAJE_MS
static const ReglasViaje REGLAS = {3000, 500, 5000};

void setUp() {}

void tearDown() {}

// 10 minutos a 60 km/h y 2000 rpm, una lectura por segundo, con los picos en
// el medio: 10 km
static Viaje viajeDeDiezMinutos() {
  Viaje viaje(REGLAS);
  for (uint32_t s = 0; s <= 600; s++) {
    uint32_t ahora = 1000 + s * 1000;
    bool pico = s == 300;
    viaje.velocidad(60, ahora);
    viaje.rpm(pico ? 5120 : 2000, ahora);
    viaje.refrigerante(pico ? 94 : 90);
    viaje.voltaje(s == 10 ? 13.6f : (pico ? 14.3f : 14.0f), ahora);
  }
  return viaje;
}

static const FechaHora LUNES = {2026, 10, 26, 8, 12, 41};

static void test_resumir() {
  Viaje viaje = viajeDeDiezMinutos();
  ResumenViaje r = resumir(viaje, 7, &LUNES);
  TEST_ASSERT_EQUAL_UINT32(7, r.numero);
  TEST_ASSERT_TRUE(r.conFecha);
  TEST_ASSERT_EQUAL_UINT8(12, r.inicio.minuto);
  TEST_ASSERT_EQUAL_UINT8(0, r.inicio.segundo);
  TEST_ASSERT_FLOAT_WITHIN(0.0001f, 10, r.km);
  TEST_ASSERT_EQUAL_UINT32(600000, r.enMarchaMs);
  TEST_ASSERT_TRUE(r.velocidadMaxima.hay);
  TEST_ASSERT_EQUAL_FLOAT(60, r.velocidadMaxima.valor);
  TEST_ASSERT_EQUAL_FLOAT(5120, r.rpmMaxima.valor);
  TEST_ASSERT_EQUAL_FLOAT(94, r.refrigeranteMaximo.valor);
  TEST_ASSERT_EQUAL_FLOAT(13.6f, r.voltajeMinimo.valor);
  TEST_ASSERT_EQUAL_FLOAT(14.3f, r.voltajeMaximo.valor);
  float kmh = 0;
  TEST_ASSERT_TRUE(promedio(r, &kmh));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 60, kmh);

  // Sin hora y sin lecturas: sin fecha, sin extremos y sin promedio
  ResumenViaje vacio = resumir(Viaje(REGLAS), 1, nullptr);
  TEST_ASSERT_FALSE(vacio.conFecha);
  TEST_ASSERT_FALSE(vacio.velocidadMaxima.hay);
  TEST_ASSERT_FALSE(vacio.voltajeMinimo.hay);
  TEST_ASSERT_FALSE(promedio(vacio, &kmh));
}

static void test_bytes_ida_y_vuelta() {
  ResumenViaje r = resumir(viajeDeDiezMinutos(), 123456, &LUNES);
  // Uno sin voltajes: las banderas de cada extremo van por separado
  r.voltajeMinimo = Extremo();
  r.voltajeMaximo = Extremo();
  uint8_t bytes[LARGO_RESUMEN];
  empaquetar(r, bytes);
  ResumenViaje leido;
  TEST_ASSERT_TRUE(desempaquetar(bytes, sizeof(bytes), &leido));
  TEST_ASSERT_EQUAL_UINT32(123456, leido.numero);
  TEST_ASSERT_TRUE(leido.conFecha);
  TEST_ASSERT_EQUAL_UINT16(2026, leido.inicio.anio);
  TEST_ASSERT_EQUAL_UINT8(10, leido.inicio.mes);
  TEST_ASSERT_EQUAL_UINT8(26, leido.inicio.dia);
  TEST_ASSERT_EQUAL_UINT8(8, leido.inicio.hora);
  TEST_ASSERT_EQUAL_UINT8(12, leido.inicio.minuto);
  TEST_ASSERT_EQUAL_FLOAT(r.km, leido.km);
  TEST_ASSERT_EQUAL_UINT32(600000, leido.enMarchaMs);
  TEST_ASSERT_TRUE(leido.velocidadMaxima.hay && leido.rpmMaxima.hay &&
                   leido.refrigeranteMaximo.hay);
  TEST_ASSERT_FALSE(leido.voltajeMinimo.hay || leido.voltajeMaximo.hay);
  TEST_ASSERT_EQUAL_FLOAT(60, leido.velocidadMaxima.valor);
  TEST_ASSERT_EQUAL_FLOAT(5120, leido.rpmMaxima.valor);
  TEST_ASSERT_EQUAL_FLOAT(94, leido.refrigeranteMaximo.valor);

  // Lo mismo da los mismos bytes
  uint8_t otra[LARGO_RESUMEN];
  empaquetar(leido, otra);
  TEST_ASSERT_EQUAL_MEMORY(bytes, otra, LARGO_RESUMEN);

  // Sin fecha
  ResumenViaje sinFecha = resumir(viajeDeDiezMinutos(), 3, nullptr);
  empaquetar(sinFecha, bytes);
  TEST_ASSERT_TRUE(desempaquetar(bytes, sizeof(bytes), &leido));
  TEST_ASSERT_FALSE(leido.conFecha);
  TEST_ASSERT_EQUAL_UINT32(3, leido.numero);
}

static void test_bytes_que_no_valen() {
  ResumenViaje r = resumir(viajeDeDiezMinutos(), 5, &LUNES);
  uint8_t bytes[LARGO_RESUMEN + 1];
  empaquetar(r, bytes);
  ResumenViaje leido;
  leido.numero = 99;
  // Otro largo (otra versión que creció, o nada guardado)
  TEST_ASSERT_FALSE(desempaquetar(bytes, LARGO_RESUMEN + 1, &leido));
  TEST_ASSERT_FALSE(desempaquetar(bytes, LARGO_RESUMEN - 1, &leido));
  TEST_ASSERT_FALSE(desempaquetar(bytes, 0, &leido));
  // Otra versión
  uint8_t otra[LARGO_RESUMEN];
  memcpy(otra, bytes, LARGO_RESUMEN);
  otra[0]++;
  TEST_ASSERT_FALSE(desempaquetar(otra, LARGO_RESUMEN, &leido));
  // Sin número
  empaquetar(resumir(viajeDeDiezMinutos(), 0, &LUNES), otra);
  TEST_ASSERT_FALSE(desempaquetar(otra, LARGO_RESUMEN, &leido));
  // Con una fecha que no existe: el mes 13
  memcpy(otra, bytes, LARGO_RESUMEN);
  otra[8] = 13;
  TEST_ASSERT_FALSE(desempaquetar(otra, LARGO_RESUMEN, &leido));
  // Lo que no vale no toca el resumen
  TEST_ASSERT_EQUAL_UINT32(99, leido.numero);
}

static ResumenViaje numerado(uint32_t numero) {
  ResumenViaje r;
  r.numero = numero;
  return r;
}

static void test_anteriores() {
  Anteriores lista;
  TEST_ASSERT_EQUAL_UINT8(0, lista.cantidad());
  // Al leerlos de la flash: del más nuevo al más viejo
  lista.agregarViejo(numerado(12));
  lista.agregarViejo(numerado(11));
  TEST_ASSERT_EQUAL_UINT8(2, lista.cantidad());
  TEST_ASSERT_EQUAL_UINT32(12, lista.viaje(0).numero);
  TEST_ASSERT_EQUAL_UINT32(11, lista.viaje(1).numero);
  // Al reiniciar el viaje: el que termina va primero
  lista.agregarNuevo(numerado(13));
  TEST_ASSERT_EQUAL_UINT8(3, lista.cantidad());
  TEST_ASSERT_EQUAL_UINT32(13, lista.viaje(0).numero);
  TEST_ASSERT_EQUAL_UINT32(11, lista.viaje(2).numero);

  // Llena: el nuevo entra y se pierde el más viejo; uno viejo no entra
  Anteriores llena;
  for (uint32_t n = 1; n <= VIAJES_GUARDADOS; n++) {
    llena.agregarNuevo(numerado(n));
  }
  TEST_ASSERT_EQUAL_UINT8(VIAJES_GUARDADOS, llena.cantidad());
  llena.agregarViejo(numerado(99));
  llena.agregarNuevo(numerado(VIAJES_GUARDADOS + 1));
  TEST_ASSERT_EQUAL_UINT8(VIAJES_GUARDADOS, llena.cantidad());
  TEST_ASSERT_EQUAL_UINT32(VIAJES_GUARDADOS + 1, llena.viaje(0).numero);
  TEST_ASSERT_EQUAL_UINT32(2, llena.viaje(VIAJES_GUARDADOS - 1).numero);
}

static void test_texto_inicio() {
  char texto[24];
  textoInicio(LUNES, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING("Lun 26/10 08:12", texto);
  textoInicio({2026, 1, 4, 23, 5, 0}, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING("Dom 04/01 23:05", texto);
  textoInicio({2028, 2, 29, 0, 0, 0}, texto, sizeof(texto));
  TEST_ASSERT_EQUAL_STRING("Mar 29/02 00:00", texto);
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_resumir);
  RUN_TEST(test_bytes_ida_y_vuelta);
  RUN_TEST(test_bytes_que_no_valen);
  RUN_TEST(test_anteriores);
  RUN_TEST(test_texto_inicio);
  return UNITY_END();
}
