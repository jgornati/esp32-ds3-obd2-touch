#include "elm327.h"

#include <BLEDevice.h>
#include <BLESecurity.h>
#include <BLEUtils.h>
#include <Preferences.h>
#include <comandos.h>
#include <esp_timer.h>
#include <freertos/stream_buffer.h>

#include <atomic>

#include "config.h"

// En el ESP32-S3, Arduino-ESP32 3.3 trae NimBLE, no Bluedroid. Se usan
// funciones suyas (ble_gap_conn_cancel, ble_gap_conn_find, ble_store_clear)
// y subscribe(), que es solo de NimBLE.
#if !defined(CONFIG_NIMBLE_ENABLED)
#error "elm327.cpp espera el stack NimBLE"
#endif

// Lo que pase de este largo se descarta hasta el '>'.
static const size_t LARGO_MAXIMO_RESPUESTA = 1024;
static const size_t TAMANIO_BUFFER = 4096;
static const uint32_t ESPERA_DESCONEXION_MS = 2000;

static BLEClient *cliente = nullptr;
static BLERemoteCharacteristic *escritura = nullptr;
static bool conRespuesta = false; // la característica no acepta WRITE_NR
static bool listo = false;        // conectado y suscripto a las notificaciones
static esp_timer_handle_t vencimiento = nullptr;
static uint16_t intervaloInformado = 0;

// Los completan los callbacks del stack BLE, que corren en otra tarea.
// Las notificaciones se copian a un StreamBuffer; enviar() es lo único que lo lee.
static StreamBufferHandle_t buffer = nullptr;
static std::atomic<uint32_t> perdidos{0};
static std::atomic<bool> seDesconecto{false};
static std::atomic<uint32_t> emparejamientos{0};
static std::atomic<int> estadoEmparejamiento{0};
static uint32_t emparejamientosInformados = 0;
static std::atomic<uint32_t> pedidosParametros{0};
static std::atomic<uint16_t> intervaloPedido{0};
static uint32_t pedidosInformados = 0;

// Búsqueda por nombre o servicio
static BLEUUID servicioElm;
static std::atomic<bool> encontrado{false};
static std::atomic<bool> busquedaTerminada{false};
static String direccionEncontrada;
static uint8_t tipoEncontrado = 0;
static int rssiEncontrado = 0;

static void alNotificar(BLERemoteCharacteristic *, uint8_t *datos, size_t largo, bool) {
  size_t copiados = xStreamBufferSend(buffer, datos, largo, 0);
  if (copiados < largo) {
    perdidos += largo - copiados;
  }
}

class AlCliente : public BLEClientCallbacks {
  void onDisconnect(BLEClient *) override {
    seDesconecto = true;
  }
  // Si se acepta, la biblioteca le contesta con los parámetros con los que
  // conectó (30-50 ms) y se pierde el intervalo corto. Se rechaza y se avisa.
  bool onConnParamsUpdateRequest(BLEClient *, const ble_gap_upd_params *pedido) override {
    intervaloPedido = pedido->itvl_max;
    pedidosParametros++;
    return false;
  }
};

// Solo se empareja si el adaptador lo pide (o si rechaza una operación por
// falta de cifrado). Esto lo avisa, salga bien o mal.
class AlEmparejar : public BLESecurityCallbacks {
  using BLESecurityCallbacks::onAuthenticationComplete;
  void onAuthenticationComplete(ble_gap_conn_desc *, int estado) override {
    estadoEmparejamiento = estado;
    emparejamientos++;
  }
};

class AlBuscar : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice anuncio) override {
    if (encontrado) {
      return;
    }
    String nombre = anuncio.haveName() ? anuncio.getName() : String();
    // El adaptador del DS3 rellena el nombre con bytes nulos
    while (nombre.length() > 0 && nombre[nombre.length() - 1] == '\0') {
      nombre.remove(nombre.length() - 1);
    }
    if (!nombre.equalsIgnoreCase(BLE_NOMBRE_REMOTO) && !anuncio.isAdvertisingService(servicioElm)) {
      return;
    }
    direccionEncontrada = anuncio.getAddress().toString();
    tipoEncontrado = anuncio.getAddressType();
    rssiEncontrado = anuncio.getRSSI();
    encontrado = true;
  }
};

