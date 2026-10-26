#include "pantalla_hw.h"

#include <Arduino.h>
#include <axs15231b/esp_lcd_axs15231b.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <freertos/semphr.h>
#include <lvgl.h>

#include "config.h"

// A partir de lvgl_port.c del ejemplo 10_LVGL_V9_Test de Waveshare.
//
// El panel es vertical y en QSPI no se elige la fila de inicio (el driver
// solo manda CASET): la imagen va siempre entera, de arriba abajo, en
// tandas de FILAS_TANDA filas.
static const int ANCHO_PANEL = 172;
static const int ALTO_PANEL = 640;
static const int ANCHO = ALTO_PANEL; // lo que ve LVGL
static const int ALTO = ANCHO_PANEL;
static const int FILAS_TANDA = 64;
static const size_t BYTES_TANDA = ANCHO_PANEL * FILAS_TANDA * sizeof(uint16_t);
static const size_t BYTES_IMAGEN = ANCHO * ALTO * sizeof(uint16_t);

static const spi_host_device_t SPI_PANEL = SPI3_HOST;
static const int RELOJ_PANEL_HZ = 40 * 1000 * 1000;
static const i2c_port_num_t PUERTO_TOUCH = I2C_NUM_1;
static const uint32_t RELOJ_TOUCH_HZ = 300000;
static const int TIMEOUT_TOUCH_MS = 20;
// Si una tanda no se terminó de mandar en esto, se sigue igual
static const uint32_t TIMEOUT_TANDA_MS = 100;
// La tarea que atiende LVGL mientras loop() bloquea: entre una atención y
// la siguiente deja correr a las demás
static const uint32_t PILA_APARTE = 8192;
static const uint32_t PAUSA_APARTE_MS = 5;

// Los del ejemplo: salir del modo sueño y prender la imagen. El driver
// manda antes MADCTL y COLMOD.
static const uint8_t SIN_DATOS[] = {0x00};
static const axs15231b_lcd_init_cmd_t COMANDOS_PANEL[] = {
    {0x11, SIN_DATOS, 0, 100},
    {0x29, SIN_DATOS, 0, 100},
};

static bool lista = false;
static uint32_t ultimaAtencion = 0;
static uint32_t hueco = 0;
// LVGL: lo tiene loop(), salvo entre soltar() y tomar()
static SemaphoreHandle_t cerrojo = nullptr;

static esp_lcd_panel_handle_t panel = nullptr;
static i2c_master_dev_handle_t touch = nullptr;
static lv_display_t *display = nullptr;

static uint16_t *imagen = nullptr; // la de LVGL, en la PSRAM
// Dos búferes con DMA en la RAM interna: se prepara uno mientras se manda
// el otro. El semáforo cuenta los libres; lo da la interrupción de fin de envío.
static uint16_t *tandas[2] = {nullptr, nullptr};
static int proximaTanda = 0;
static SemaphoreHandle_t tandasLibres = nullptr;
static bool avisoTanda = false;
static uint32_t ultimoEnvioMs = 0;

static bool tocando = false;
static uint16_t crudoX = 0;
static uint16_t crudoY = 0;
static bool touchFallando = false;

static bool falla(const char *que, esp_err_t err) {
  Serial.printf("Pantalla: fallo %s: %s\n", que, esp_err_to_name(err));
  return false;
}

static bool alTerminarTanda(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *) {
  BaseType_t despertar = pdFALSE;
  xSemaphoreGiveFromISR(tandasLibres, &despertar);
  return despertar == pdTRUE;
}

// Copia las filas [fila, fila + FILAS_TANDA) del panel desde la imagen
// horizontal: cada columna de la imagen es una fila del panel. De paso da
// vuelta los bytes de cada píxel (el panel espera primero el alto).
static void prepararTanda(int fila, uint16_t *destino) {
  for (int y = 0; y < ALTO; y++) {
    const uint16_t *origen = imagen + y * ANCHO;
    int columna = PANTALLA_GIRADA ? y : ANCHO_PANEL - 1 - y;
    for (int i = 0; i < FILAS_TANDA; i++) {
      int x = PANTALLA_GIRADA ? ANCHO - 1 - (fila + i) : fila + i;
      uint16_t p = origen[x];
      destino[i * ANCHO_PANEL + columna] = (uint16_t)((p >> 8) | (p << 8));
    }
  }
}

