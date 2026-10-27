#include "placa.h"

#include <Arduino.h>

#include "config.h"

static const uint32_t VELOCIDAD_I2C_HZ = 300000; // la de los ejemplos de Waveshare
static const int TIMEOUT_I2C_MS = 50;
// Registros del TCA9554
static const uint8_t SALIDAS = 0x01;
static const uint8_t DIRECCIONES = 0x03; // 1: entrada

static i2c_master_bus_handle_t bus = nullptr;
static i2c_master_dev_handle_t expansor = nullptr;

static bool falla(const char *que) {
  Serial.printf("Placa: no se pudo iniciar %s\n", que);
  return false;
}

bool iniciarPlaca() {
  i2c_master_bus_config_t configBus = {};
  configBus.i2c_port = I2C_NUM_0; // el del touch es el 1
  configBus.sda_io_num = (gpio_num_t)PIN_I2C_SDA;
  configBus.scl_io_num = (gpio_num_t)PIN_I2C_SCL;
  configBus.clk_source = I2C_CLK_SRC_DEFAULT;
  configBus.glitch_ignore_cnt = 7;
  configBus.flags.enable_internal_pullup = true;
  if (i2c_new_master_bus(&configBus, &bus) != ESP_OK) {
    bus = nullptr;
    return falla("el I2C");
  }
  i2c_device_config_t configExpansor = {};
  configExpansor.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  configExpansor.scl_speed_hz = VELOCIDAD_I2C_HZ;
  configExpansor.device_address = DIRECCION_EXPANSOR;
  if (i2c_master_bus_add_device(bus, &configExpansor, &expansor) != ESP_OK ||
      i2c_master_probe(bus, DIRECCION_EXPANSOR, TIMEOUT_I2C_MS) != ESP_OK) {
    expansor = nullptr;
    return falla("el TCA9554");
  }
  return true;
}

i2c_master_bus_handle_t busPlaca() {
  return bus;
}

// Lee el registro y cambia solo el bit del pin
static bool cambiarBit(uint8_t registro, uint8_t pin, bool valor) {
  uint8_t actual;
  if (i2c_master_transmit_receive(expansor, &registro, 1, &actual, 1, TIMEOUT_I2C_MS) != ESP_OK) {
    return false;
  }
  uint8_t bit = 1 << pin;
  uint8_t datos[] = {registro, (uint8_t)(valor ? actual | bit : actual & ~bit)};
  return i2c_master_transmit(expansor, datos, sizeof(datos), TIMEOUT_I2C_MS) == ESP_OK;
}

bool salidaExpansor(uint8_t pin, bool nivel) {
  return expansor != nullptr && cambiarBit(SALIDAS, pin, nivel) &&
         cambiarBit(DIRECCIONES, pin, false);
}