static AlCliente alCliente;
static AlEmparejar alEmparejar;
static AlBuscar alBuscar;

static void alTerminarBusqueda(BLEScanResults) {
  busquedaTerminada = true;
}

// BLEClient::connect() con NimBLE ignora el timeout que se le pasa y espera
// hasta 30 s: este temporizador cancela el intento.
static void alVencer(void *) {
  ble_gap_conn_cancel();
}

// Muestra \r, \n y demás caracteres de control de forma visible.
static void imprimirVisible(const char *texto, size_t largo) {
  for (size_t i = 0; i < largo; i++) {
    unsigned char c = (unsigned char)texto[i];
    if (c == '\r') {
      Serial.print("\\r");
    } else if (c == '\n') {
      Serial.print("\\n");
    } else if (c < 0x20 || c > 0x7E) {
      Serial.printf("\\x%02X", c);
    } else {
      Serial.print((char)c);
    }
  }
}

static void imprimirComando(const char *comando) {
  Serial.print(">> ");
  imprimirVisible(comando, strlen(comando));
  Serial.println();
}

// La dirección va como texto: con NimBLE, BLEAddress da vuelta los bytes al
// construirla desde un arreglo y getNative() no.
static bool leerDireccion(String &direccion, uint8_t &tipo) {
  Preferences prefs;
  if (!prefs.begin("obd", true)) {
    return false;
  }
  bool ok = prefs.isKey("dir");
  if (ok) {
    direccion = prefs.getString("dir");
    tipo = prefs.getUChar("tipo", BLE_ADDR_PUBLIC);
  }
  prefs.end();
  return ok && direccion.length() == 17;
}

static void guardarDireccion(const String &direccion, uint8_t tipo) {
  Preferences prefs;
  if (prefs.begin("obd", false)) {
    prefs.putString("dir", direccion);
    prefs.putUChar("tipo", tipo);
    prefs.end();
  }
  Serial.printf("BLE: direccion guardada %s (tipo %u)\n", direccion.c_str(), tipo);
}

static bool conectarA(const String &direccion, uint8_t tipo) {
  esp_timer_start_once(vencimiento, (uint64_t)TIMEOUT_CONEXION_MS * 1000);
  bool ok = cliente->connect(BLEAddress(direccion, tipo), tipo);
  esp_timer_stop(vencimiento);
  return ok;
}

static void esperarDesconexion() {
  uint32_t inicio = millis();
  while (cliente->isConnected() && millis() - inicio < ESPERA_DESCONEXION_MS) {
    delay(10);
  }
}

static void informarEventos() {
  uint32_t n = emparejamientos;
  if (n != emparejamientosInformados) {
    emparejamientosInformados = n;
    int estado = estadoEmparejamiento;
    if (estado == 0) {
      Serial.println("BLE: el adaptador pidio emparejamiento; se hizo sin clave y sin guardar");
    } else {
      Serial.printf("BLE: el adaptador pidio emparejamiento y fallo: %d %s\n", estado,
                    BLEUtils::returnCodeToString(estado));
    }
  }

  n = pedidosParametros;
  if (n != pedidosInformados) {
    pedidosInformados = n;
    Serial.printf("BLE: el adaptador pidio un intervalo de hasta %.2f ms; se rechazo\n",
                  intervaloPedido * 1.25f);
  }

  ble_gap_conn_desc desc;
  if (cliente != nullptr && cliente->isConnected() &&
      ble_gap_conn_find(cliente->getConnId(), &desc) == 0 && desc.conn_itvl != intervaloInformado) {
    intervaloInformado = desc.conn_itvl;
    // NimBLE da el intervalo en unidades de 1,25 ms y la supervisión en 10 ms
    Serial.printf("BLE: intervalo %.2f ms, latencia %u, supervision %u ms\n",
                  desc.conn_itvl * 1.25f, desc.conn_latency, desc.supervision_timeout * 10);
  }

  uint32_t p = perdidos.exchange(0);
  if (p > 0) {
    Serial.printf("BLE: %lu bytes no entraron en el buffer\n", (unsigned long)p);
  }
}

