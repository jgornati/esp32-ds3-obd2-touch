#include "sonido.h"

#include <Arduino.h>
#include <driver/i2c_master.h>
#include <driver/i2s_std.h>
#include <math.h>

#include "config.h"
#include "placa.h"

// 16 kHz con MCLK de 256 veces (4,096 MHz): una de las combinaciones de la
// tabla del ES8311
static const uint32_t MUESTREO_HZ = 16000;
static const uint32_t VELOCIDAD_I2C_HZ = 300000; // la de los ejemplos de Waveshare
static const int TIMEOUT_I2C_MS = 50;
// Al principio y al final de cada pitido, para que no haga clic
static const uint32_t RAMPA_MS = 5;
// Lo que se genera de una vez: 10 ms, estéreo
static const uint32_t CUADROS_POR_TANDA = MUESTREO_HZ / 100;

static i2c_master_dev_handle_t codec = nullptr;
static i2s_chan_handle_t canal = nullptr;
static TaskHandle_t tarea = nullptr;
static bool listo = false;
// Los pedidos, en bits de la notificación de la tarea: así uno no pisa al otro
static const uint32_t PEDIDO_ALARMA = 1 << 0;
static const uint32_t PEDIDO_CAMBIO = 1 << 1;

static bool escribir(i2c_master_dev_handle_t dispositivo, uint8_t registro, uint8_t valor) {
  uint8_t datos[] = {registro, valor};
  return i2c_master_transmit(dispositivo, datos, sizeof(datos), TIMEOUT_I2C_MS) == ESP_OK;
}

static bool leer(i2c_master_dev_handle_t dispositivo, uint8_t registro, uint8_t *valor) {
  return i2c_master_transmit_receive(dispositivo, &registro, 1, valor, 1, TIMEOUT_I2C_MS) == ESP_OK;
}

// Lee el registro y cambia solo los bits de mascara por los de bits
static bool cambiar(i2c_master_dev_handle_t dispositivo, uint8_t registro, uint8_t mascara,
                    uint8_t bits) {
  uint8_t valor;
  return leer(dispositivo, registro, &valor) &&
         escribir(dispositivo, registro, (valor & ~mascara) | (bits & mascara));
}

static bool escribirTodos(const uint8_t (*lista)[2], size_t cantidad) {
  for (size_t i = 0; i < cantidad; i++) {
    if (!escribir(codec, lista[i][0], lista[i][1])) {
      return false;
    }
  }
  return true;
}

static bool falla(const char *que) {
  Serial.printf("Sonido: no se pudo iniciar (%s)\n", que);
  return false;
}

// El I2S manda MCLK, BCLK y WS siempre; sin pitidos, ceros
static bool iniciarI2s() {
  i2s_chan_config_t configCanal = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  configCanal.auto_clear_after_cb = true;
  if (i2s_new_channel(&configCanal, &canal, nullptr) != ESP_OK) {
    return false;
  }
  i2s_std_config_t config = {};
  config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(MUESTREO_HZ);
  config.slot_cfg =
      I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  config.gpio_cfg.mclk = (gpio_num_t)PIN_I2S_MCLK;
  config.gpio_cfg.bclk = (gpio_num_t)PIN_I2S_BCLK;
  config.gpio_cfg.ws = (gpio_num_t)PIN_I2S_WS;
  config.gpio_cfg.dout = (gpio_num_t)PIN_I2S_SALIDA;
  config.gpio_cfg.din = I2S_GPIO_UNUSED;
  return i2s_channel_init_std_mode(canal, &config) == ESP_OK && i2s_channel_enable(canal) == ESP_OK;
}

// La secuencia del driver de Espressif (esp_codec_dev, es8311.c,
// Apache-2.0) que usa el ejemplo de audio de Waveshare: es8311_open,
// es8311_set_fs con 16 bits y 16 kHz, y es8311_start, solo el DAC, como
// esclavo y con MCLK.
static bool iniciarCodec() {
  // Más inmunidad al ruido en el I2C. Se escribe dos veces: según el
  // driver, la primera escritura a veces falla.
  escribir(codec, 0x44, 0x08);
  static const uint8_t APERTURA[][2] = {{0x44, 0x08},
                                        {0x01, 0x30},
                                        {0x02, 0x00},
                                        {0x03, 0x10},
                                        {0x16, 0x24},
                                        {0x04, 0x10},
                                        {0x05, 0x00},
                                        {0x0B, 0x00},
                                        {0x0C, 0x00},
                                        {0x10, 0x1F},
                                        {0x11, 0x7F},
                                        // Esclavo; el reloj interno sale de MCLK, sin invertir
                                        {0x00, 0x80},
                                        {0x01, 0x3F}};
  bool bien = escribirTodos(APERTURA, sizeof(APERTURA) / sizeof(APERTURA[0]));
  bien = bien && cambiar(codec, 0x06, 0x20, 0x00); // SCLK sin invertir
  static const uint8_t REFERENCIAS[][2] = {{0x13, 0x10}, {0x1B, 0x0A}, {0x1C, 0x6A}, {0x44, 0x08}};
  bien = bien && escribirTodos(REFERENCIAS, sizeof(REFERENCIAS) / sizeof(REFERENCIAS[0]));

  // 16 bits (0x0C) en formato I2S (bits 0 y 1 en 0), en el DAC y el ADC
  bien = bien && cambiar(codec, 0x09, 0x1F, 0x0C) && cambiar(codec, 0x0A, 0x1F, 0x0C);
  // Los divisores de la tabla para 4,096 MHz y 16 kHz
  bien = bien && cambiar(codec, 0x02, 0xF8, 0x00) && escribir(codec, 0x05, 0x00) &&
         cambiar(codec, 0x03, 0x7F, 0x10) && cambiar(codec, 0x04, 0x7F, 0x20) &&
         cambiar(codec, 0x07, 0x3F, 0x00) && escribir(codec, 0x08, 0xFF) &&
         cambiar(codec, 0x06, 0x1F, 0x03);

  // Arranca solo el DAC: el bit 6 en 0 lo prende, en 1 apaga el ADC
  bien = bien && escribir(codec, 0x00, 0x80) && escribir(codec, 0x01, 0x3F) &&
         cambiar(codec, 0x09, 0x40, 0x00) && cambiar(codec, 0x0A, 0x40, 0x40);
  static const uint8_t ARRANQUE[][2] = {{0x17, 0xBF},
                                        {0x0E, 0x02},
                                        {0x12, 0x00},
                                        {0x14, 0x1A},
                                        {0x0D, 0x01},
                                        {0x15, 0x40},
                                        {0x37, 0x08},
                                        {0x45, 0x00},
                                        // Volumen del DAC: 0 dB
                                        {0x32, 0xBF}};
  return bien && escribirTodos(ARRANQUE, sizeof(ARRANQUE) / sizeof(ARRANQUE[0]));
}

