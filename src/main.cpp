#include <Arduino.h>
#include <comandos.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <esp_system.h>
#include <pids.h>
#include <respuesta.h>

#include "config.h"
#include "consola.h"
#include "datos.h"
#include "descubrimiento.h"
#include "elm327.h"
#include "explorar.h"
#include "pantalla.h"
#include "pantalla_hw.h"
#include "sondeo.h"

// Máquina de estados de la conexión, portada del anterior. INICIO es setup().
//
// CONECTANDO_BT → INICIANDO_ELM → BUSCANDO_ECU → LEYENDO
//       ↑                              ↓            ↓
//       └──────── BT_PERDIDO ←───── SIN_ECU ←───────┘
//
// Toda pérdida del BLE lleva a BT_PERDIDO. Un aviso del ELM (LV RESET,
// etc.) vuelve a INICIANDO_ELM.
enum class Estado { CONECTANDO_BT, INICIANDO_ELM, BUSCANDO_ECU, LEYENDO, SIN_ECU, BT_PERDIDO };

// ATSP6: el protocolo del DS3 (CAN 11 bit, 500 kbaud), confirmado en el
// anterior. Con ATSP0 el ELM probaba todos los protocolos cada vez que no
// había ECU.
static const char *const INICIALIZACION[] = {"ATZ",  "ATE0",  "ATL0", "ATS0",
                                             "ATH0", "ATSP6", "ATAT1"};

// Pantalla provisoria (E3): RPM en grande y estas a la derecha. El sondeo
// consulta las visibles en cada vuelta; VOLT_ELM va siempre cada PERIODO_ATRV_MS.
static const Magnitud VISIBLES[] = {RPM, REFRIGERANTE, VELOCIDAD, ADMISION};
static const Magnitud A_LA_DERECHA[] = {REFRIGERANTE, VELOCIDAD, ADMISION, VOLT_ELM};
static const uint8_t CANT_VISIBLES = sizeof(VISIBLES) / sizeof(VISIBLES[0]);
static const uint8_t CANT_A_LA_DERECHA = sizeof(A_LA_DERECHA) / sizeof(A_LA_DERECHA[0]);
static_assert(PIDS_POR_CONSULTA >= 2 && PIDS_POR_CONSULTA <= MAX_PIDS_POR_PEDIDO,
              "PIDS_POR_CONSULTA va de 2 a MAX_PIDS_POR_PEDIDO");

Elm327 elm;
Explorador explorador;
Consola consola(elm, explorador);
PantallaHw pantallaHw;
Pantalla pantalla(pantallaHw);
PidsSoportados soportados;
Sondeo sondeo;

static Estado estado = Estado::CONECTANDO_BT;
static uint32_t proximoIntento = 0; // cuándo vuelve a actuar el estado
static uint32_t ultimaLectura = 0;  // ATRV en SIN_ECU
static uint32_t ultimoDibujo = 0;
static uint32_t ultimaTabla = 0;
static uint16_t consultas = 0;   // desde la última tabla
static uint32_t msConsultas = 0; // lo que tardaron, para el promedio
static uint16_t lecturasRpm = 0; // desde la última tabla
static const char *motivo = "";
static uint8_t fallasEcu = 0;
static uint32_t ultimaRespuestaEcu = 0; // la última consulta a la ECU que salió bien
static bool variosPids = false;         // la ECU contesta pedidos de varios PIDs (010C0D...)
static uint8_t incompletas = 0;         // respuestas seguidas a las que les faltó algún PID
static uint8_t fallasElm = 0;
static char version[12] = "";
static char protocolo[4] = ""; // ATDPN
static bool descubriendo = false;

static const char *textoEstado(Estado e) {
  switch (e) {
  case Estado::CONECTANDO_BT:
    return "CONECTANDO_BT";
  case Estado::INICIANDO_ELM:
    return "INICIANDO_ELM";
  case Estado::BUSCANDO_ECU:
    return "BUSCANDO_ECU";
  case Estado::LEYENDO:
    return "LEYENDO";
  case Estado::SIN_ECU:
    return "SIN_ECU";
  case Estado::BT_PERDIDO:
    return "BT_PERDIDO";
  }
  return "?";
}