const char *nombreEstado(EstadoTransporte estado) {
  switch (estado) {
  case EstadoTransporte::OK:
    return "OK";
  case EstadoTransporte::TIMEOUT:
    return "TIMEOUT";
  case EstadoTransporte::DESCONECTADO:
    return "DESCONECTADO";
  case EstadoTransporte::RECHAZADO:
    return "RECHAZADO";
  }
  return "?";
}

bool Elm327::iniciar() {
  if (iniciado) {
    return true;
  }
  buffer = xStreamBufferCreate(TAMANIO_BUFFER, 1);
  esp_timer_create_args_t temporizador = {};
  temporizador.callback = alVencer;
  temporizador.name = "conexion_ble";
  if (buffer == nullptr || esp_timer_create(&temporizador, &vencimiento) != ESP_OK ||
      !BLEDevice::init(BLE_NOMBRE_LOCAL)) {
    Serial.println("BLE: no se pudo iniciar");
    return false;
  }
  // Si el adaptador pide emparejamiento: sin clave ("Just Works") y sin
  // vínculo, para no guardar claves. Nunca se empareja por iniciativa propia.
  BLESecurity::setAuthenticationMode(false, false, true);
  BLESecurity::setForceAuthentication(false);
  BLEDevice::setSecurityCallbacks(&alEmparejar);

  cliente = BLEDevice::createClient();
  cliente->setClientCallbacks(&alCliente);
  servicioElm = BLEUUID::fromString(BLE_SERVICIO_ELM);
  iniciado = true;
  Serial.printf("BLE: iniciado con %s, direccion propia %s\n",
                BLEDevice::getBLEStackString().c_str(), BLEDevice::getAddress().toString().c_str());
  return true;
}

// Por nombre o por servicio. Mientras busca, llama a espera.
bool Elm327::buscar(uint8_t *tipo, String *direccion) {
  BLEScan *scan = BLEDevice::getScan();
  encontrado = false;
  busquedaTerminada = false;
  scan->setAdvertisedDeviceCallbacks(&alBuscar);
  // Activa: el nombre suele venir en la respuesta de escaneo
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  bool empezo = scan->start(BUSQUEDA_ELM_S, alTerminarBusqueda, false);
  while (empezo && !encontrado && !busquedaTerminada) {
    if (espera != nullptr) {
      espera();
    }
    delay(5);
  }
  if (empezo && !busquedaTerminada) {
    scan->stop();
  }
  scan->setAdvertisedDeviceCallbacks(nullptr);
  scan->clearResults();
  if (!encontrado) {
    return false;
  }
  *direccion = direccionEncontrada;
  *tipo = tipoEncontrado;
  return true;
}

