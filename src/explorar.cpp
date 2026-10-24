#include "explorar.h"

#include <Arduino.h>
#include <BLEDevice.h>

#include <algorithm>

#include "config.h"

// En el ESP32-S3, Arduino-ESP32 3.3 trae NimBLE, no Bluedroid. Se usa
// ble_gap_conn_find, que es suyo.
#if !defined(CONFIG_NIMBLE_ENABLED)
#error "explorar.cpp espera el stack NimBLE"
#endif

static const size_t MAX_DISPOSITIVOS = 30;
static const uint32_t ESPERA_DESCONEXION_MS = 2000;

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
    Serial.println("BLE: enlace sin cifrar: no pidio emparejamiento");
  }
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

static bool listo() {
  if (!BLEDevice::getInitialized()) {
    Serial.println("BLE: no esta iniciado");
    return false;
  }
  if (cliente == nullptr) {
    cliente = BLEDevice::createClient();
  }
  return true;
}

void Explorador::escanear() {
  if (!listo()) {
    return;
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
  if (!listo()) {
    return;
  }
  if (numero < 1 || (size_t)numero > cantidad) {
    Serial.printf("BLE: no hay un %d en la lista; primero !escanear\n", numero);
    return;
  }

  Dispositivo &d = dispositivos[numero - 1];
  Serial.printf("BLE: conectando a %d (%s, %s)\n", numero, d.direccion.toString().c_str(),
                nombreTipo(d.tipo));
  uint32_t inicio = millis();
  // Con NimBLE la biblioteca espera hasta 30 s e ignora el timeout que se le pasa
  bool ok = cliente->connect(d.direccion, d.tipo);
  if (!ok) {
    Serial.printf("BLE: no se pudo conectar (%lu ms)\n", (unsigned long)(millis() - inicio));
    return;
  }
  Serial.printf("BLE: conectado en %lu ms\n", (unsigned long)(millis() - inicio));
  imprimirConexion();

  inicio = millis();
  auto *servicios = cliente->getServices();
  Serial.printf("BLE: %u servicios leidos en %lu ms\n", (unsigned)servicios->size(),
                (unsigned long)(millis() - inicio));
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

  cliente->disconnect();
  inicio = millis();
  while (cliente->isConnected() && millis() - inicio < ESPERA_DESCONEXION_MS) {
    delay(10);
  }
  Serial.printf("BLE: desconectado de %d\n", numero);
}