static const char *textoFalla(EstadoTransporte transporte, TipoRespuesta tipo) {
  switch (tipo) {
  case TipoRespuesta::SIN_ECU:
    return "No responde";
  case TipoRespuesta::NO_DATA:
    return "Sin datos";
  case TipoRespuesta::ERROR_BUS:
    return "Error de bus";
  case TipoRespuesta::BUFFER_LLENO:
    return "Buffer lleno";
  case TipoRespuesta::DETENIDO:
    return "Interrumpido";
  case TipoRespuesta::NO_SOPORTADO:
    return "No soportado";
  case TipoRespuesta::INVALIDA:
    return "Respuesta invalida";
  default:
    break;
  }
  return transporte == EstadoTransporte::TIMEOUT ? "Sin respuesta" : "Respuesta vacia";
}

static const char *motivoReinicio(esp_reset_reason_t causa) {
  switch (causa) {
  case ESP_RST_POWERON:
    return "encendido";
  case ESP_RST_EXT:
    return "pin externo";
  case ESP_RST_SW:
    return "por software";
  case ESP_RST_PANIC:
    return "excepcion";
  case ESP_RST_INT_WDT:
    return "watchdog de interrupciones";
  case ESP_RST_TASK_WDT:
    return "watchdog de tareas";
  case ESP_RST_WDT:
    return "otro watchdog";
  case ESP_RST_DEEPSLEEP:
    return "salida de deep sleep";
  case ESP_RST_BROWNOUT:
    return "brownout (baja tension)";
  case ESP_RST_USB:
    return "USB";
  case ESP_RST_JTAG:
    return "JTAG";
  case ESP_RST_PWR_GLITCH:
    return "falla de alimentacion";
  case ESP_RST_CPU_LOCKUP:
    return "CPU bloqueada";
  default:
    return "desconocido";
  }
}

static void imprimirInfo() {
  Serial.printf("Chip: %s rev %u, %u nucleos, %lu MHz\n", ESP.getChipModel(),
                (unsigned)ESP.getChipRevision(), (unsigned)ESP.getChipCores(),
                (unsigned long)ESP.getCpuFreqMHz());

  // El tamaño del chip, no el del heap (que es un poco menor)
  size_t psram = esp_psram_get_size();
  if (psram > 0) {
    Serial.printf("PSRAM: %u MB (libres %u KB)\n", (unsigned)(psram >> 20),
                  (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) >> 10));
  } else {
    Serial.println("PSRAM: NO");
  }

  // La física la lee del chip; la configurada es la de platformio.ini
  uint32_t fisica = 0;
  esp_flash_get_physical_size(nullptr, &fisica);
  Serial.printf("Flash: %lu MB (configurada %lu MB, %lu MHz)\n", (unsigned long)(fisica >> 20),
                (unsigned long)(ESP.getFlashChipSize() >> 20),
                (unsigned long)(ESP.getFlashChipSpeed() / 1000000));

  Serial.printf("RAM interna libre: %u KB (bloque mayor %u KB)\n",
                (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) >> 10),
                (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) >> 10));
  Serial.printf("Ultimo reinicio: %s\n", motivoReinicio(esp_reset_reason()));

  if (pantallaHw.iniciada()) {
    uint32_t envio;
    uint32_t total = pantallaHw.medirRefresco(envio);
    Serial.printf("Pantalla: refresco completo %lu ms (envio al panel %lu ms), brillo %u\n",
                  (unsigned long)total, (unsigned long)envio, pantallaHw.brillo());
  }
  Serial.printf("Estado: %s; ELM %s, protocolo %s, %u PIDs soportados\n", textoEstado(estado),
                version[0] != '\0' ? version : "-", protocolo[0] != '\0' ? protocolo : "-",
                soportados.cantidad());
}

static bool yaEs(uint32_t momento) {
  return (int32_t)(millis() - momento) >= 0;
}

static uint32_t segundosHasta(uint32_t momento) {
  if (yaEs(momento)) {
    return 0;
  }
  return (momento - millis() + 999) / 1000;
}

// La línea de la magnitud para la pantalla. El valor se escribe en texto.
static LineaDato lineaDe(Magnitud m, uint32_t ahora, char *texto, size_t tam) {
  textoDato(m, ahora, true, texto, tam);
  float valor;
  return {etiquetaDe(m), texto, valorActual(m, ahora, &valor)};
}

