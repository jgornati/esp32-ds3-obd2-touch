#include "explorar.h"

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include <BLEUtils.h>
#include <comandos.h>
#include <freertos/message_buffer.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <string>

#include "config.h"

// En el ESP32-S3, Arduino-ESP32 3.3 trae NimBLE, no Bluedroid. Se usan
// funciones suyas (ble_gap_conn_find) y subscribe(), que es solo de NimBLE.
#if !defined(CONFIG_NIMBLE_ENABLED)
#error "explorar.cpp espera el stack NimBLE"
#endif

static const size_t MAX_DISPOSITIVOS = 30;
// Lo que pase de este largo se descarta hasta el '>'
static const size_t LARGO_MAXIMO_RESPUESTA = 1024;
// Una notificación no pasa del MTU (NimBLE pide 256); lo que pase de esto se corta
static const size_t LARGO_MAXIMO_NOTIFICACION = 512;
static const size_t TAMANIO_BUFFER = 4096;

struct Dispositivo {
  BLEAddress direccion;
  uint8_t tipo;
  String nombre;
  String servicios; // UUIDs anunciados, separados por espacios
  int rssi;
  bool pareceObd;
};

static Dispositivo dispositivos[MAX_DISPOSITIVOS];
static size_t cantidad = 0;

static BLEClient *cliente = nullptr;
static int conectadoA = 0; // número en la lista; 0 si no hay conexión
// Se descubren una vez por conexión: getServices() borra los anteriores y
// dejaría colgadas las características en uso.
static std::map<std::string, BLERemoteService *> *servicios = nullptr;
static BLERemoteCharacteristic *notificacion = nullptr;
static BLERemoteCharacteristic *escritura = nullptr;
static bool conRespuesta = false; // la característica no acepta WRITE_NR

// Los completan los callbacks del stack BLE, que corren en otra tarea.
// Cada notificación es un mensaje del buffer, así se sabe su tamaño.
static MessageBufferHandle_t buffer = nullptr;
static std::atomic<uint32_t> perdidas{0};
static volatile bool seDesconecto = false;
static std::atomic<uint32_t> emparejamientos{0};
static volatile int estadoEmparejamiento = 0;
static uint32_t emparejamientosInformados = 0;

static void alNotificar(BLERemoteCharacteristic *, uint8_t *datos, size_t largo, bool) {
  if (largo == 0) {
    return;
  }
  if (largo > LARGO_MAXIMO_NOTIFICACION) {
    largo = LARGO_MAXIMO_NOTIFICACION;
  }
  if (xMessageBufferSend(buffer, datos, largo, 0) != largo) {
    perdidas++;
  }
}

