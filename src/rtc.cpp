#include "rtc.h"

#include <Arduino.h>
#include <driver/i2c_master.h>
#include <string.h>
#include <sys/time.h>

#include "config.h"
#include "placa.h"

static const uint32_t VELOCIDAD_I2C_HZ = 300000; // la de los ejemplos de Waveshare
static const int TIMEOUT_I2C_MS = 50;

// Registros del PCF85063A (datasheet y SensorLib, en el repositorio de la V2)
static const uint8_t CONTROL_1 = 0x00;
static const uint8_t SEGUNDOS = 0x04; // y los 6 que siguen, hasta el año
static const uint8_t DETENIDO = 0x20; // STOP, en CONTROL_1
static const uint8_t DOCE_HORAS = 0x02;

static i2c_master_dev_handle_t chip = nullptr;
static bool listo = false;
static volatile bool conHora = false;

static bool leer(uint8_t registro, uint8_t *datos, size_t cantidad) {
  return i2c_master_transmit_receive(chip, &registro, 1, datos, cantidad, TIMEOUT_I2C_MS) == ESP_OK;
}

// En una sola escritura: el chip congela la hora mientras dura
static bool escribir(uint8_t registro, const uint8_t *datos, size_t cantidad) {
  uint8_t mensaje[1 + REGISTROS_HORA];
  mensaje[0] = registro;
  memcpy(mensaje + 1, datos, cantidad);
  return i2c_master_transmit(chip, mensaje, 1 + cantidad, TIMEOUT_I2C_MS) == ESP_OK;
}

static bool falla(const char *que) {
  Serial.printf("Reloj: no se pudo iniciar (%s)\n", que);
  return false;
}

// La hora local, como si fuera UTC (sin zona horaria): localtime() da lo
// mismo, y la FAT la usa para la fecha de los archivos
static void ponerSistema(const FechaHora &f) {
  timeval tv = {(time_t)segundosDesde1970(f), 0};
  settimeofday(&tv, nullptr);
}

bool Rtc::iniciar() {
  if (busPlaca() == nullptr) {
    return falla("sin el I2C de la placa");
  }
  if (i2c_master_probe(busPlaca(), DIRECCION_RTC, TIMEOUT_I2C_MS) != ESP_OK) {
    return falla("el PCF85063 no contesta");
  }
  i2c_device_config_t config = {};
  config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  config.scl_speed_hz = VELOCIDAD_I2C_HZ;
  config.device_address = DIRECCION_RTC;
  if (i2c_master_bus_add_device(busPlaca(), &config, &chip) != ESP_OK) {
    return falla("el PCF85063 en el I2C");
  }
  setenv("TZ", "UTC0", 1);
  tzset();

  // Andando y en 24 h: así arranca de fábrica. Si estaba en 12 h, la hora
  // guardada no sirve.
  uint8_t control;
  if (!leer(CONTROL_1, &control, 1)) {
    return falla("no se pudo leer");
  }
  bool doceHoras = control & DOCE_HORAS;
  if ((control & (DETENIDO | DOCE_HORAS)) != 0) {
    control &= ~(DETENIDO | DOCE_HORAS);
    if (!escribir(CONTROL_1, &control, 1)) {
      return falla("no se pudo configurar");
    }
  }
  uint8_t registros[REGISTROS_HORA];
  if (!leer(SEGUNDOS, registros, sizeof(registros))) {
    return falla("no se pudo leer la hora");
  }
  FechaHora f;
  if (!doceHoras && leerRegistrosHora(registros, &f)) {
    ponerSistema(f);
    conHora = true;
  }
  listo = true;
  imprimir();
  return true;
}

bool Rtc::poner(const FechaHora &f) {
  if (!listo) {
    return false;
  }
  uint8_t registros[REGISTROS_HORA];
  registrosHora(f, registros);
  if (!escribir(SEGUNDOS, registros, sizeof(registros))) {
    return false;
  }
  ponerSistema(f);
  conHora = true;
  return true;
}

bool Rtc::ahora(FechaHora *f) const {
  if (!conHora) {
    return false;
  }
  timeval tv;
  gettimeofday(&tv, nullptr);
  *f = fechaHoraDe(tv.tv_sec);
  return true;
}

void Rtc::imprimir() const {
  FechaHora f;
  char texto[24];
  if (!listo) {
    Serial.println("Reloj: no anda (el PCF85063 no contesta)");
  } else if (ahora(&f)) {
    textoFechaHora(f, texto, sizeof(texto));
    Serial.printf("Reloj: %s\n", texto);
  } else {
    Serial.println("Reloj: sin hora (se detuvo, por ejemplo sin bateria): ponerla con "
                   "!hora AAAA-MM-DD HH:MM");
  }
}