static void dibujarDatos(uint32_t ahora) {
  char rpm[16];
  LineaDato principal = lineaDe(RPM, ahora, rpm, sizeof(rpm));
  char textos[CANT_A_LA_DERECHA][16];
  LineaDato lineas[CANT_A_LA_DERECHA];
  for (uint8_t i = 0; i < CANT_A_LA_DERECHA; i++) {
    lineas[i] = lineaDe(A_LA_DERECHA[i], ahora, textos[i], sizeof(textos[i]));
  }
  pantalla.datos(version, principal, lineas, CANT_A_LA_DERECHA);
}

// Las pantallas de CONECTANDO_BT, de INICIANDO_ELM mientras configura y de
// !descubrir las dibujan esos pasos.
static void dibujar() {
  uint32_t ahora = millis();
  ultimoDibujo = ahora;
  if (descubriendo) {
    return;
  }
  uint32_t segundos = segundosHasta(proximoIntento);
  char voltaje[16];

  switch (estado) {
  case Estado::CONECTANDO_BT:
    break;
  case Estado::INICIANDO_ELM:
    if (segundos > 0) {
      pantalla.elmNoResponde(segundos);
    }
    break;
  case Estado::BUSCANDO_ECU:
    pantalla.buscandoEcu(version);
    break;
  case Estado::LEYENDO:
    dibujarDatos(ahora);
    break;
  case Estado::SIN_ECU:
    pantalla.sinEcu(version, lineaDe(VOLT_ELM, ahora, voltaje, sizeof(voltaje)), motivo, segundos);
    break;
  case Estado::BT_PERDIDO:
    pantalla.sinConexion(motivo, segundos);
    break;
  }
}

static void cambiarA(Estado nuevo, uint32_t esperaMs = 0) {
  estado = nuevo;
  proximoIntento = millis() + esperaMs;
  // SIN_ECU lee ATRV apenas entra
  ultimaLectura = millis() - PERIODO_ATRV_MS;
  if (nuevo == Estado::SIN_ECU || nuevo == Estado::BT_PERDIDO) {
    Serial.printf("== %s (%s)\n", textoEstado(nuevo), motivo);
  } else {
    Serial.printf("== %s\n", textoEstado(nuevo));
  }
  dibujar();
}

// Mientras se espera al ELM, la pantalla sigue al día: así un dato que se
// vence mientras tanto no queda a la vista. No usa el ELM.
static void alEsperar() {
  if (millis() - ultimoDibujo >= PERIODO_PANTALLA_MS) {
    dibujar();
  }
  pantallaHw.atender();
}

// Espera atendiendo la pantalla
static void esperar(uint32_t ms) {
  uint32_t inicio = millis();
  while (millis() - inicio < ms) {
    pantallaHw.atender();
    delay(5);
  }
}

static void perderConexion(const char *por) {
  motivo = por;
  version[0] = '\0';
  marcarSinConexion();
  cambiarA(Estado::BT_PERDIDO, REINTENTO_BT_MS);
}

// !escanear y !explorar sueltan el ELM y bloquean varios segundos sin
// atender la pantalla: antes se sacan los datos.
static void alExplorar() {
  perderConexion("Explorando BLE");
  pantallaHw.dibujarYa();
}

static void alAvanzarDescubrimiento(uint16_t hechos, uint16_t total) {
  pantalla.descubriendo(hechos, total);
}

// Solo con la ECU ya encontrada: así el ELM no busca protocolo en el medio
static void atenderDescubrimiento() {
  if (estado != Estado::LEYENDO) {
    Serial.printf("Consola: !descubrir solo con la ECU conectada (ECU OK). Ahora: %s\n",
                  textoEstado(estado));
    return;
  }
  descubriendo = true;
  descubrir(elm, alAvanzarDescubrimiento);
  descubriendo = false;
  // Que la tabla siguiente no cuente el rato del descubrimiento
  consultas = 0;
  msConsultas = 0;
  lecturasRpm = 0;
  ultimaTabla = millis();
  dibujar();
}

static void alCambiarFase(FaseConexion fase) {
  pantalla.conectando(fase == FaseConexion::BUSCANDO);
}