static void enviarImagen() {
  for (int fila = 0; fila < ALTO_PANEL; fila += FILAS_TANDA) {
    if (xSemaphoreTake(tandasLibres, pdMS_TO_TICKS(TIMEOUT_TANDA_MS)) != pdTRUE && !avisoTanda) {
      Serial.println("Pantalla: una tanda no termino de mandarse");
      avisoTanda = true;
    }
    uint16_t *tanda = tandas[proximaTanda];
    proximaTanda ^= 1;
    prepararTanda(fila, tanda);
    // No espera: la interrupción avisa cuando se mandó
    if (esp_lcd_panel_draw_bitmap(panel, 0, fila, ANCHO_PANEL, fila + FILAS_TANDA, tanda) !=
        ESP_OK) {
      xSemaphoreGive(tandasLibres);
    }
  }
}

static void alDibujar(lv_display_t *disp, const lv_area_t *, uint8_t *) {
  // Modo directo: LVGL dibuja en la imagen entera solo lo que cambió. Se
  // manda una vez, al terminar la última zona.
  if (lv_display_flush_is_last(disp)) {
    uint32_t inicio = millis();
    enviarImagen();
    ultimoEnvioMs = millis() - inicio;
  }
  lv_display_flush_ready(disp);
}

// Como el ejemplo: se piden 14 bytes. En [1] viene la cantidad de dedos y
// en [2..5], x e y de 12 bits.
static bool leerTouch(uint16_t &x, uint16_t &y) {
  static const uint8_t pedido[] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00,
                                   0x00, 0x0E, 0x00, 0x00, 0x00};
  uint8_t datos[14] = {};
  esp_err_t err = i2c_master_transmit_receive(touch, pedido, sizeof(pedido), datos, sizeof(datos),
                                              TIMEOUT_TOUCH_MS);
  if (err != ESP_OK) {
    if (!touchFallando) {
      Serial.printf("Touch: no responde (%s)\n", esp_err_to_name(err));
      touchFallando = true;
    }
    return false;
  }
  if (touchFallando) {
    Serial.println("Touch: responde de nuevo");
    touchFallando = false;
  }
  if (datos[1] == 0 || datos[1] > 4) {
    return false;
  }
  x = ((datos[2] & 0x0F) << 8) | datos[3];
  y = ((datos[4] & 0x0F) << 8) | datos[5];
  return true;
}

// El crudo va a lo largo del panel. Se pasa a vertical como en el ejemplo
// y de ahí a horizontal igual que la imagen.
static void alLeerToque(lv_indev_t *, lv_indev_data_t *data) {
  uint16_t x;
  uint16_t y;
  tocando = leerTouch(x, y);
  if (!tocando) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  crudoX = x;
  crudoY = y;
  int columna = min<int>(y, ANCHO_PANEL - 1);
  int fila = min<int>(ALTO_PANEL - min<int>(x, ALTO_PANEL), ALTO_PANEL - 1);
  if (PANTALLA_GIRADA) {
    data->point.x = ANCHO - 1 - fila;
    data->point.y = columna;
  } else {
    data->point.x = fila;
    data->point.y = ALTO - 1 - columna;
  }
  data->state = LV_INDEV_STATE_PRESSED;
}

static void alRegistrar(lv_log_level_t, const char *texto) {
  Serial.print("LVGL: ");
  Serial.print(texto);
}

static uint32_t milisegundos() {
  return millis();
}

