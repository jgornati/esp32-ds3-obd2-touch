#include "pantalla_hw.h"

#include <Arduino.h>
#include <axs15231b/esp_lcd_axs15231b.h>
#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <driver/spi_master.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_timer.h>
#include <freertos/semphr.h>
#include <lvgl.h>

#include "config.h"
#include "giro.h"
#include "placa.h"

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
// La luz, con el LEDC del ESP-IDF como el programa de fábrica: 8 bits
static const ledc_mode_t MODO_LUZ = LEDC_LOW_SPEED_MODE;
static const ledc_timer_t TIMER_LUZ = LEDC_TIMER_3;
static const ledc_channel_t CANAL_LUZ = LEDC_CHANNEL_1;
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
static uint8_t brilloActual = 0;
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

// Lo que tarda cada cuadro (E17). Al empezar a dibujar, LVGL avisa con
// LV_EVENT_REFR_START.
static int64_t inicioCuadro = 0;
static TiemposCuadro ultimoCuadro = {};
// Los cuadros del segundo en curso: cuántos, la suma y el máximo de cada tiempo
static uint32_t cuadros = 0;
static TiemposCuadro suma = {};
static TiemposCuadro maximo = {};
static uint32_t inicioSegundo = 0;
static MedidasPantalla medidasSegundo = {};

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

static uint32_t microsDesde(int64_t inicio) {
  return (uint32_t)(esp_timer_get_time() - inicio);
}

static void sumar(uint32_t &total, uint32_t &mayor, uint32_t valor) {
  total += valor;
  if (valor > mayor) {
    mayor = valor;
  }
}

static void contarCuadro(const TiemposCuadro &t) {
  ultimoCuadro = t;
  cuadros++;
  sumar(suma.dibujo, maximo.dibujo, t.dibujo);
  sumar(suma.espera, maximo.espera, t.espera);
  sumar(suma.giro, maximo.giro, t.giro);
  sumar(suma.envio, maximo.envio, t.envio);
}

// Con LVGL tomado: cada segundo, lo juntado pasa a medidasSegundo
static void cerrarSegundo() {
  uint32_t ahora = millis();
  uint32_t lapso = ahora - inicioSegundo;
  if (lapso < 1000) {
    return;
  }
  MedidasPantalla m = {};
  m.cuadros = (cuadros * 1000 + lapso / 2) / lapso;
  if (cuadros > 0) {
    m.promedio = {suma.dibujo / cuadros, suma.espera / cuadros, suma.giro / cuadros,
                  suma.envio / cuadros};
    m.maximo = maximo;
  }
  medidasSegundo = m;
  cuadros = 0;
  suma = {};
  maximo = {};
  inicioSegundo = ahora;
}

static void enviarImagen(TiemposCuadro &t) {
  int64_t inicio = esp_timer_get_time();
  for (int fila = 0; fila < ALTO_PANEL; fila += FILAS_TANDA) {
    if (xSemaphoreTake(tandasLibres, pdMS_TO_TICKS(TIMEOUT_TANDA_MS)) != pdTRUE && !avisoTanda) {
      Serial.println("Pantalla: una tanda no termino de mandarse");
      avisoTanda = true;
    }
    uint16_t *tanda = tandas[proximaTanda];
    proximaTanda ^= 1;
    int64_t antes = esp_timer_get_time();
    girarTanda(imagen, ANCHO, ALTO, fila, FILAS_TANDA, PANTALLA_GIRADA, tanda);
    t.giro += microsDesde(antes);
    // No espera: la interrupción avisa cuando se mandó
    if (esp_lcd_panel_draw_bitmap(panel, 0, fila, ANCHO_PANEL, fila + FILAS_TANDA, tanda) !=
        ESP_OK) {
      xSemaphoreGive(tandasLibres);
    }
  }
  // El envío termina cuando el panel recibió la última tanda
  int libres = 0;
  while (libres < 2 && xSemaphoreTake(tandasLibres, pdMS_TO_TICKS(TIMEOUT_TANDA_MS)) == pdTRUE) {
    libres++;
  }
  while (libres-- > 0) {
    xSemaphoreGive(tandasLibres);
  }
  t.envio = microsDesde(inicio);
}