static void conectarBt() {
  if (elm.conectar(alCambiarFase)) {
    fallasElm = 0;
    marcarConectado();
    cambiarA(Estado::INICIANDO_ELM);
  } else {
    motivo = "No conecta con el adaptador";
    cambiarA(Estado::BT_PERDIDO, REINTENTO_BT_MS);
  }
}

static void inicializarElm() {
  if (!yaEs(proximoIntento)) {
    return; // esperando para reintentar
  }

  for (const char *comando : INICIALIZACION) {
    pantalla.iniciandoElm(comando);
    uint32_t timeout = strcmp(comando, "ATZ") == 0 ? TIMEOUT_ATZ_MS : TIMEOUT_COMANDO_MS;
    ResultadoComando r = elm.enviar(comando, timeout);
    if (r.estado == EstadoTransporte::DESCONECTADO) {
      perderConexion("Conexion perdida");
      return;
    }
    if (r.estado != EstadoTransporte::OK) {
      Serial.println("ELM: no responde");
      cambiarA(Estado::INICIANDO_ELM, REINTENTO_ELM_MS);
      return;
    }
    // Un "?" se registra pero no frena la inicialización
    if (interpretar(r.cruda.c_str(), comando).tipo == TipoRespuesta::NO_SOPORTADO) {
      Serial.printf("ELM: no acepta %s, sigo\n", comando);
    }
  }

  // De "ELM327 v2.1" queda "v2.1"
  pantalla.iniciandoElm("ATI");
  ResultadoComando r = elm.enviar("ATI", TIMEOUT_COMANDO_MS);
  char texto[20];
  if (r.estado == EstadoTransporte::OK &&
      textoRespuesta(r.cruda.c_str(), "ATI", texto, sizeof(texto))) {
    const char *espacio = strrchr(texto, ' ');
    strlcpy(version, espacio != nullptr ? espacio + 1 : texto, sizeof(version));
  } else {
    strlcpy(version, "?", sizeof(version));
  }

  cambiarA(Estado::BUSCANDO_ECU);
}

// false si cambió el estado (se perdió la conexión o hay que reiniciar el ELM).
static bool leerVoltaje() {
  ResultadoComando r = elm.enviar("ATRV", TIMEOUT_COMANDO_MS, consola.detalle());
  if (r.estado == EstadoTransporte::DESCONECTADO) {
    perderConexion("Conexion perdida");
    return false;
  }
  float v;
  if (r.estado == EstadoTransporte::OK && interpretarVoltaje(r.cruda.c_str(), &v)) {
    actualizarDato(VOLT_ELM, v, millis());
    fallasElm = 0;
    return true;
  }

  bool aviso = interpretar(r.cruda.c_str(), "ATRV").tipo == TipoRespuesta::AVISO_ELM;
  if (r.estado == EstadoTransporte::TIMEOUT) {
    fallasElm++;
  }
  if (aviso || fallasElm >= FALLAS_PARA_REINICIAR_ELM) {
    cambiarA(Estado::INICIANDO_ELM);
    return false;
  }
  return true;
}

// Consulta 0100 y decide: ECU presente, aviso del ELM o falla.
static Respuesta consultarEcu(uint32_t timeoutMs, EstadoTransporte &transporte) {
  ResultadoComando r = elm.enviar("0100", timeoutMs);
  transporte = r.estado;
  Respuesta respuesta = interpretar(r.cruda.c_str(), "0100");
  if (r.estado != EstadoTransporte::OK && respuesta.tipo == TipoRespuesta::DATOS) {
    // Llegaron datos pero no el '>': no se confía en ellos
    respuesta.tipo = TipoRespuesta::INVALIDA;
  }
  Serial.printf("   = %s\n", nombreTipo(respuesta.tipo));
  return respuesta;
}