// Con LVGL tomado
static void atenderLvgl() {
  uint32_t ahora = millis();
  if (ultimaAtencion != 0 && ahora - ultimaAtencion > hueco) {
    hueco = ahora - ultimaAtencion;
  }
  ultimaAtencion = ahora;
  lv_timer_handler();
}

// Corre solo cuando loop() soltó LVGL; el resto del tiempo espera el cerrojo
static void atenderAparte(void *) {
  for (;;) {
    xSemaphoreTakeRecursive(cerrojo, portMAX_DELAY);
    atenderLvgl();
    xSemaphoreGiveRecursive(cerrojo);
    vTaskDelay(pdMS_TO_TICKS(PAUSA_APARTE_MS));
  }
}

static bool iniciarPanel() {
  spi_bus_config_t bus = {};
  bus.sclk_io_num = PIN_PANTALLA_CLK;
  bus.data0_io_num = PIN_PANTALLA_D0;
  bus.data1_io_num = PIN_PANTALLA_D1;
  bus.data2_io_num = PIN_PANTALLA_D2;
  bus.data3_io_num = PIN_PANTALLA_D3;
  bus.data4_io_num = -1;
  bus.data5_io_num = -1;
  bus.data6_io_num = -1;
  bus.data7_io_num = -1;
  bus.max_transfer_sz = BYTES_TANDA;
  esp_err_t err = spi_bus_initialize(SPI_PANEL, &bus, SPI_DMA_CH_AUTO);
  if (err != ESP_OK) {
    return falla("el bus QSPI", err);
  }

  esp_lcd_panel_io_spi_config_t io = {};
  io.cs_gpio_num = PIN_PANTALLA_CS;
  io.dc_gpio_num = -1;
  io.spi_mode = 3;
  io.pclk_hz = RELOJ_PANEL_HZ;
  io.trans_queue_depth = 10;
  io.on_color_trans_done = alTerminarTanda;
  io.lcd_cmd_bits = 32;
  io.lcd_param_bits = 8;
  io.flags.quad_mode = 1;
  esp_lcd_panel_io_handle_t panelIo = nullptr;
  err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_PANEL, &io, &panelIo);
  if (err != ESP_OK) {
    return falla("el IO del panel", err);
  }

  axs15231b_vendor_config_t fabricante = {};
  fabricante.init_cmds = COMANDOS_PANEL;
  fabricante.init_cmds_size = sizeof(COMANDOS_PANEL) / sizeof(COMANDOS_PANEL[0]);
  fabricante.flags.use_qspi_interface = 1;
  esp_lcd_panel_dev_config_t dispositivo = {};
  dispositivo.reset_gpio_num = -1;
  dispositivo.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  dispositivo.bits_per_pixel = 16;
  dispositivo.vendor_config = &fabricante;
  err = esp_lcd_new_panel_axs15231b(panelIo, &dispositivo, &panel);
  if (err != ESP_OK) {
    return falla("el driver del panel", err);
  }

  // Reset por hardware, con los tiempos del ejemplo
  pinMode(PIN_PANTALLA_RST, OUTPUT);
  digitalWrite(PIN_PANTALLA_RST, HIGH);
  delay(30);
  digitalWrite(PIN_PANTALLA_RST, LOW);
  delay(250);
  digitalWrite(PIN_PANTALLA_RST, HIGH);
  delay(30);
  err = esp_lcd_panel_init(panel);
  if (err != ESP_OK) {
    return falla("la inicializacion del panel", err);
  }
  return true;
}

static bool iniciarTouch() {
  i2c_master_bus_config_t bus = {};
  bus.i2c_port = PUERTO_TOUCH;
  bus.sda_io_num = (gpio_num_t)PIN_TOUCH_SDA;
  bus.scl_io_num = (gpio_num_t)PIN_TOUCH_SCL;
  bus.clk_source = I2C_CLK_SRC_DEFAULT;
  bus.glitch_ignore_cnt = 7;
  bus.flags.enable_internal_pullup = true;
  i2c_master_bus_handle_t manejador = nullptr;
  esp_err_t err = i2c_new_master_bus(&bus, &manejador);
  if (err != ESP_OK) {
    return falla("el I2C del touch", err);
  }

  i2c_device_config_t dispositivo = {};
  dispositivo.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  dispositivo.device_address = DIRECCION_TOUCH;
  dispositivo.scl_speed_hz = RELOJ_TOUCH_HZ;
  err = i2c_master_bus_add_device(manejador, &dispositivo, &touch);
  if (err != ESP_OK) {
    return falla("el touch", err);
  }
  return true;
}