class AlCliente : public BLEClientCallbacks {
  void onDisconnect(BLEClient *) override {
    seDesconecto = true;
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

static AlCliente alCliente;
static AlEmparejar alEmparejar;

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

// "FFF0" si es de 16 bits; si no, el UUID entero. En mayúsculas.
static String textoUuid(BLEUUID uuid) {
  String texto = uuid.toString();
  texto.toUpperCase();
  if (texto.startsWith("0000") && texto.endsWith("-0000-1000-8000-00805F9B34FB")) {
    return texto.substring(4, 8);
  }
  return texto;
}

static bool servicioObd(const String &uuid) {
  for (const char *obd : BLE_SERVICIOS_OBD) {
    if (uuid.equalsIgnoreCase(obd)) {
      return true;
    }
  }
  return false;
}

static bool nombreObd(String nombre) {
  nombre.toUpperCase();
  for (const char *parte : BLE_NOMBRES_OBD) {
    if (nombre.indexOf(parte) >= 0) {
      return true;
    }
  }
  return false;
}

static const char *nombreTipo(uint8_t tipo) {
  switch (tipo) {
  case BLE_ADDR_PUBLIC:
    return "publica";
  case BLE_ADDR_RANDOM:
    return "aleatoria";
  case BLE_ADDR_PUBLIC_ID:
    return "publica (RPA)";
  case BLE_ADDR_RANDOM_ID:
    return "aleatoria (RPA)";
  }
  return "?";
}

static void imprimirPropiedades(BLERemoteCharacteristic *c) {
  if (c->canRead()) {
    Serial.print(" READ");
  }
  if (c->canWrite()) {
    Serial.print(" WRITE");
  }
  if (c->canWriteNoResponse()) {
    Serial.print(" WRITE_NR");
  }
  if (c->canNotify()) {
    Serial.print(" NOTIFY");
  }
  if (c->canIndicate()) {
    Serial.print(" INDICATE");
  }
}

static void imprimirConexion() {
  ble_gap_conn_desc desc;
  if (ble_gap_conn_find(cliente->getConnId(), &desc) != 0) {
    return;
  }
  // NimBLE da el intervalo en unidades de 1,25 ms y la supervisión en 10 ms
  Serial.printf("BLE: MTU %u, intervalo %.2f ms, latencia %u, supervision %u ms\n",
                cliente->getMTU(), desc.conn_itvl * 1.25f, desc.conn_latency,
                desc.supervision_timeout * 10);
  if (desc.sec_state.encrypted) {
    Serial.printf("BLE: enlace cifrado: el adaptador pidio emparejamiento (vinculo %s)\n",
                  desc.sec_state.bonded ? "guardado" : "no guardado");
  } else {
    Serial.println("BLE: enlace sin cifrar: hasta ahora no pidio emparejamiento");
  }
}

static void olvidarConexion() {
  conectadoA = 0;
  servicios = nullptr;
  notificacion = nullptr;
  escritura = nullptr;
}

static void cortarConexion() {
  int numero = conectadoA;
  olvidarConexion();
  if (cliente->isConnected()) {
    cliente->disconnect();
    uint32_t inicio = millis();
    while (cliente->isConnected() && millis() - inicio < 2000) {
      delay(10);
    }
  }
  // Pedida acá: no es una pérdida
  seDesconecto = false;
  Serial.printf("BLE: desconectado de %d\n", numero);
}

static bool conectar(int numero) {
  if (numero < 1 || (size_t)numero > cantidad) {
    Serial.printf("BLE: no hay un %d en la lista; primero !escanear\n", numero);
    return false;
  }
  if (conectadoA == numero && cliente->isConnected()) {
    return true;
  }
  if (conectadoA != 0 || cliente->isConnected()) {
    cortarConexion();
  }

  Dispositivo &d = dispositivos[numero - 1];
  Serial.printf("BLE: conectando a %d (%s, %s)\n", numero, d.direccion.toString().c_str(),
                nombreTipo(d.tipo));
  seDesconecto = false;
  uint32_t inicio = millis();
  // Con NimBLE la biblioteca espera hasta 30 s e ignora el timeout que se le pasa
  bool ok = cliente->connect(d.direccion, d.tipo);
  uint32_t ms = millis() - inicio;
  if (!ok) {
    Serial.printf("BLE: no se pudo conectar (%lu ms)\n", (unsigned long)ms);
    return false;
  }
  conectadoA = numero;
  Serial.printf("BLE: conectado en %lu ms\n", (unsigned long)ms);
  imprimirConexion();
  return true;
}

static bool leerServicios() {
  if (servicios != nullptr) {
    return true;
  }
  uint32_t inicio = millis();
  servicios = cliente->getServices();
  if (servicios->empty()) {
    Serial.println("BLE: no se pudieron leer los servicios");
    servicios = nullptr;
    return false;
  }
  for (auto &par : *servicios) {
    par.second->getCharacteristicsByHandle();
  }
  Serial.printf("BLE: %u servicios leidos en %lu ms\n", (unsigned)servicios->size(),
                (unsigned long)(millis() - inicio));
  return true;
}

// S1, S2... en el orden de !explorar
static BLERemoteService *servicioNumero(int numero) {
  int i = 1;
  for (auto &par : *servicios) {
    if (i++ == numero) {
      return par.second;
    }
  }
  return nullptr;
}

static bool seEscribe(BLERemoteCharacteristic *c) {
  return c->canWriteNoResponse() || c->canWrite();
}

// La primera de notificación (o indicación) y, de escritura, la primera que
// no sea esa (FFF2 en el adaptador del DS3); si no hay otra, la misma.
static void elegirCaracteristicas(BLERemoteService *servicio, BLERemoteCharacteristic *&notif,
                                  BLERemoteCharacteristic *&escr) {
  notif = nullptr;
  escr = nullptr;
  BLERemoteCharacteristic *indic = nullptr;
  auto *caracteristicas = servicio->getCharacteristicsByHandle();
  for (auto &par : *caracteristicas) {
    BLERemoteCharacteristic *c = par.second;
    if (notif == nullptr && c->canNotify()) {
      notif = c;
    }
    if (indic == nullptr && c->canIndicate()) {
      indic = c;
    }
  }
  if (notif == nullptr) {
    notif = indic;
  }
  for (auto &par : *caracteristicas) {
    if (par.second != notif && seEscribe(par.second)) {
      escr = par.second;
      return;
    }
  }
  if (notif != nullptr && seEscribe(notif)) {
    escr = notif;
  }
}

// El primero de BLE_SERVICIOS_OBD con notificación y escritura; si no hay,
// cualquiera que las tenga.
static BLERemoteService *servicioAutomatico() {
  for (bool soloObd : {true, false}) {
    for (auto &par : *servicios) {
      BLERemoteService *servicio = par.second;
      if (soloObd && !servicioObd(textoUuid(servicio->getUUID()))) {
        continue;
      }
      BLERemoteCharacteristic *notif;
      BLERemoteCharacteristic *escr;
      elegirCaracteristicas(servicio, notif, escr);
      if (notif != nullptr && escr != nullptr) {
        return servicio;
      }
    }
  }
  return nullptr;
}

static void copiar(BLEAdvertisedDevice &anuncio, Dispositivo &d) {
  d.direccion = anuncio.getAddress();
  d.tipo = anuncio.getAddressType();
  d.nombre = anuncio.haveName() ? anuncio.getName() : String();
  // El adaptador del DS3 rellena el nombre con bytes nulos
  while (d.nombre.length() > 0 && d.nombre[d.nombre.length() - 1] == '\0') {
    d.nombre.remove(d.nombre.length() - 1);
  }
  d.rssi = anuncio.getRSSI();
  d.pareceObd = nombreObd(d.nombre);
  d.servicios = String();
  for (int i = 0; i < anuncio.getServiceUUIDCount(); i++) {
    String uuid = textoUuid(anuncio.getServiceUUID(i));
    // Puede venir en el anuncio y en la respuesta de escaneo
    if ((" " + d.servicios + " ").indexOf(" " + uuid + " ") >= 0) {
      continue;
    }
    if (d.servicios.length() > 0) {
      d.servicios += ' ';
    }
    d.servicios += uuid;
    if (servicioObd(uuid)) {
      d.pareceObd = true;
    }
  }
}

bool Explorador::iniciar() {
  buffer = xMessageBufferCreate(TAMANIO_BUFFER);
  if (buffer == nullptr || !BLEDevice::init(BLE_NOMBRE_LOCAL)) {
    Serial.println("BLE: no se pudo iniciar");
    return false;
  }
  // Si el adaptador pide emparejamiento: sin clave ("Just Works") y sin
  // vínculo, para no guardar nada en la flash. Nunca se empareja por iniciativa propia.
  BLESecurity::setAuthenticationMode(false, false, true);
  BLESecurity::setForceAuthentication(false);
  BLEDevice::setSecurityCallbacks(&alEmparejar);

  cliente = BLEDevice::createClient();
  cliente->setClientCallbacks(&alCliente);
  Serial.printf("BLE: iniciado con %s, direccion propia %s\n",
                BLEDevice::getBLEStackString().c_str(), BLEDevice::getAddress().toString().c_str());
  return true;
}

void Explorador::atender() {
  if (seDesconecto) {
    seDesconecto = false;
    if (conectadoA != 0) {
      Serial.printf("BLE: se perdio la conexion con %d\n", conectadoA);
      olvidarConexion();
    }
  }

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

  uint32_t p = perdidas.exchange(0);
  if (p > 0) {
    Serial.printf("BLE: %lu notificaciones no entraron en el buffer\n", (unsigned long)p);
  }
}

void Explorador::escanear() {
  // Sin búsqueda no hay lista, así que el resto de los comandos no llega a usar el cliente
  if (cliente == nullptr) {
    Serial.println("BLE: no esta iniciado");
    return;
  }
  if (conectadoA != 0 || cliente->isConnected()) {
    cortarConexion();
  }

  BLEScan *scan = BLEDevice::getScan();
  // Activa: pide la respuesta de escaneo, donde muchos mandan el nombre
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(99);
  Serial.printf("BLE: buscando durante %lu s\n", (unsigned long)BUSQUEDA_BLE_S);
  BLEScanResults *resultados = scan->start(BUSQUEDA_BLE_S, false);

  cantidad = 0;
  int total = resultados->getCount();
  for (int i = 0; i < total; i++) {
    BLEAdvertisedDevice anuncio = resultados->getDevice(i);
    Dispositivo nuevo;
    copiar(anuncio, nuevo);
    if (cantidad < MAX_DISPOSITIVOS) {
      dispositivos[cantidad++] = nuevo;
      continue;
    }
    // Lista llena: un posible OBD reemplaza a uno que no lo parece
    if (nuevo.pareceObd) {
      for (size_t j = 0; j < cantidad; j++) {
        if (!dispositivos[j].pareceObd) {
          dispositivos[j] = nuevo;
          break;
        }
      }
    }
  }
  scan->clearResults();

  // Los que parecen OBD primero; después, del más cercano al más lejano
  std::sort(dispositivos, dispositivos + cantidad, [](const Dispositivo &a, const Dispositivo &b) {
    if (a.pareceObd != b.pareceObd) {
      return a.pareceObd;
    }
    return a.rssi > b.rssi;
  });

  if (cantidad == 0) {
    Serial.println("BLE: no se encontro ningun dispositivo");
    return;
  }
  Serial.printf("BLE: %d dispositivos; con * los que parecen OBD\n", total);
  for (size_t i = 0; i < cantidad; i++) {
    Dispositivo &d = dispositivos[i];
    Serial.printf("%c%2u  %4d dBm  %s %-15s  \"", d.pareceObd ? '*' : ' ', (unsigned)(i + 1),
                  d.rssi, d.direccion.toString().c_str(), nombreTipo(d.tipo));
    imprimirVisible(d.nombre.c_str(), d.nombre.length());
    Serial.printf("\"  servicios: %s\n", d.servicios.length() > 0 ? d.servicios.c_str() : "-");
  }
  if ((size_t)total > cantidad) {
    Serial.printf("BLE: se muestran %u de %d\n", (unsigned)cantidad, total);
  }
}

void Explorador::explorar(int numero) {
  if (!conectar(numero) || !leerServicios()) {
    atender();
    return;
  }

  int s = 1;
  for (auto &par : *servicios) {
    BLERemoteService *servicio = par.second;
    String uuid = textoUuid(servicio->getUUID());
    Serial.printf("S%d  %s%s\n", s++, uuid.c_str(), servicioObd(uuid) ? "  <- parece OBD" : "");
    // Generic Access y Device Information: nombre, fabricante, modelo, versiones
    bool informativo = uuid == "1800" || uuid == "180A";
    for (auto &c : *servicio->getCharacteristicsByHandle()) {
      BLERemoteCharacteristic *car = c.second;
      Serial.printf("    %s", textoUuid(car->getUUID()).c_str());
      imprimirPropiedades(car);
      if (informativo && car->canRead()) {
        String valor = car->readValue();
        Serial.print("  = \"");
        imprimirVisible(valor.c_str(), valor.length());
        Serial.print('"');
      }
      Serial.println();
    }
  }
  imprimirConexion();
  Serial.printf("BLE: sigue conectado; !usar %d o !desconectar\n", numero);
  atender();
}

void Explorador::usar(int numero, int elegido) {
  if (!conectar(numero) || !leerServicios()) {
    atender();
    return;
  }

  BLERemoteService *servicio;
  if (elegido > 0) {
    servicio = servicioNumero(elegido);
    if (servicio == nullptr) {
      Serial.printf("BLE: no hay un servicio S%d; ver !explorar %d\n", elegido, numero);
      return;
    }
  } else {
    servicio = servicioAutomatico();
    if (servicio == nullptr) {
      Serial.printf("BLE: ningun servicio tiene notificacion y escritura; ver !explorar %d\n",
                    numero);
      return;
    }
  }

  BLERemoteCharacteristic *notif;
  BLERemoteCharacteristic *escr;
  elegirCaracteristicas(servicio, notif, escr);
  String uuid = textoUuid(servicio->getUUID());
  if (notif == nullptr || escr == nullptr) {
    Serial.printf("BLE: el servicio %s no tiene notificacion y escritura\n", uuid.c_str());
    return;
  }

  if (notificacion != nullptr && notificacion != notif) {
    notificacion->unsubscribe();
  }
  notificacion = nullptr;
  escritura = nullptr;
  if (!notif->subscribe(notif->canNotify(), alNotificar, true)) {
    Serial.println("BLE: no se pudo suscribir a las notificaciones");
    atender();
    return;
  }
  notificacion = notif;
  escritura = escr;
  conRespuesta = !escr->canWriteNoResponse();

  Serial.printf(
      "BLE: servicio %s; %s de %s, escritura %s en %s\n", uuid.c_str(),
      notif->canNotify() ? "notificaciones" : "indicaciones", textoUuid(notif->getUUID()).c_str(),
      conRespuesta ? "con respuesta" : "sin respuesta", textoUuid(escr->getUUID()).c_str());
  Serial.println("BLE: las lineas sin '!' van al adaptador");
  atender();
}

void Explorador::desconectar() {
  if (cliente == nullptr || (conectadoA == 0 && !cliente->isConnected())) {
    Serial.println("BLE: no hay conexion");
    return;
  }
  cortarConexion();
}

void Explorador::enviar(const char *comando, uint32_t timeoutMs) {
  Serial.print(">> ");
  imprimirVisible(comando, strlen(comando));
  Serial.println();

  // La lista blanca va primero: lo rechazado no toca el BLE.
  if (!comandoPermitido(comando)) {
    Serial.println("<< RECHAZADO");
    return;
  }
  if (escritura == nullptr) {
    Serial.println("<< SIN ADAPTADOR: primero !usar N");
    return;
  }
  if (!cliente->isConnected()) {
    Serial.println("<< DESCONECTADO");
    return;
  }

  static uint8_t trozo[LARGO_MAXIMO_NOTIFICACION];
  uint32_t inicio = millis();

  // Lo que haya quedado de un comando anterior (por ejemplo, tras un
  // timeout) se descarta, pero se muestra.
  String descartado;
  size_t n;
  while ((n = xMessageBufferReceive(buffer, trozo, sizeof(trozo), 0)) > 0) {
    descartado.concat((const char *)trozo, n);
  }
  if (descartado.length() > 0) {
    Serial.print("-- descartado: ");
    imprimirVisible(descartado.c_str(), descartado.length());
    Serial.println();
  }

  String linea = comando;
  linea += '\r';
  if (!escritura->writeValue((uint8_t *)linea.c_str(), linea.length(), conRespuesta)) {
    Serial.printf("<< NO SE PUDO ESCRIBIR %lu ms\n", (unsigned long)(millis() - inicio));
    atender();
    return;
  }

  const char *estado = "TIMEOUT";
  String cruda;
  String sobrante; // lo que llegó después del '>' en la misma notificación
  String tamanios;
  uint32_t notificaciones = 0;
  bool fin = false;
  while (!fin && millis() - inicio < timeoutMs) {
    n = xMessageBufferReceive(buffer, trozo, sizeof(trozo), pdMS_TO_TICKS(5));
    if (n == 0) {
      if (!cliente->isConnected()) {
        estado = "DESCONECTADO";
        break;
      }
      continue;
    }
    notificaciones++;
    if (tamanios.length() > 0) {
      tamanios += '+';
    }
    tamanios += (unsigned)n;
    for (size_t i = 0; i < n; i++) {
      char c = (char)trozo[i];
      if (fin) {
        sobrante += c;
      } else if (c == '>') {
        fin = true;
        estado = "OK";
      } else if (c != '\0' && cruda.length() < LARGO_MAXIMO_RESPUESTA) {
        // Algunos clones mandan bytes nulos sueltos; no sirven y cortarían el texto.
        cruda += c;
      }
    }
  }
  uint32_t ms = millis() - inicio;

  Serial.print("<< \"");
  imprimirVisible(cruda.c_str(), cruda.length());
  Serial.printf("\" %s %lu ms, %lu notificaciones", estado, (unsigned long)ms,
                (unsigned long)notificaciones);
  if (notificaciones > 0) {
    Serial.printf(" de %s bytes", tamanios.c_str());
  }
  Serial.println();
  if (sobrante.length() > 0) {
    Serial.print("-- despues del '>': ");
    imprimirVisible(sobrante.c_str(), sobrante.length());
    Serial.println();
  }
  atender();
}