// Con el 0100 ya leído, pide 0120, 0140 y 0160 mientras el mapa anterior
// los anuncie. Cada uno se intenta INTENTOS_AL_ENCONTRAR veces: con el
// adaptador nuevo, 0120 contestó NO DATA una vez (E3b). Si no sale, sus
// PIDs quedan como no soportados. false si se perdió la conexión.
static bool cargarMapas(const Respuesta &mapa00) {
  static const char *const MAPAS[] = {"0120", "0140", "0160"};
  soportados.limpiar();
  soportados.cargar(0x00, mapa00.datos, mapa00.largo);
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t base = 0x20 * (i + 1);
    if (!soportados.soportado(base)) {
      break;
    }
    Respuesta respuesta;
    bool leido = false;
    for (uint8_t intento = 1; intento <= INTENTOS_AL_ENCONTRAR && !leido; intento++) {
      ResultadoComando r = elm.enviar(MAPAS[i], TIMEOUT_COMANDO_MS);
      if (r.estado == EstadoTransporte::DESCONECTADO) {
        return false;
      }
      respuesta = interpretar(r.cruda.c_str(), MAPAS[i]);
      leido = r.estado == EstadoTransporte::OK && respuesta.tipo == TipoRespuesta::DATOS;
      if (!leido) {
        Serial.printf("ECU: no se pudo leer %s (%s), intento %u de %u\n", MAPAS[i],
                      nombreTipo(respuesta.tipo), intento, INTENTOS_AL_ENCONTRAR);
      }
    }
    if (!leido) {
      break;
    }
    soportados.cargar(base, respuesta.datos, respuesta.largo);
  }
  Serial.printf("ECU: %u PIDs soportados\n", soportados.cantidad());
  return true;
}

// "6", o "A6" si el ELM lo eligió en automático. false si se perdió la conexión.
static bool leerProtocolo() {
  ResultadoComando r = elm.enviar("ATDPN", TIMEOUT_COMANDO_MS);
  if (r.estado == EstadoTransporte::DESCONECTADO) {
    return false;
  }
  if (r.estado != EstadoTransporte::OK ||
      !textoRespuesta(r.cruda.c_str(), "ATDPN", protocolo, sizeof(protocolo))) {
    strlcpy(protocolo, "?", sizeof(protocolo));
  }
  const char *nombre = nombreProtocolo(protocolo);
  Serial.printf("ECU: protocolo %s (%s)\n", protocolo, nombre != nullptr ? nombre : "?");
  return true;
}

static void guardar(Magnitud m, float valor) {
  actualizarDato(m, valor, millis());
  if (m == RPM) {
    lecturasRpm++;
  }
}

// "01" y el PID de cada magnitud: "010C0D050F". Sin el sufijo de
// cantidad: con una respuesta de varios frames, un ELM podría cortarla en
// el primero.
static void armarPedido(const Magnitud *lista, uint8_t cantidad, char *comando, size_t tam) {
  size_t largo = strlcpy(comando, "01", tam);
  for (uint8_t i = 0; i < cantidad && largo < tam; i++) {
    largo += snprintf(comando + largo, tam - largo, "%02X", pidDe(lista[i]));
  }
}

// Guarda las magnitudes de lista que vinieron en la respuesta a un pedido
// de varios PIDs. Devuelve cuántas.
static uint8_t guardarVarias(const Respuesta &respuesta, const Magnitud *lista, uint8_t cantidad) {
  DatoPid pids[MAX_PIDS_POR_PEDIDO];
  uint8_t llegados = separarPids(respuesta.datos, respuesta.largo, pids, MAX_PIDS_POR_PEDIDO);
  uint8_t guardadas = 0;
  for (uint8_t i = 0; i < cantidad; i++) {
    for (uint8_t j = 0; j < llegados; j++) {
      float valor;
      if (pids[j].pid == pidDe(lista[i]) &&
          valorDesdePid(lista[i], pids[j].datos, pids[j].largo, &valor)) {
        guardar(lista[i], valor);
        guardadas++;
        break;
      }
    }
  }
  return guardadas;
}

