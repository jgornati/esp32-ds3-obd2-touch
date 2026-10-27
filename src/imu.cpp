#include "imu.h"

#include <Arduino.h>
#include <driver/i2c_master.h>

#include "config.h"
#include "placa.h"

static const uint32_t VELOCIDAD_I2C_HZ = 300000; // la de los ejemplos de Waveshare
static const int TIMEOUT_I2C_MS = 50;

// Registros del QMI8658 (datasheet del QMI8658A, rev. A)
static const uint8_t QUIEN_SOY = 0x00; // WHO_AM_I
static const uint8_t ES_QMI8658 = 0x05;
static const uint8_t CTRL1 = 0x02;
static const uint8_t CTRL2 = 0x03;
static const uint8_t CTRL5 = 0x06;
static const uint8_t CTRL7 = 0x08;
static const uint8_t AX_L = 0x35; // x, y, z del acelerómetro, bajo y alto
static const uint8_t RESULTADO_REINICIO = 0x4D;
static const uint8_t REINICIADO = 0x80;
static const uint8_t REINICIO = 0x60;
static const uint8_t REINICIAR = 0xB0;
// Dirección automática y primero el byte bajo; las salidas de interrupción
// en alta impedancia (van a los pines 2 y 3 del TCA9554)
static const uint8_t SERIE = 0x40;
// ±4 g (los baches no lo saturan) y 125 muestras por segundo
static const uint8_t ACELEROMETRO = 0x16;
// Su filtro al 3,63 % de las 125: 4,5 Hz, para leerlo a 50 Hz sin ruido de más
static const uint8_t FILTRO = 0x03;
// Solo el acelerómetro: el giróscopo queda apagado
static const uint8_t SENSORES = 0x01;
static const float G_POR_CUENTA = 4.0f / 32768;
// Lo que tarda en dar muestras: 3 ms + 3 muestras
static const uint32_t ARRANQUE_MS = 30;
static const uint32_t REINICIO_MS = 100; // datasheet: ~15 ms

// La derecha del auto en los ejes del sensor; girada 180°, al revés
static const float SIGNO = PANTALLA_GIRADA ? -1 : 1;
static const ReglasFuerzaG REGLAS = {
    {SIGNO * EJE_DERECHA_IMU[0], SIGNO * EJE_DERECHA_IMU[1], SIGNO * EJE_DERECHA_IMU[2]},
    FILTRO_FUERZA_G_MS,
    TOLERANCIA_QUIETA_G,
    QUIETA_FUERZA_G_MS,
    VIGENCIA_FUERZA_G_MS};

static i2c_master_dev_handle_t sensor = nullptr;
static SemaphoreHandle_t cerrojo = nullptr;
static FuerzaG fuerza(REGLAS);
static volatile bool estaParado = false;
static volatile uint32_t fallidas = 0;
static bool listo = false;

static bool escribir(uint8_t registro, uint8_t valor) {
  uint8_t datos[] = {registro, valor};
  return i2c_master_transmit(sensor, datos, sizeof(datos), TIMEOUT_I2C_MS) == ESP_OK;
}

static bool leer(uint8_t registro, uint8_t *datos, size_t cantidad) {
  return i2c_master_transmit_receive(sensor, &registro, 1, datos, cantidad, TIMEOUT_I2C_MS) ==
         ESP_OK;
}

static bool falla(const char *que) {
  Serial.printf("IMU: no se pudo iniciar (%s)\n", que);
  return false;
}

static int16_t entero(const uint8_t *datos) {
  return (int16_t)(datos[1] << 8 | datos[0]);
}

static void alLeer(void *) {
  TickType_t proxima = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&proxima, pdMS_TO_TICKS(PERIODO_IMU_MS));
    uint8_t datos[6];
    if (!leer(AX_L, datos, sizeof(datos))) {
      fallidas = fallidas + 1;
      continue;
    }
    Vector a = {entero(datos) * G_POR_CUENTA, entero(datos + 2) * G_POR_CUENTA,
                entero(datos + 4) * G_POR_CUENTA};
    uint32_t ahora = millis();
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    fuerza.medir(a, ahora, estaParado);
    xSemaphoreGive(cerrojo);
  }
}

bool Imu::iniciar() {
  if (busPlaca() == nullptr) {
    return falla("sin el I2C de la placa");
  }
  if (i2c_master_probe(busPlaca(), DIRECCION_IMU, TIMEOUT_I2C_MS) != ESP_OK) {
    return falla("el QMI8658 no contesta");
  }
  i2c_device_config_t config = {};
  config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  config.scl_speed_hz = VELOCIDAD_I2C_HZ;
  config.device_address = DIRECCION_IMU;
  if (i2c_master_bus_add_device(busPlaca(), &config, &sensor) != ESP_OK) {
    return falla("el QMI8658 en el I2C");
  }

  // Desde cero, por si quedó configurado de antes de un reinicio de la placa
  escribir(REINICIO, REINICIAR);
  uint8_t valor = 0;
  uint32_t inicio = millis();
  do {
    delay(5);
  } while (!(leer(RESULTADO_REINICIO, &valor, 1) && valor == REINICIADO) &&
           millis() - inicio < REINICIO_MS);
  if (valor != REINICIADO) {
    return falla("no termino de reiniciarse");
  }
  if (!leer(QUIEN_SOY, &valor, 1) || valor != ES_QMI8658) {
    return falla("no es un QMI8658");
  }
  if (!escribir(CTRL1, SERIE) || !escribir(CTRL2, ACELEROMETRO) || !escribir(CTRL5, FILTRO) ||
      !escribir(CTRL7, SENSORES)) {
    return falla("no se pudo configurar");
  }
  delay(ARRANQUE_MS);

  cerrojo = xSemaphoreCreateMutex();
  // En el núcleo 0, como el sonido: loop() y la pantalla van en el 1
  if (cerrojo == nullptr ||
      xTaskCreatePinnedToCore(alLeer, "imu", 3072, nullptr, 2, nullptr, 0) != pdPASS) {
    return falla("la tarea");
  }
  listo = true;
  Serial.println("IMU: listo");
  return true;
}

bool Imu::iniciado() const {
  return listo;
}

LecturaG Imu::lectura(uint32_t ahora) {
  if (!listo) {
    return LecturaG();
  }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  LecturaG l = fuerza.lectura(ahora);
  xSemaphoreGive(cerrojo);
  return l;
}

void Imu::parado(bool siONo) {
  estaParado = siONo;
}

void Imu::reiniciarMaximos() {
  if (listo) {
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    fuerza.reiniciarMaximos();
    xSemaphoreGive(cerrojo);
  }
}

bool Imu::cruda(Vector *a) {
  if (!listo) {
    return false;
  }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  bool hay = fuerza.cruda(a);
  xSemaphoreGive(cerrojo);
  return hay;
}

uint32_t Imu::calibraciones() {
  if (!listo) {
    return 0;
  }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  uint32_t cantidad = fuerza.calibraciones();
  xSemaphoreGive(cerrojo);
  return cantidad;
}

uint32_t Imu::lecturasFallidas() const {
  return fallidas;
}