// Una tanda de cuadros: el pitido (con fase continua y la rampa) o silencio
static void mandar(int16_t *muestras, uint32_t cuadros) {
  size_t escritos;
  i2s_channel_write(canal, muestras, cuadros * 2 * sizeof(int16_t), &escritos, portMAX_DELAY);
}

static void pitido(uint32_t ms, uint32_t frecuencia) {
  static int16_t muestras[CUADROS_POR_TANDA * 2];
  const uint32_t total = MUESTREO_HZ * ms / 1000;
  const uint32_t rampa = MUESTREO_HZ * RAMPA_MS / 1000;
  const float paso = 2 * (float)M_PI * frecuencia / MUESTREO_HZ;
  for (uint32_t hecho = 0; hecho < total; hecho += CUADROS_POR_TANDA) {
    uint32_t cuadros = min(CUADROS_POR_TANDA, total - hecho);
    for (uint32_t i = 0; i < cuadros; i++) {
      uint32_t n = hecho + i;
      uint32_t borde = min(n, total - 1 - n);
      float envolvente = borde < rampa ? (float)borde / rampa : 1;
      int16_t valor = (int16_t)(32767 * VOLUMEN_PITIDO * envolvente * sinf(paso * n));
      muestras[2 * i] = valor;
      muestras[2 * i + 1] = valor;
    }
    mandar(muestras, cuadros);
  }
}

static void silencio(uint32_t ms) {
  static int16_t ceros[CUADROS_POR_TANDA * 2] = {};
  const uint32_t total = MUESTREO_HZ * ms / 1000;
  for (uint32_t hecho = 0; hecho < total; hecho += CUADROS_POR_TANDA) {
    mandar(ceros, min(CUADROS_POR_TANDA, total - hecho));
  }
}

static void alPitar(void *) {
  for (;;) {
    uint32_t pedidos = 0;
    xTaskNotifyWait(0, UINT32_MAX, &pedidos, portMAX_DELAY);
    if (pedidos & PEDIDO_CAMBIO) {
      pitido(PITIDO_CAMBIO_MS, FRECUENCIA_CAMBIO_HZ);
      silencio(SILENCIO_PITIDO_MS);
    }
    if (pedidos & PEDIDO_ALARMA) {
      for (uint8_t i = 0; i < PITIDOS_ALARMA; i++) {
        pitido(PITIDO_MS, FRECUENCIA_PITIDO_HZ);
        silencio(SILENCIO_PITIDO_MS);
      }
    }
  }
}

bool Sonido::iniciar() {
  if (busPlaca() == nullptr) {
    return falla("sin el I2C de la placa");
  }
  i2c_device_config_t configCodec = {};
  configCodec.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  configCodec.scl_speed_hz = VELOCIDAD_I2C_HZ;
  configCodec.device_address = DIRECCION_CODEC;
  if (i2c_master_bus_add_device(busPlaca(), &configCodec, &codec) != ESP_OK) {
    return falla("el ES8311 en el I2C");
  }

  // Como el ejemplo: el I2S, después el códec y al final el amplificador
  if (!iniciarI2s()) {
    return falla("el I2S");
  }
  if (!iniciarCodec()) {
    return falla("el ES8311 no contesta");
  }
  if (!salidaExpansor(PIN_EXPANSOR_AMPLIFICADOR, true)) {
    return falla("el TCA9554 no contesta");
  }
  if (xTaskCreatePinnedToCore(alPitar, "sonido", 3072, nullptr, 2, &tarea, 0) != pdPASS) {
    return falla("la tarea");
  }
  listo = true;
  Serial.println("Sonido: listo");
  return true;
}

bool Sonido::iniciado() const {
  return listo;
}

void Sonido::pitarAlarma() {
  if (listo) {
    xTaskNotify(tarea, PEDIDO_ALARMA, eSetBits);
  }
}

void Sonido::pitarCambio() {
  if (listo) {
    xTaskNotify(tarea, PEDIDO_CAMBIO, eSetBits);
  }
}