// Prueba un pedido de varios PIDs con los visibles que la ECU soporta. Si
// vuelven todos, el sondeo los pide juntos; si no, de a uno, como en el
// anterior. Con los datos reiniciados: lo que llega ya se muestra.
// false si se perdió la conexión.
static bool probarVariosPids() {
  Magnitud lista[PIDS_POR_CONSULTA];
  uint8_t cantidad = 0;
  for (Magnitud m : VISIBLES) {
    if (cantidad < PIDS_POR_CONSULTA && dato(m).estado != EstadoDato::NO_SOPORTADO) {
      lista[cantidad++] = m;
    }
  }
  variosPids = false;
  incompletas = 0;
  if (cantidad < 2) {
    return true;
  }
  char comando[2 + 2 * MAX_PIDS_POR_PEDIDO + 1];
  armarPedido(lista, cantidad, comando, sizeof(comando));
  for (uint8_t intento = 1; intento <= INTENTOS_AL_ENCONTRAR && !variosPids; intento++) {
    ResultadoComando r = elm.enviar(comando, TIMEOUT_COMANDO_MS);
    if (r.estado == EstadoTransporte::DESCONECTADO) {
      return false;
    }
    Respuesta respuesta = interpretar(r.cruda.c_str(), comando);
    if (r.estado == EstadoTransporte::OK && respuesta.tipo == TipoRespuesta::DATOS) {
      // Si contesta pero incompleto, no se insiste: no es una falla suelta
      variosPids = guardarVarias(respuesta, lista, cantidad) == cantidad;
      break;
    }
  }
  Serial.printf("ECU: pedidos de varios PIDs: %s\n", variosPids ? "SI" : "NO, se piden de a uno");
  return true;
}

static void buscarEcu() {
  EstadoTransporte transporte;
  Respuesta respuesta = consultarEcu(TIMEOUT_BUSCAR_ECU_MS, transporte);
  if (transporte == EstadoTransporte::DESCONECTADO) {
    perderConexion("Conexion perdida");
  } else if (respuesta.tipo == TipoRespuesta::DATOS) {
    if (!cargarMapas(respuesta) || !leerProtocolo()) {
      perderConexion("Conexion perdida");
      return;
    }
    reiniciarDatos(soportados);
    if (!probarVariosPids()) {
      perderConexion("Conexion perdida");
      return;
    }
    fallasEcu = 0;
    ultimaRespuestaEcu = millis();
    consultas = 0;
    msConsultas = 0;
    lecturasRpm = 0;
    ultimaTabla = millis();
    sondeo.reiniciar(millis());
    cambiarA(Estado::LEYENDO);
  } else if (respuesta.tipo == TipoRespuesta::AVISO_ELM) {
    cambiarA(Estado::INICIANDO_ELM);
  } else {
    motivo = textoFalla(transporte, respuesta.tipo);
    cambiarA(Estado::SIN_ECU, REINTENTO_ECU_MS);
  }
}

// Una consulta a la ECU que no trajo nada. false si cambió el estado.
static bool registrarFalla(const char *comando, const ResultadoComando &r,
                           const Respuesta &respuesta) {
  if (respuesta.tipo == TipoRespuesta::AVISO_ELM) {
    cambiarA(Estado::INICIANDO_ELM);
    return false;
  }

  if (fallasEcu < UINT8_MAX) {
    fallasEcu++;
  }
  uint32_t silencio = millis() - ultimaRespuestaEcu;
  Serial.printf("ECU: %s = %s en %lu ms (falla %u seguida, %lu ms sin respuesta)\n", comando,
                nombreTipo(respuesta.tipo), (unsigned long)r.ms, fallasEcu,
                (unsigned long)silencio);
  // enviar() solo muestra lo crudo si falló el transporte
  if (respuesta.tipo == TipoRespuesta::INVALIDA) {
    String visible = r.cruda;
    visible.replace("\r", "\\r");
    visible.replace("\n", "\\n");
    Serial.printf("     cruda: \"%s\"\n", visible.c_str());
  }
  if (fallasEcu >= FALLAS_PARA_SIN_ECU && silencio >= SILENCIO_PARA_SIN_ECU_MS) {
    motivo = textoFalla(r.estado, respuesta.tipo);
    cambiarA(Estado::SIN_ECU, REINTENTO_ECU_MS);
    return false;
  }
  return true;
}