static void alEmpezarCuadro(lv_event_t *) {
  inicioCuadro = esp_timer_get_time();
}

static void alDibujar(lv_display_t *disp, const lv_area_t *, uint8_t *) {
  // Modo directo: LVGL dibuja en la imagen entera solo lo que cambió. Se
  // manda una vez, al terminar la última zona.
  if (lv_display_flush_is_last(disp)) {
    TiemposCuadro t = {};
    t.dibujo = microsDesde(inicioCuadro);
    enviarImagen(t);
    // Mientras se manda, quien tiene LVGL espera
    t.espera = t.envio;
    contarCuadro(t);
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
  cerrarSegundo();
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

  // Reset por hardware, por el TCA9554, con los tiempos del ejemplo. Sin el
  // expansor no se hace: al prender la placa el panel arranca reseteado.
  if (salidaExpansor(PIN_EXPANSOR_RESET_PANTALLA, true)) {
    delay(30);
    salidaExpansor(PIN_EXPANSOR_RESET_PANTALLA, false);
    delay(250);
    salidaExpansor(PIN_EXPANSOR_RESET_PANTALLA, true);
    delay(30);
  } else {
    Serial.println("Pantalla: sin el TCA9554, sin reset del panel");
  }
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
  lv_display_add_event_cb(display, alEmpezarCuadro, LV_EVENT_REFR_START, nullptr);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, alLeerToque);

  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
}

// El PWM de la luz, apagada, y el regulador apagado
static bool iniciarLuz() {
  ledc_timer_config_t timer = {};
  timer.speed_mode = MODO_LUZ;
  timer.duty_resolution = LEDC_TIMER_8_BIT;
  timer.timer_num = TIMER_LUZ;
  timer.freq_hz = FRECUENCIA_RETROILUMINACION_HZ;
  timer.clk_cfg = LEDC_AUTO_CLK;
  ledc_channel_config_t canal = {};
  canal.gpio_num = PIN_RETROILUMINACION;
  canal.speed_mode = MODO_LUZ;
  canal.channel = CANAL_LUZ;
  canal.timer_sel = TIMER_LUZ;
  canal.duty = 255; // en alto, apagada
  salidaExpansor(PIN_EXPANSOR_LUZ, false);
  return ledc_timer_config(&timer) == ESP_OK && ledc_channel_config(&canal) == ESP_OK;
}

bool PantallaHw::iniciar(uint8_t brillo) {
  // Apagada mientras el panel muestra basura
  if (!iniciarLuz()) {
    Serial.println("Pantalla: no se pudo iniciar el PWM de la luz");
  }

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

  // Primero el negro y después la luz
  lv_refr_now(display);
  ponerBrillo(brillo);
  salidaExpansor(PIN_EXPANSOR_LUZ, true);
  return true;
}

void PantallaHw::ponerBrillo(uint8_t nivel) {
  brilloActual = nivel;
  // En bajo, al máximo
  ledc_set_duty(MODO_LUZ, CANAL_LUZ, 255 - nivel);
  ledc_update_duty(MODO_LUZ, CANAL_LUZ);
}

uint8_t PantallaHw::brillo() const {
  return brilloActual;
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

TiemposCuadro PantallaHw::medirRefresco() {
  if (!lista) {
    return {};
  }
  tomar();
  lv_obj_invalidate(lv_screen_active());
  lv_refr_now(display);
  TiemposCuadro t = ultimoCuadro;
  soltar();
  return t;
}

MedidasPantalla PantallaHw::medidas() const {
  return medidasSegundo;
}

bool PantallaHw::toqueCrudo(uint16_t &x, uint16_t &y) const {
  x = crudoX;
  y = crudoY;
  return tocando;
}