bool Elm327::intentarConexion(void (*aviso)(FaseConexion)) {
  String guardada;
  uint8_t tipoGuardado = BLE_ADDR_PUBLIC;
  bool hayGuardada = leerDireccion(guardada, tipoGuardado);
  if (hayGuardada) {
    if (aviso != nullptr) {
      aviso(FaseConexion::POR_DIRECCION);
    }
    Serial.printf("BLE: conectando a la direccion guardada %s\n", guardada.c_str());
    uint32_t inicio = millis();
    bool ok = conectarA(guardada, tipoGuardado);
    Serial.printf("BLE: %s (%lu ms)\n", ok ? "conectado por direccion" : "no conecto",
                  (unsigned long)(millis() - inicio));
    if (ok) {
      return true;
    }
  }

  if (aviso != nullptr) {
    aviso(FaseConexion::BUSCANDO);
  }
  Serial.printf("BLE: buscando \"%s\" o el servicio %s\n", BLE_NOMBRE_REMOTO, BLE_SERVICIO_ELM);
  uint32_t inicio = millis();
  String direccion;
  uint8_t tipo;
  if (!buscar(&tipo, &direccion)) {
    Serial.printf("BLE: no aparecio (%lu ms)\n", (unsigned long)(millis() - inicio));
    return false;
  }
  Serial.printf("BLE: encontrado %s (%d dBm) en %lu ms\n", direccion.c_str(), rssiEncontrado,
                (unsigned long)(millis() - inicio));
  inicio = millis();
  bool ok = conectarA(direccion, tipo);
  Serial.printf("BLE: %s (%lu ms)\n", ok ? "conectado" : "no conecto",
                (unsigned long)(millis() - inicio));
  if (!ok) {
    return false;
  }
  if (!hayGuardada || direccion != guardada || tipo != tipoGuardado) {
    guardarDireccion(direccion, tipo);
  }
  return true;
}

bool Elm327::prepararConexion() {
  // Antes de leer los servicios, que con 50 ms tardaba ~600 ms
  intervaloInformado = 0;
  if (!cliente->updateConnParams(BLE_INTERVALO_MIN, BLE_INTERVALO_MAX, 0, BLE_SUPERVISION)) {
    Serial.println("BLE: no se pudo pedir el intervalo corto");
  }

  // Una sola vez por conexión: getServices() borra los anteriores
  uint32_t inicio = millis();
  cliente->getServices();
  BLERemoteService *servicio = cliente->getService(BLE_SERVICIO_ELM);
  BLERemoteCharacteristic *notif =
      servicio != nullptr ? servicio->getCharacteristic(BLE_NOTIFICACION_ELM) : nullptr;
  BLERemoteCharacteristic *escr =
      servicio != nullptr ? servicio->getCharacteristic(BLE_ESCRITURA_ELM) : nullptr;
  if (notif == nullptr || escr == nullptr || !notif->canNotify() ||
      !(escr->canWrite() || escr->canWriteNoResponse())) {
    Serial.printf("BLE: el adaptador no tiene el servicio %s con %s (notificaciones) y %s "
                  "(escritura)\n",
                  BLE_SERVICIO_ELM, BLE_NOTIFICACION_ELM, BLE_ESCRITURA_ELM);
    return false;
  }

  xStreamBufferReset(buffer);
  if (!notif->subscribe(true, alNotificar, true)) {
    Serial.println("BLE: no se pudo suscribir a las notificaciones");
    return false;
  }
  escritura = escr;
  conRespuesta = !escr->canWriteNoResponse();
  Serial.printf("BLE: servicios y suscripcion en %lu ms\n", (unsigned long)(millis() - inicio));
  return true;
}

bool Elm327::conectar(void (*aviso)(FaseConexion)) {
  if (!iniciar()) {
    return false;
  }
  if (conectado()) {
    return true;
  }

  informarEventos();
  seDesconecto = false;
  uint32_t inicio = millis();
  if (!intentarConexion(aviso)) {
    return false;
  }
  if (!prepararConexion()) {
    cliente->disconnect();
    esperarDesconexion();
    return false;
  }
  listo = true;
  Serial.printf("BLE: listo en %lu ms (MTU %u)\n", (unsigned long)(millis() - inicio),
                cliente->getMTU());
  return true;
}

void Elm327::olvidar() {
  if (!iniciar()) {
    return;
  }
  desconectar();
  Preferences prefs;
  if (prefs.begin("obd", false)) {
    prefs.remove("dir");
    prefs.remove("tipo");
    prefs.end();
  }
  // Por si alguna vez quedó un vínculo
  ble_store_clear();
  Serial.println("BLE: direccion y vinculos olvidados; la proxima conexion busca de cero");
}