// Una consulta del sondeo. false si cambió el estado.
static bool consultar(Magnitud m) {
  sondeo.consultada(m, millis());
  consultas++;
  if (m == VOLT_ELM) {
    return leerVoltaje();
  }

  // Con el sufijo 1 el ELM no espera más respuestas: contesta una sola ECU
  char comando[8];
  snprintf(comando, sizeof(comando), "01%02X1", pidDe(m));
  ResultadoComando r = elm.enviar(comando, TIMEOUT_COMANDO_MS, consola.detalle());
  if (r.estado == EstadoTransporte::DESCONECTADO) {
    perderConexion("Conexion perdida");
    return false;
  }
  Respuesta respuesta = interpretar(r.cruda.c_str(), comando);
  if (r.estado != EstadoTransporte::OK && respuesta.tipo == TipoRespuesta::DATOS) {
    respuesta.tipo = TipoRespuesta::INVALIDA;
  }

  float valor;
  if (respuesta.tipo == TipoRespuesta::DATOS &&
      valorDesdePid(m, respuesta.datos, respuesta.largo, &valor)) {
    guardar(m, valor);
    fallasEcu = 0;
    ultimaRespuestaEcu = millis();
    return true;
  }
  return registrarFalla(comando, r, respuesta);
}

// Las magnitudes que tocan, en una sola consulta (010C0D050F). El
// adaptador nuevo tarda lo mismo pida uno o varios PIDs. false si cambió
// el estado.
static bool consultarVarias() {
  Magnitud lista[PIDS_POR_CONSULTA];
  uint8_t cantidad = sondeo.tocan(millis(), lista, PIDS_POR_CONSULTA);
  if (cantidad < 2) {
    return cantidad == 0 || consultar(lista[0]);
  }
  for (uint8_t i = 0; i < cantidad; i++) {
    sondeo.consultada(lista[i], millis());
  }
  consultas++;

  char comando[2 + 2 * MAX_PIDS_POR_PEDIDO + 1];
  armarPedido(lista, cantidad, comando, sizeof(comando));
  ResultadoComando r = elm.enviar(comando, TIMEOUT_COMANDO_MS, consola.detalle());
  if (r.estado == EstadoTransporte::DESCONECTADO) {
    perderConexion("Conexion perdida");
    return false;
  }
  Respuesta respuesta = interpretar(r.cruda.c_str(), comando);
  if (r.estado != EstadoTransporte::OK && respuesta.tipo == TipoRespuesta::DATOS) {
    respuesta.tipo = TipoRespuesta::INVALIDA;
  }
  uint8_t guardadas =
      respuesta.tipo == TipoRespuesta::DATOS ? guardarVarias(respuesta, lista, cantidad) : 0;
  if (guardadas == 0) {
    return registrarFalla(comando, r, respuesta);
  }

  fallasEcu = 0;
  ultimaRespuestaEcu = millis();
  if (guardadas == cantidad) {
    incompletas = 0;
    return true;
  }
  // Si siempre falta alguno, ese dato no se renovaría nunca
  incompletas++;
  Serial.printf("ECU: %s trajo %u de %u PIDs (%u seguidas)\n", comando, guardadas, cantidad,
                incompletas);
  if (incompletas >= INCOMPLETAS_PARA_DE_A_UNO) {
    variosPids = false;
    Serial.println("ECU: se vuelve a pedir de a un PID");
  }
  return true;
}

// Una línea con todos los datos, como se verían en pantalla
static void imprimirTabla() {
  uint32_t ahora = millis();
  Serial.print("|");
  for (uint8_t i = 0; i < CANT_MAGNITUDES; i++) {
    char texto[16];
    textoDato((Magnitud)i, ahora, false, texto, sizeof(texto));
    Serial.printf(" %s %s |", etiquetaDe((Magnitud)i), texto);
  }
  // Con el promedio de cada consulta se ve cuánto se va fuera de ellas
  uint32_t lapso = ahora - ultimaTabla;
  Serial.printf(" %lu consultas/s, %lu ms cada una, RPM %lu/s\n",
                (unsigned long)consultas * 1000 / lapso,
                (unsigned long)(consultas > 0 ? msConsultas / consultas : 0),
                (unsigned long)lecturasRpm * 1000 / lapso);
  consultas = 0;
  msConsultas = 0;
  lecturasRpm = 0;
  ultimaTabla = ahora;
}

static void leer() {
  // Lo de la consulta anterior se dibuja mientras se espera esta: así el
  // dibujo no se suma a lo que tarda el adaptador
  dibujar();
  Magnitud m;
  if (sondeo.siguiente(millis(), m)) {
    uint32_t inicio = millis();
    bool sigue = m == VOLT_ELM || !variosPids ? consultar(m) : consultarVarias();
    msConsultas += millis() - inicio;
    if (!sigue) {
      return;
    }
  }
  if (millis() - ultimaTabla >= PERIODO_TABLA_MS) {
    imprimirTabla();
  }
}