static void iniciarLvgl() {
  lv_init();
  lv_tick_set_cb(milisegundos);
  lv_log_register_print_cb(alRegistrar);

  display = lv_display_create(ANCHO, ALTO);
  lv_display_set_flush_cb(display, alDibujar);
  lv_display_set_buffers(display, imagen, nullptr, BYTES_IMAGEN, LV_DISPLAY_RENDER_MODE_DIRECT);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, alLeerToque);

  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
}

bool PantallaHw::iniciar() {
  // Apagada mientras el panel muestra basura (en la placa del DS3 no se apaga)
  pinMode(PIN_RETROILUMINACION, OUTPUT);
  digitalWrite(PIN_RETROILUMINACION, HIGH);

  tandasLibres = xSemaphoreCreateCounting(2, 2);
  imagen = (uint16_t *)heap_caps_calloc(1, BYTES_IMAGEN, MALLOC_CAP_SPIRAM);
  for (uint16_t *&tanda : tandas) {
    tanda = (uint16_t *)heap_caps_malloc(BYTES_TANDA, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  }
  if (tandasLibres == nullptr || imagen == nullptr || tandas[0] == nullptr ||
      tandas[1] == nullptr) {
    Serial.println("Pantalla: falta memoria");
    return false;
  }

  if (!iniciarPanel() || !iniciarTouch()) {
    return false;
  }
  iniciarLvgl();
  cerrojo = xSemaphoreCreateRecursiveMutex();
  if (cerrojo == nullptr) {
    Serial.println("Pantalla: falta memoria");
    return false;
  }
  xSemaphoreTakeRecursive(cerrojo, portMAX_DELAY);
  if (xTaskCreatePinnedToCore(atenderAparte, "pantalla", PILA_APARTE, nullptr, 1, nullptr,
                              ARDUINO_RUNNING_CORE) != pdPASS) {
    Serial.println("Pantalla: no se pudo crear la tarea; mientras conecta, queda quieta");
  }
  lista = true;

  // Primero el negro y después la luz, al máximo
  lv_refr_now(display);
  digitalWrite(PIN_RETROILUMINACION, LOW);
  return true;
}

bool PantallaHw::iniciada() const {
  return lista;
}

void PantallaHw::atender() {
  if (!lista) {
    return;
  }
  tomar();
  atenderLvgl();
  soltar();
}

void PantallaHw::soltar() {
  if (cerrojo != nullptr) {
    xSemaphoreGiveRecursive(cerrojo);
  }
}

void PantallaHw::tomar() {
  if (cerrojo != nullptr) {
    xSemaphoreTakeRecursive(cerrojo, portMAX_DELAY);
  }
}

uint32_t PantallaHw::huecoMaximo() {
  uint32_t maximo = hueco;
  hueco = 0;
  return maximo;
}

void PantallaHw::dibujarYa() {
  if (lista) {
    tomar();
    lv_refr_now(display);
    soltar();
  }
}

uint32_t PantallaHw::medirRefresco(uint32_t &envio) {
  envio = 0;
  if (!lista) {
    return 0;
  }
  tomar();
  lv_obj_invalidate(lv_screen_active());
  uint32_t inicio = millis();
  lv_refr_now(display);
  envio = ultimoEnvioMs;
  soltar();
  return millis() - inicio;
}

bool PantallaHw::toqueCrudo(uint16_t &x, uint16_t &y) const {
  x = crudoX;
  y = crudoY;
  return tocando;
}