bool Elm327::conectado() {
  if (seDesconecto.exchange(false) || (listo && !cliente->isConnected())) {
    listo = false;
    escritura = nullptr;
  }
  return listo;
}

void Elm327::desconectar() {
  listo = false;
  escritura = nullptr;
  if (iniciado && cliente->isConnected()) {
    cliente->disconnect();
    esperarDesconexion();
  }
  // Pedida acá: no es una pérdida
  seDesconecto = false;
}

String Elm327::direccion() {
  if (!conectado()) {
    return String();
  }
  String texto = cliente->getPeerAddress().toString();
  texto.toUpperCase();
  return texto;
}

int Elm327::rssi() {
  return conectado() ? cliente->getRssi() : 0;
}

ResultadoComando Elm327::enviar(const char *comando, uint32_t timeoutMs, bool registrar) {
  ResultadoComando r{EstadoTransporte::OK, String(), 0};
  uint32_t inicio = millis();
  if (comando == nullptr) {
    comando = "";
  }

  informarEventos();
  if (registrar) {
    imprimirComando(comando);
  }

  // La lista blanca va primero: lo rechazado no toca el BLE.
  if (!comandoPermitido(comando)) {
    r.estado = EstadoTransporte::RECHAZADO;
    if (!registrar) {
      imprimirComando(comando);
    }
    Serial.println("<< RECHAZADO");
    return r;
  }
  if (!conectado()) {
    r.estado = EstadoTransporte::DESCONECTADO;
    if (!registrar) {
      imprimirComando(comando);
    }
    Serial.println("<< DESCONECTADO");
    return r;
  }

  // Lo que haya quedado de un comando anterior (por ejemplo, tras un
  // timeout, o lo que llegó después del '>') se descarta, pero se muestra.
  String descartado;
  char trozo[64];
  size_t n;
  while ((n = xStreamBufferReceive(buffer, trozo, sizeof(trozo), 0)) > 0) {
    if (descartado.length() < LARGO_MAXIMO_RESPUESTA) {
      descartado.concat(trozo, n);
    }
  }
  if (descartado.length() > 0) {
    Serial.print("-- descartado: ");
    imprimirVisible(descartado.c_str(), descartado.length());
    Serial.println();
  }

  String linea = comando;
  linea += '\r';
  if (!escritura->writeValue((uint8_t *)linea.c_str(), linea.length(), conRespuesta)) {
    r.estado = conectado() ? EstadoTransporte::TIMEOUT : EstadoTransporte::DESCONECTADO;
  } else {
    r.estado = EstadoTransporte::TIMEOUT;
    while (millis() - inicio < timeoutMs) {
      char c;
      // Vuelve apenas llega algo; si no, a los 2 ms
      if (xStreamBufferReceive(buffer, &c, 1, pdMS_TO_TICKS(2)) == 0) {
        if (!conectado()) {
          r.estado = EstadoTransporte::DESCONECTADO;
          break;
        }
        if (espera != nullptr) {
          espera();
        }
        continue;
      }
      if (c == '>') {
        r.estado = EstadoTransporte::OK;
        break;
      }
      // Algunos clones mandan bytes nulos sueltos; no sirven y cortarían el texto.
      if (c != '\0' && r.cruda.length() < LARGO_MAXIMO_RESPUESTA) {
        r.cruda += c;
      }
    }
  }
  r.ms = millis() - inicio;

  // Sin registrar, solo se muestra lo que salió mal
  if (!registrar) {
    if (r.estado == EstadoTransporte::OK) {
      return r;
    }
    imprimirComando(comando);
  }
  Serial.print("<< \"");
  imprimirVisible(r.cruda.c_str(), r.cruda.length());
  Serial.printf("\" %s %lu ms\n", nombreEstado(r.estado), (unsigned long)r.ms);
  return r;
}