// Lee ATRV, que no toca el bus del auto, y cada REINTENTO_ECU_MS vuelve a
// buscar la ECU.
static void esperarEcu() {
  if (yaEs(proximoIntento)) {
    cambiarA(Estado::BUSCANDO_ECU);
    return;
  }
  if (millis() - ultimaLectura >= PERIODO_ATRV_MS) {
    ultimaLectura = millis();
    if (leerVoltaje()) {
      dibujar();
    }
  }
}

static void esperarReconexion() {
  if (yaEs(proximoIntento)) {
    cambiarA(Estado::CONECTANDO_BT);
  }
}

void setup() {
  Serial.begin(115200);

  // La pantalla antes que el BLE: necesita RAM interna con DMA. Lo que no
  // anduvo se informa cuando abre el monitor.
  bool conPantalla = pantallaHw.iniciar();
  pantalla.iniciar();
  pantalla.inicio();
  uint32_t inicio = millis();
  while (!Serial && millis() - inicio < ESPERA_MONITOR_MS) {
    pantallaHw.atender();
    delay(10);
  }
  Serial.println();
  Serial.println("Monitor OBDII DS3");
  if (!conPantalla) {
    Serial.println("Pantalla: no se pudo iniciar");
  }

  // El brownout queda activo (se decide en E7, con la placa instalada)
  if (esp_reset_reason() == ESP_RST_BROWNOUT) {
    Serial.println("Reinicio por brownout: la alimentacion no aguanta");
    pantalla.alimentacionBaja();
    esperar(ESPERA_TRAS_BROWNOUT_MS);
  } else if (millis() < ESPERA_ANTES_DE_BT_MS) {
    esperar(ESPERA_ANTES_DE_BT_MS - millis());
  }

  elm.iniciar();
  elm.mientrasEspera(alEsperar);
  consola.antesDeExplorar(alExplorar);
  marcarSinConexion();
  sondeo.visibles(VISIBLES, CANT_VISIBLES);
  imprimirInfo();
  Serial.println("Escribi un comando para el ELM, !reconectar, !olvidar, !descubrir, !log, "
                 "!info, !escanear o !explorar N.");

  cambiarA(Estado::CONECTANDO_BT);
}

void loop() {
  consola.atender();
  if (consola.reconexionPedida()) {
    elm.desconectar();
    version[0] = '\0';
    marcarSinConexion();
    cambiarA(Estado::CONECTANDO_BT);
  }
  if (consola.olvidoPedido()) {
    elm.olvidar();
    version[0] = '\0';
    marcarSinConexion();
    cambiarA(Estado::CONECTANDO_BT);
  }
  if (consola.descubrimientoPedido()) {
    atenderDescubrimiento();
  }
  int ajuste = consola.ajusteBrillo();
  if (ajuste != 0 && pantallaHw.iniciada()) {
    pantallaHw.ponerBrillo(constrain(pantallaHw.brillo() + ajuste * PASO_BRILLO, 0, 255));
    Serial.printf("Brillo: %u\n", pantallaHw.brillo());
  }
  if (consola.infoPedida()) {
    imprimirInfo();
  }

  bool usaBt = estado != Estado::CONECTANDO_BT && estado != Estado::BT_PERDIDO;
  if (usaBt && !elm.conectado()) {
    perderConexion("Conexion perdida");
  }

  switch (estado) {
  case Estado::CONECTANDO_BT:
    conectarBt();
    break;
  case Estado::INICIANDO_ELM:
    inicializarElm();
    break;
  case Estado::BUSCANDO_ECU:
    buscarEcu();
    break;
  case Estado::LEYENDO:
    leer();
    break;
  case Estado::SIN_ECU:
    esperarEcu();
    break;
  case Estado::BT_PERDIDO:
    esperarReconexion();
    break;
  }

  if (millis() - ultimoDibujo >= PERIODO_PANTALLA_MS) {
    dibujar();
  }
  pantallaHw.atender();
  delay(1);
}
