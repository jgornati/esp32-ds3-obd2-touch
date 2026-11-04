#include <Arduino.h>
#include <alarmas.h>
#include <aviso_cambio.h>
#include <comandos.h>
#include <dtc.h>
#include <marcha.h>
#include <esp_core_dump.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_psram.h>
#include <esp_system.h>
#include <pids.h>
#include <registro.h>
#include <reloj.h>
#include <respuesta.h>
#include <stdarg.h>
#include <viaje.h>

#include "boton.h"
#include "brillo.h"
#include "config.h"
#include "consola.h"
#include "datos.h"
#include "descubrimiento.h"
#include "elm327.h"
#include "explorar.h"
#include "guardados.h"
#include "imu.h"
#include "paginas.h"
#include "pantalla.h"
#include "pantalla_hw.h"
#include "placa.h"
#include "rtc.h"
#include "sondeo.h"
#include "sonido.h"
#include "tarjeta.h"
#include "umbral_cambio.h"

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

static_assert(PIDS_POR_CONSULTA >= 2 && PIDS_POR_CONSULTA <= MAX_PIDS_POR_PEDIDO,
              "PIDS_POR_CONSULTA va de 2 a MAX_PIDS_POR_PEDIDO");

Elm327 elm;
Explorador explorador;
Consola consola(elm, explorador);
PantallaHw pantallaHw;
Pantalla pantalla(pantallaHw);
Paginas paginas(pantalla);
Boton boton;
PidsSoportados soportados;
Sondeo sondeo;
// Desde que se puso el contacto: la placa arranca con la llave
Viaje viaje({VIGENCIA_MS, RPM_EN_MARCHA, ESPERA_VOLTAJE_VIAJE_MS});
// Con la RPM y la velocidad de una misma consulta (E24)
Marcha marcha({RELACIONES_MARCHAS, CANT_MARCHAS, TOLERANCIA_MARCHA, TOLERANCIA_PRIMERA,
               VELOCIDAD_MINIMA_MARCHA, LECTURAS_PARA_MARCHA, VIGENCIA_MS});
// En el orden de TipoAlarma
static const ReglaAlarma REGLAS_ALARMAS[CANT_ALARMAS] = {
    {REFRIGERANTE_ALARMA, REFRIGERANTE_NORMAL, true, REFRIGERANTE_ALARMA_MS,
     REFRIGERANTE_ALARMA_MS},
    {VOLTAJE_BAJO_ALARMA, VOLTAJE_BAJO_NORMAL, false, VOLTAJE_ALARMA_MS, VOLTAJE_ALARMA_MS},
    {VOLTAJE_ALTO_ALARMA, VOLTAJE_ALTO_NORMAL, true, VOLTAJE_ALARMA_MS, VOLTAJE_ALARMA_MS},
};
Alarmas alarmas(REGLAS_ALARMAS);
// El aviso de cambio (E25), con la marcha de E24
static const ReglasAvisoCambio REGLAS_CAMBIO = {REARME_CAMBIO_RPM, DURACION_CAMBIO_MS,
                                                CANT_MARCHAS,      CAMBIO_MINIMO_RPM,
                                                CAMBIO_MAXIMO_RPM, PASO_CAMBIO_RPM};
AvisoCambio avisoCambio(REGLAS_CAMBIO);
UmbralCambio umbralCambio(REGLAS_CAMBIO);
Sonido sonido;
Brillo brillo;
Imu imu;
Tarjeta tarjeta;
Rtc rtc;
ViajesGuardados guardados;

static Estado estado = Estado::CONECTANDO_BT;
static uint32_t proximoIntento = 0; // cuándo vuelve a actuar el estado
static uint32_t ultimaLectura = 0;  // ATRV en SIN_ECU
static uint32_t ultimoDibujo = 0;
static uint32_t ultimaTabla = 0;
static uint16_t consultas = 0;           // desde la última tabla
static uint32_t msConsultas = 0;         // lo que tardaron, para el promedio
static uint16_t lecturasRpm = 0;         // desde la última tabla
static int16_t consultasPorSegundo = -1; // de la última tabla, para Info; -1 sin medir
static const char *motivo = "";
static uint8_t fallasEcu = 0;
static uint32_t ultimaRespuestaEcu = 0; // la última consulta a la ECU que salió bien
static bool variosPids = false;         // la ECU contesta pedidos de varios PIDs (010C0D...)
static uint8_t incompletas = 0;         // respuestas seguidas a las que les faltó algún PID
static uint8_t fallasElm = 0;
static char version[12] = "";
static char protocolo[4] = "";  // ATDPN
static char direccion[18] = ""; // del adaptador conectado, para Info
static int16_t rssi = 0;        // del adaptador, en Info; 0 sin medir
static uint32_t ultimoRssi = 0;
static bool avisarConexion = true; // la fase de la conexión; si no, "Esperando el adaptador"
static bool ecuPerdida = false;    // SIN_ECU con el motor andando: no es lo normal
static bool descubriendo = false;
// Lo que dice la espera en CONECTANDO_BT: la fase, de alCambiarFase()
static const char *textoConexion = "Iniciando...";
static LecturaFallas fallas;              // la de la página Fallas
static uint32_t proximaLecturaFallas = 0; // mientras se ve la página
// El registro en la microSD (E13): una línea cada PERIODO_LINEA_MS desde la
// primera lectura de la ECU
static const size_t LARGO_LINEA_REGISTRO = 512; // con los eventos enteros
static bool registrando = false;
static uint32_t inicioRegistro = 0; // el millis() de la línea 0
static uint32_t proximaLinea = 0;
static Eventos eventos; // los de la línea que viene
static char fallasAnotadas[LARGO_EVENTOS] = "";
static EstadoTarjeta tarjetaAntes = EstadoTarjeta::BUSCANDO;

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

// El motivo del último reinicio; si fue el guardián (E23), con lo que anotó
static const char *motivoInicio() {
  static char texto[128];
  const char *traba = pantallaHw.trabaAnterior();
  if (traba[0] == '\0') {
    return motivoReinicio(esp_reset_reason());
  }
  snprintf(texto, sizeof(texto), "guardian (%s)", traba);
  return texto;
}

// Para el guardián: lo llama desde su tarea, con loop() quizás trabado
static const char *estadoActual() {
  return textoEstado(estado);
}

static float ms(uint32_t us) {
  return us / 1000.0f;
}

// "12 cuadros/s, dibujo 3.2/7.9, ... ms": de cada tiempo, el promedio y el
// máximo de los cuadros de un segundo
static void imprimirMedidas(const MedidasPantalla &m) {
  Serial.printf("%u cuadros/s", m.cuadros);
  if (m.cuadros == 0) {
    return;
  }
  Serial.printf(", dibujo %.1f/%.1f, espera %.1f/%.1f, giro %.1f/%.1f, envio %.1f/%.1f ms",
                ms(m.promedio.dibujo), ms(m.maximo.dibujo), ms(m.promedio.espera),
                ms(m.maximo.espera), ms(m.promedio.giro), ms(m.maximo.giro), ms(m.promedio.envio),
                ms(m.maximo.envio));
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
  Serial.printf("Ultimo reinicio: %s\n", motivoInicio());
  // Queda hasta el próximo pánico. Las demás tareas (dónde estaba trabado
  // loop()) se ven leyendo la partición coredump con esptool.
  esp_core_dump_summary_t volcado;
  if (esp_core_dump_get_summary(&volcado) == ESP_OK) {
    Serial.printf("Volcado del ultimo panico: tarea %s, pc 0x%08lx\n", volcado.exc_task,
                  (unsigned long)volcado.exc_pc);
  } else {
    Serial.println("Volcado del ultimo panico: ninguno");
  }

  if (pantallaHw.iniciada()) {
    MedidasPantalla m = pantallaHw.medidas();
    Serial.print("Pantalla en el ultimo segundo: ");
    imprimirMedidas(m);
    Serial.printf(" (promedio/maximo); LCD_TE %u barridos/s\n", m.barridos);
    TiemposCuadro t = pantallaHw.medirRefresco();
    Serial.printf("Pantalla: refresco completo %.1f ms: dibujo %.1f, espera %.1f, giro %.1f, envio "
                  "%.1f ms\n",
                  ms(t.dibujo + t.envio), ms(t.dibujo), ms(t.espera), ms(t.giro), ms(t.envio));
  }
  Serial.printf("Estado: %s; adaptador %s, %d dBm; ELM %s, protocolo %s, %u PIDs soportados\n",
                textoEstado(estado), direccion[0] != '\0' ? direccion : "-", elm.rssi(),
                version[0] != '\0' ? version : "-", protocolo[0] != '\0' ? protocolo : "-",
                soportados.cantidad());
  Serial.printf("Placa: I2C %s; brillo %u de %u\n", busPlaca() != nullptr ? "listo" : "no anda",
                brillo.nivel(), CANT_NIVELES_BRILLO);
  char cambio[16];
  textoUmbralCambio(umbralCambio.rpm(), cambio, sizeof(cambio));
  Serial.printf("Sonido: %s; alarmas activas: %u%s; aviso de cambio: %s\n",
                sonido.iniciado() ? "listo" : "no anda", alarmas.activas(),
                ALARMAS_DE_PRUEBA ? " (umbrales de prueba)" : "", cambio);
  if (imu.iniciado()) {
    Serial.printf("IMU: listo; %lu calibraciones, %lu lecturas fallidas\n",
                  (unsigned long)imu.calibraciones(), (unsigned long)imu.lecturasFallidas());
  } else {
    Serial.println("IMU: no anda");
  }
  tarjeta.imprimir();
  rtc.imprimir();
}

// !imu: la última muestra sin filtrar, para ver los ejes del sensor, y lo
// que sale de ella
static void imprimirImu() {
  Vector a;
  if (!imu.cruda(&a)) {
    Serial.println("IMU: sin muestras (ver el arranque o !info)");
    return;
  }
  LecturaG g = imu.lectura(millis());
  Serial.printf("IMU: x %+.2f, y %+.2f, z %+.2f g (total %.2f g); ", a.x, a.y, a.z,
                sqrtf(a.x * a.x + a.y * a.y + a.z * a.z));
  if (!g.calibrada) {
    Serial.printf("sin calibrar (calibra quieta %lu s); %lu lecturas fallidas\n",
                  (unsigned long)(QUIETA_FUERZA_G_MS / 1000),
                  (unsigned long)imu.lecturasFallidas());
    return;
  }
  Serial.printf("lateral %+.2f, longitudinal %+.2f g%s; maximos: frenada %.2f, aceleracion %.2f, "
                "curva izq %.2f, curva der %.2f g; %lu calibraciones, %lu lecturas fallidas\n",
                g.lateral, g.longitudinal, g.actual ? "" : " (vencida)", g.frenada.valor,
                g.aceleracion.valor, g.curvaIzquierda.valor, g.curvaDerecha.valor,
                (unsigned long)imu.calibraciones(), (unsigned long)imu.lecturasFallidas());
}

// La magnitud de la página que va en rojo con cada alarma
static Magnitud magnitudDe(TipoAlarma tipo) {
  switch (tipo) {
  case REFRIGERANTE_ALTO:
    return REFRIGERANTE;
  default:
    return VOLT_ELM;
  }
}

// La de la barra: con varias, una distinta cada PERIODO_ALARMAS_MS, con
// "1/2" detrás
static AlarmaALaVista alarmaALaVista(uint32_t ahora) {
  static char texto[40];
  AlarmaALaVista vista = {nullptr, 0};
  TipoAlarma tipo;
  uint8_t numero;
  if (!alarmas.aLaVista(ahora / PERIODO_ALARMAS_MS, &tipo, &numero)) {
    return vista;
  }
  textoAlarma(tipo, alarmas.alarma(tipo).valor(), texto, sizeof(texto));
  uint8_t activas = alarmas.activas();
  if (activas > 1) {
    size_t largo = strlen(texto);
    snprintf(texto + largo, sizeof(texto) - largo, "  %u/%u", numero + 1, activas);
  }
  vista.texto = texto;
  for (uint8_t i = 0; i < CANT_ALARMAS; i++) {
    if (alarmas.alarma((TipoAlarma)i).activa()) {
      vista.magnitudes |= 1 << magnitudDe((TipoAlarma)i);
    }
  }
  return vista;
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

// Sin datos se ve la espera, que dice solo en qué anda la conexión, salvo
// que se haya pasado deslizando a Anteriores o a DS3 (E22)
static void sinDatos(const char *texto, bool problema = false) {
  if (paginas.muestraEspera()) {
    pantalla.espera(texto, problema);
  } else {
    paginas.dibujarSinDatos(guardados.anteriores(), tarjeta.estado());
  }
}

// La pantalla de !descubrir la pone ese paso. El aviso de cambio (E25) tapa
// todo: mientras parpadea, la página no se toca, y al terminar se dibuja con
// lo último.
static void dibujar() {
  uint32_t ahora = millis();
  ultimoDibujo = ahora;
  bool cambio = estado == Estado::LEYENDO && !descubriendo && avisoCambio.activo(ahora);
  pantalla.avisoCambio(cambio);
  if (descubriendo || cambio) {
    return;
  }

  switch (estado) {
  case Estado::CONECTANDO_BT:
    sinDatos(textoConexion);
    break;
  case Estado::INICIANDO_ELM:
    if (yaEs(proximoIntento)) {
      sinDatos("Iniciando el adaptador...");
    } else {
      sinDatos("El adaptador no responde", true);
    }
    break;
  case Estado::BUSCANDO_ECU:
    sinDatos("Buscando la ECU...");
    break;
  case Estado::LEYENDO:
    paginas.dibujar(ahora,
                    {version, protocolo, soportados.cantidad(), consultasPorSegundo, rssi,
                     brillo.nivel(), umbralCambio.rpm(), tarjeta.estado(), tarjeta.viaje()},
                    fallas, segundosHasta(proximaLecturaFallas), viaje, marcha,
                    guardados.anteriores(), {imu.iniciado(), imu.lectura(ahora)},
                    alarmaALaVista(ahora));
    break;
  case Estado::SIN_ECU:
    if (ecuPerdida) {
      sinDatos("Sin respuesta de la ECU", true);
    } else {
      sinDatos("Esperando la ECU");
    }
    break;
  case Estado::BT_PERDIDO:
    sinDatos("Esperando el adaptador");
    break;
  }
}

// Un evento para la última columna del registro. Antes de la primera
// lectura de la ECU no hay archivo: no se anota.
static void anotar(const char *formato, ...) {
  if (!registrando) {
    return;
  }
  char texto[LARGO_EVENTOS];
  va_list argumentos;
  va_start(argumentos, formato);
  vsnprintf(texto, sizeof(texto), formato, argumentos);
  va_end(argumentos);
  eventos.agregar(texto);
}

// La primera línea del CSV: las columnas, con las etiquetas y unidades de la
// tabla del monitor
static size_t encabezadoRegistro(char *texto, size_t tam) {
  static const char *const DESPUES[] = {
      "Consultas/s", "Distancia km", "En marcha s",   "G lateral", "G long.",       "Pagina",
      "Cuadros/s",   "Dibujo ms",    "Dibujo max ms", "Espera ms", "Espera max ms", "Giro ms",
      "Giro max ms", "Envio ms",     "Envio max ms",  "Eventos"};
  LineaCsv linea(texto, tam);
  linea.texto("Seg");
  linea.texto("Hora");
  for (uint8_t i = 0; i < CANT_MAGNITUDES; i++) {
    char nombre[24];
    const char *unidad = unidadDe((Magnitud)i);
    snprintf(nombre, sizeof(nombre), "%s%s%s", etiquetaDe((Magnitud)i),
             unidad[0] != '\0' ? " " : "", unidad);
    linea.texto(nombre);
    if (i == VELOCIDAD) {
      linea.texto("Marcha"); // E24
    }
  }
  for (const char *nombre : DESPUES) {
    linea.texto(nombre);
  }
  return linea.terminar();
}

// Con la primera lectura de la ECU: un archivo por encendido, como el viaje
static void abrirRegistro() {
  if (registrando) {
    return;
  }
  registrando = true;
  inicioRegistro = millis();
  proximaLinea = inicioRegistro;
  char texto[LARGO_LINEA_REGISTRO];
  tarjeta.abrir(texto, encabezadoRegistro(texto, sizeof(texto)));
  anotar("Inicio de la placa: %s", motivoInicio());
}

// Una línea por segundo, con lo de la tabla del monitor; sin un valor
// actual, el campo va vacío. Si loop() estuvo bloqueado (conectando el BLE),
// los segundos que faltan no se escriben: la columna Seg salta.
static void registrarSegundo() {
  uint32_t ahora = millis();
  if (!registrando || (int32_t)(ahora - proximaLinea) < 0) {
    return;
  }
  uint32_t segundo = (ahora - inicioRegistro) / PERIODO_LINEA_MS;
  proximaLinea = inicioRegistro + (segundo + 1) * PERIODO_LINEA_MS;

  char texto[LARGO_LINEA_REGISTRO];
  char valor[24];
  LineaCsv linea(texto, sizeof(texto));
  snprintf(valor, sizeof(valor), "%lu", (unsigned long)segundo);
  linea.texto(valor);
  FechaHora f;
  bool conHora = rtc.ahora(&f);
  if (conHora) {
    textoFechaHora(f, valor, sizeof(valor));
  }
  linea.texto(conHora ? valor : nullptr);
  for (uint8_t i = 0; i < CANT_MAGNITUDES; i++) {
    linea.texto(textoValor((Magnitud)i, ahora, valor, sizeof(valor)) ? valor : nullptr);
    if (i == VELOCIDAD) {
      // 0 sin marcha
      snprintf(valor, sizeof(valor), "%u", marcha.numero());
      linea.texto(marcha.actual(ahora) ? valor : nullptr);
    }
  }
  snprintf(valor, sizeof(valor), "%d", consultasPorSegundo);
  linea.texto(estado == Estado::LEYENDO && consultasPorSegundo >= 0 ? valor : nullptr);
  snprintf(valor, sizeof(valor), "%.2f", viaje.distanciaKm());
  linea.texto(valor);
  snprintf(valor, sizeof(valor), "%lu", (unsigned long)(viaje.tiempoEnMarchaMs() / 1000));
  linea.texto(valor);
  LecturaG g = imu.lectura(ahora);
  snprintf(valor, sizeof(valor), "%+.2f", g.lateral);
  linea.texto(g.actual ? valor : nullptr);
  snprintf(valor, sizeof(valor), "%+.2f", g.longitudinal);
  linea.texto(g.actual ? valor : nullptr);
  // La pantalla: qué página se ve y lo que tardan sus cuadros
  linea.texto(estado == Estado::LEYENDO && !descubriendo ? paginas.titulo() : nullptr);
  MedidasPantalla m = pantallaHw.medidas();
  snprintf(valor, sizeof(valor), "%u", m.cuadros);
  linea.texto(valor);
  const uint32_t tiempos[] = {m.promedio.dibujo, m.maximo.dibujo, m.promedio.espera,
                              m.maximo.espera,   m.promedio.giro, m.maximo.giro,
                              m.promedio.envio,  m.maximo.envio};
  for (uint32_t us : tiempos) {
    snprintf(valor, sizeof(valor), "%.1f", ms(us));
    linea.texto(m.cuadros > 0 ? valor : nullptr);
  }
  linea.texto(eventos.texto());
  eventos.vaciar();
  tarjeta.agregar(texto, linea.terminar());
}

// Para la barra y la espera: "14:35"
static bool horaParaPantalla(char *texto, size_t tam) {
  FechaHora f;
  if (!rtc.ahora(&f)) {
    return false;
  }
  textoHora(f, texto, tam);
  return true;
}

// !hora: sin nada, la muestra; con "AAAA-MM-DD HH:MM", la pone en el RTC.
// En el registro queda anotado: la columna Hora salta.
static void ponerHora(const char *texto) {
  if (texto[0] == '\0') {
    rtc.imprimir();
    return;
  }
  FechaHora f;
  if (!interpretarHora(texto, &f)) {
    Serial.println("Consola: !hora AAAA-MM-DD HH:MM (o HH:MM:SS), por ejemplo "
                   "!hora 2026-10-27 14:35");
    return;
  }
  if (!rtc.poner(f)) {
    Serial.println("Reloj: no se pudo poner (el PCF85063 no contesta)");
    return;
  }
  rtc.imprimir();
  char puesta[24];
  textoFechaHora(f, puesta, sizeof(puesta));
  anotar("Reloj puesto en hora: %s", puesta);
}

// Cada cambio de la tarjeta sale en el monitor serie; un error, también en
// el registro (se ve si después vuelve a grabar)
static void revisarTarjeta() {
  EstadoTarjeta ahora = tarjeta.estado();
  if (ahora == tarjetaAntes) {
    return;
  }
  tarjetaAntes = ahora;
  tarjeta.imprimir();
  if (ahora == EstadoTarjeta::ERROR) {
    anotar("Tarjeta: error al escribir");
  }
}

// El sondeo consulta en cada vuelta lo que se ve en la página actual. En
// Fallas, Info, Viaje, Anteriores, Fuerza G y DS3 no hay datos a la vista: se
// siguen pidiendo los de Motor, juntos. Si no, la ECU se consulta de a un PID
// y espaciado, y en el auto hubo 4 NO DATA seguidos con el motor andando, a
// uno de un SIN_ECU falso (E5). Para el viaje, además, la velocidad va en
// todas (la distancia sale de integrarla) y la RPM en las que entra, para no
// perder el máximo. En Mezcla no entra: con sus 5 y la velocidad ya son
// MAX_VISIBLES. La velocidad sirve también para recalibrar la fuerza G.
static void actualizarVisibles() {
  static const Magnitud DEL_VIAJE[] = {VELOCIDAD, RPM};
  Magnitud lista[MAX_VISIBLES];
  uint8_t cantidad = paginas.visibles(lista);
  if (cantidad == 0) {
    cantidad = paginas.primeras(lista);
  }
  for (Magnitud m : DEL_VIAJE) {
    bool esta = false;
    for (uint8_t i = 0; i < cantidad; i++) {
      esta = esta || lista[i] == m;
    }
    if (!esta && cantidad < MAX_VISIBLES) {
      lista[cantidad++] = m;
    }
  }
  sondeo.visibles(lista, cantidad);
}

// Las páginas saben si hay datos (E22): sin datos se recorren la espera,
// Anteriores y DS3, y al volver puede quedar otra página que la de antes
static void cambiarA(Estado nuevo, uint32_t esperaMs = 0) {
  estado = nuevo;
  proximoIntento = millis() + esperaMs;
  // SIN_ECU lee ATRV apenas entra
  ultimaLectura = millis() - PERIODO_ATRV_MS;
  paginas.conDatos(nuevo == Estado::LEYENDO);
  if (nuevo == Estado::LEYENDO) {
    abrirRegistro();
    actualizarVisibles();
  }
  if (nuevo == Estado::SIN_ECU || nuevo == Estado::BT_PERDIDO) {
    Serial.printf("== %s (%s)\n", textoEstado(nuevo), motivo);
    anotar("%s (%s)", textoEstado(nuevo), motivo);
  } else {
    Serial.printf("== %s\n", textoEstado(nuevo));
    anotar("%s", textoEstado(nuevo));
  }
  dibujar();
}

// La página Fallas se lee al entrar: mientras tanto muestra "Leyendo...",
// no la lectura anterior.
static void reiniciarFallas() {
  fallas = LecturaFallas();
  proximaLecturaFallas = millis();
}

// El botón y los toques cambian de página con las páginas o la espera a la
// vista, no descubriendo. Tocar Anteriores pasa a otro viaje. No usa el ELM:
// se llama también mientras se lo espera, así la página cambia sin esperar a
// que termine la consulta. Sin datos, también desde el evento de LVGL del
// toque, que puede ser de la otra tarea si loop() está conectando el BLE.
static void atenderPaginas() {
  boton.actualizar();
  int8_t paso = paginas.tomarToque();
  if (boton.pulsado()) {
    paso = 1;
  }
  bool otroViaje = paginas.tomarOtroViaje();
  if ((paso == 0 && !otroViaje) || descubriendo) {
    return;
  }
  if (paso != 0) {
    paginas.cambiar(paso);
    Serial.printf("Pagina: %s\n", paginas.titulo());
  }
  if (paso != 0 && estado == Estado::LEYENDO) {
    if (paginas.muestraFallas()) {
      reiniciarFallas();
    }
    if (paginas.muestraInfo()) {
      // El de la última vez ya no vale: se mide en la vuelta siguiente
      rssi = 0;
      ultimoRssi = millis() - PERIODO_RSSI_MS;
    }
    actualizarVisibles();
  }
  dibujar();
}

// En Fuerza G el punto se mueve: se redibuja más seguido
static uint32_t periodoDibujo() {
  bool fuerzaG = estado == Estado::LEYENDO && !descubriendo && paginas.muestraFuerzaG();
  return fuerzaG ? PERIODO_FUERZA_G_MS : PERIODO_PANTALLA_MS;
}

// Mientras se espera al ELM, la pantalla sigue al día: así un dato que se
// vence mientras tanto no queda a la vista. No usa el ELM. Corre también
// mientras se busca el adaptador, con LVGL suelto (ver conectarBt()).
static void alEsperar() {
  pantallaHw.tomar();
  atenderPaginas();
  if (millis() - ultimoDibujo >= periodoDibujo()) {
    dibujar();
  }
  pantallaHw.atender();
  pantallaHw.soltar();
}

// Espera atendiendo la pantalla
static void esperar(uint32_t ms) {
  uint32_t inicio = millis();
  while (millis() - inicio < ms) {
    pantallaHw.atender();
    delay(5);
  }
}

// Lo que se sabía del adaptador y de la ECU deja de valer
static void olvidarDatos() {
  version[0] = '\0';
  direccion[0] = '\0';
  rssi = 0;
  marcarSinConexion();
}

// Los reintentos que siguen dicen "Esperando el adaptador", sin las fases
static void perderConexion(const char *por) {
  motivo = por;
  olvidarDatos();
  avisarConexion = false;
  ecuPerdida = false;
  cambiarA(Estado::BT_PERDIDO, REINTENTO_BT_MS);
}

// !escanear y !explorar sueltan el ELM y bloquean varios segundos (hasta 30
// si un dispositivo conecta y no contesta): antes se sacan los datos, y
// mientras tanto LVGL queda suelto, como en conectarBt(). Si no, el guardián
// reiniciaría la placa.
static void alExplorar() {
  perderConexion("Explorando BLE");
  pantallaHw.soltar();
}

static void alTerminarExploracion() {
  pantallaHw.tomar();
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

// Al arrancar, con Reconectar u Olvidar y al tocar la espera, se ve cada
// fase. En los reintentos queda "Esperando el adaptador": con la llave
// sacada el adaptador está apagado, y cambiar el texto cada pocos segundos
// molesta. Se llama con LVGL suelto (ver conectarBt()), y con Reconectar u
// Olvidar todavía en LEYENDO: ya no hay datos.
static void alCambiarFase(FaseConexion fase) {
  if (!avisarConexion) {
    textoConexion = "Esperando el adaptador";
  } else if (fase == FaseConexion::BUSCANDO) {
    textoConexion = "Buscando el adaptador...";
  } else {
    textoConexion = "Conectando con el adaptador...";
  }
  pantallaHw.tomar();
  paginas.conDatos(false);
  sinDatos(textoConexion);
  pantallaHw.soltar();
}

// !reconectar, !olvidar y los botones de Info. Es una conexión pedida: se
// ven las fases, y la pantalla cambia antes de soltar el adaptador, que
// puede tardar.
static void volverAConectar(bool olvidando) {
  avisarConexion = true;
  pantallaHw.soltar();
  alCambiarFase(olvidando ? FaseConexion::BUSCANDO : FaseConexion::POR_DIRECCION);
  if (olvidando) {
    elm.olvidar();
  } else {
    elm.desconectar();
  }
  pantallaHw.tomar();
  olvidarDatos();
  cambiarA(Estado::CONECTANDO_BT);
}

// Conectar bloquea hasta 4 s por intento, y hasta 30 s si el adaptador
// conecta pero no contesta (ver las notas de E8). Mientras tanto LVGL queda
// suelto: lo atiende la otra tarea de PantallaHw.
static void conectarBt() {
  pantallaHw.soltar();
  bool conectado = elm.conectar(alCambiarFase);
  pantallaHw.tomar();
  if (conectado) {
    fallasElm = 0;
    strlcpy(direccion, elm.direccion().c_str(), sizeof(direccion));
    marcarConectado();
    cambiarA(Estado::INICIANDO_ELM);
  } else {
    motivo = "No conecta con el adaptador";
    avisarConexion = false;
    cambiarA(Estado::BT_PERDIDO, REINTENTO_BT_MS);
  }
}

static void inicializarElm() {
  if (!yaEs(proximoIntento)) {
    return; // esperando para reintentar
  }

  for (const char *comando : INICIALIZACION) {
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
    viaje.voltaje(v, millis());
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
  uint32_t ahora = millis();
  actualizarDato(m, valor, ahora);
  if (m == RPM) {
    lecturasRpm++;
    viaje.rpm(valor, ahora);
  } else if (m == VELOCIDAD) {
    viaje.velocidad(valor, ahora);
  } else if (m == REFRIGERANTE) {
    viaje.refrigerante(valor);
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
// de varios PIDs. Devuelve cuántas. Con la RPM y la velocidad, la marcha.
static uint8_t guardarVarias(const Respuesta &respuesta, const Magnitud *lista, uint8_t cantidad) {
  DatoPid pids[MAX_PIDS_POR_PEDIDO];
  uint8_t llegados = separarPids(respuesta.datos, respuesta.largo, pids, MAX_PIDS_POR_PEDIDO);
  uint8_t guardadas = 0;
  bool conRpm = false;
  bool conVelocidad = false;
  float rpm = 0;
  float kmh = 0;
  for (uint8_t i = 0; i < cantidad; i++) {
    for (uint8_t j = 0; j < llegados; j++) {
      float valor;
      if (pids[j].pid == pidDe(lista[i]) &&
          valorDesdePid(lista[i], pids[j].datos, pids[j].largo, &valor)) {
        guardar(lista[i], valor);
        guardadas++;
        if (lista[i] == RPM) {
          conRpm = true;
          rpm = valor;
        } else if (lista[i] == VELOCIDAD) {
          conVelocidad = true;
          kmh = valor;
        }
        break;
      }
    }
  }
  if (conRpm && conVelocidad) {
    marcha.lectura(rpm, kmh, millis());
  }
  return guardadas;
}

// De las magnitudes, las que se piden por PID y la ECU soporta, hasta
// PIDS_POR_CONSULTA. Devuelve cuántas dejó en lista.
static uint8_t pedibles(const Magnitud *magnitudes, uint8_t cantidad, Magnitud *lista) {
  uint8_t pedidas = 0;
  for (uint8_t i = 0; i < cantidad && pedidas < PIDS_POR_CONSULTA; i++) {
    Magnitud m = magnitudes[i];
    if (m != VOLT_ELM && dato(m).estado != EstadoDato::NO_SOPORTADO) {
      lista[pedidas++] = m;
    }
  }
  return pedidas;
}

// Prueba un pedido de varios PIDs con los de la página actual que la ECU
// soporta; si son menos de dos (Aire/Volt, Fallas, Info), con los de Motor.
// Si vuelven todos, el sondeo los pide juntos; si no, de a uno, como en el
// anterior. Con los datos reiniciados: lo que llega ya se muestra.
// false si se perdió la conexión.
static bool probarVariosPids() {
  Magnitud magnitudes[MAX_VISIBLES];
  Magnitud lista[PIDS_POR_CONSULTA];
  uint8_t cantidad = paginas.visibles(magnitudes);
  cantidad = pedibles(magnitudes, cantidad, lista);
  if (cantidad < 2) {
    cantidad = paginas.primeras(magnitudes);
    cantidad = pedibles(magnitudes, cantidad, lista);
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
    ecuPerdida = false;
    fallasEcu = 0;
    ultimaRespuestaEcu = millis();
    consultas = 0;
    msConsultas = 0;
    lecturasRpm = 0;
    consultasPorSegundo = -1;
    ultimaTabla = millis();
    sondeo.reiniciar(millis());
    reiniciarFallas();
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
    // Con el motor andando la ECU no deja de contestar: no es lo normal
    ecuPerdida = dato(RPM).estado == EstadoDato::VALIDO && dato(RPM).valor > 0;
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
  if (marcha.actual(ahora)) {
    Serial.printf(" Marcha %u |", marcha.numero());
  } else {
    Serial.print(" Marcha -- |");
  }
  // Con el promedio de cada consulta se ve cuánto se va fuera de ellas
  uint32_t lapso = ahora - ultimaTabla;
  consultasPorSegundo = (int16_t)(consultas * 1000UL / lapso);
  // Y el rato más largo sin leer el touch: un toque más corto se pierde
  Serial.printf(" %d consultas/s, %lu ms cada una, RPM %lu/s, touch sin leer hasta %lu ms",
                consultasPorSegundo, (unsigned long)(consultas > 0 ? msConsultas / consultas : 0),
                (unsigned long)lecturasRpm * 1000 / lapso, (unsigned long)pantallaHw.huecoMaximo());
  // Y lo que tardan los cuadros de la pantalla, que se le restan a loop()
  Serial.print(", pantalla ");
  imprimirMedidas(pantallaHw.medidas());
  Serial.printf(", viaje %.2f km en %lu s", viaje.distanciaKm(),
                (unsigned long)(viaje.tiempoEnMarchaMs() / 1000));
  LecturaG g = imu.lectura(ahora);
  if (g.actual) {
    Serial.printf(", G lateral %+.2f long. %+.2f\n", g.lateral, g.longitudinal);
  } else {
    Serial.println(", G --");
  }
  consultas = 0;
  msConsultas = 0;
  lecturasRpm = 0;
  ultimaTabla = ahora;
}

static void imprimirCodigos(const char *nombre, const char *servicio, const Codigos &codigos) {
  Serial.printf("Fallas: %s (%s): ", nombre, servicio);
  if (codigos.estado != EstadoCodigos::LEIDOS) {
    Serial.printf("%s (%s)\n",
                  codigos.estado == EstadoCodigos::NO_DISPONIBLE ? "no disponible" : "error",
                  nombreTipo(codigos.tipo));
    return;
  }
  Serial.printf("%u", codigos.lista.cantidad);
  for (uint8_t i = 0; i < codigos.lista.cantidad; i++) {
    char texto[6];
    textoDtc(codigos.lista.codigos[i], texto);
    const char *descripcion = descripcionDtc(codigos.lista.codigos[i]);
    if (descripcion != nullptr) {
      Serial.printf(" %s (%s)", texto, descripcion);
    } else {
      Serial.printf(" %s", texto);
    }
  }
  Serial.println();
}

static void imprimirFallas(TipoRespuesta tipoLuz) {
  if (fallas.luzLeida) {
    Serial.printf("Fallas: luz %s, %u guardadas segun 0101\n", fallas.luz ? "ENCENDIDA" : "apagada",
                  fallas.cantidadLuz);
  } else {
    Serial.printf("Fallas: luz sin leer (%s)\n", nombreTipo(tipoLuz));
  }
  imprimirCodigos("guardadas", "03", fallas.guardados);
  imprimirCodigos("pendientes", "07", fallas.pendientes);
  imprimirCodigos("permanentes", "0A", fallas.permanentes);
  if (errorAlLeer(fallas)) {
    Serial.printf("Fallas: error al leer, reintento en %lu s\n",
                  (unsigned long)(REINTENTO_FALLAS_MS / 1000));
  } else if (pantallaFallas(fallas, 0).vista == VistaFallas::SIN_FALLAS) {
    Serial.println("Fallas: sin fallas");
  }
}

// Para el registro: "Fallas: luz encendida; guardadas P0300 P0171". La
// página las lee cada PERIODO_FALLAS_MS: se anotan solo si cambiaron.
static void anotarFallas() {
  static const char *const NOMBRES[] = {"guardadas", "pendientes", "permanentes"};
  const Codigos *grupos[] = {&fallas.guardados, &fallas.pendientes, &fallas.permanentes};
  char texto[LARGO_EVENTOS];
  size_t largo = snprintf(texto, sizeof(texto), "Fallas: luz %s",
                          !fallas.luzLeida ? "sin leer" : (fallas.luz ? "encendida" : "apagada"));
  bool hay = false;
  for (uint8_t i = 0; i < 3 && largo < sizeof(texto); i++) {
    const Codigos &codigos = *grupos[i];
    if (codigos.estado == EstadoCodigos::ERROR) {
      largo += snprintf(texto + largo, sizeof(texto) - largo, "; %s: error", NOMBRES[i]);
      hay = true;
    } else if (codigos.estado == EstadoCodigos::LEIDOS && codigos.lista.cantidad > 0) {
      largo += snprintf(texto + largo, sizeof(texto) - largo, "; %s", NOMBRES[i]);
      for (uint8_t j = 0; j < codigos.lista.cantidad && largo < sizeof(texto); j++) {
        char dtc[6];
        textoDtc(codigos.lista.codigos[j], dtc);
        largo += snprintf(texto + largo, sizeof(texto) - largo, " %s", dtc);
      }
      hay = true;
    }
  }
  if (!hay && largo < sizeof(texto)) {
    snprintf(texto + largo, sizeof(texto) - largo, "; sin codigos");
  }
  if (strcmp(texto, fallasAnotadas) != 0) {
    strlcpy(fallasAnotadas, texto, sizeof(fallasAnotadas));
    anotar("%s", texto);
  }
}

// Luz de falla (0101) y códigos guardados (03), pendientes (07) y
// permanentes (0A). Todo es lectura: no hay nada que borre fallas.
// false si cambió el estado.
static bool leerFallas() {
  static const char *const SERVICIOS[] = {"03", "07", "0A"};
  LecturaFallas lectura;
  Codigos *destinos[] = {&lectura.guardados, &lectura.pendientes, &lectura.permanentes};

  ResultadoComando r = elm.enviar("01011", TIMEOUT_COMANDO_MS);
  if (r.estado == EstadoTransporte::DESCONECTADO) {
    perderConexion("Conexion perdida");
    return false;
  }
  Respuesta luz = interpretar(r.cruda.c_str(), "01011");
  lectura.luzLeida = r.estado == EstadoTransporte::OK && luz.tipo == TipoRespuesta::DATOS &&
                     luzFalla(luz.datos, luz.largo, &lectura.luz, &lectura.cantidadLuz);
  bool aviso = luz.tipo == TipoRespuesta::AVISO_ELM;

  for (uint8_t i = 0; i < 3; i++) {
    r = elm.enviar(SERVICIOS[i], TIMEOUT_COMANDO_MS);
    if (r.estado == EstadoTransporte::DESCONECTADO) {
      perderConexion("Conexion perdida");
      return false;
    }
    *destinos[i] =
        interpretarCodigos(r.cruda.c_str(), SERVICIOS[i], r.estado == EstadoTransporte::OK);
    aviso = aviso || destinos[i]->tipo == TipoRespuesta::AVISO_ELM;
  }
  if (aviso) {
    cambiarA(Estado::INICIANDO_ELM);
    return false;
  }

  lectura.leida = true;
  fallas = lectura;
  proximaLecturaFallas = millis() + (errorAlLeer(fallas) ? REINTENTO_FALLAS_MS : PERIODO_FALLAS_MS);
  imprimirFallas(luz.tipo);
  anotarFallas();
  dibujar();
  return true;
}

static void leer() {
  // Lo de la consulta anterior se dibuja mientras se espera esta: así el
  // dibujo no se suma a lo que tarda el adaptador
  dibujar();
  Magnitud m;
  if (paginas.muestraFallas() && yaEs(proximaLecturaFallas)) {
    // La página Fallas se lee aparte, no en el sondeo continuo
    if (!leerFallas()) {
      return;
    }
  } else if (sondeo.siguiente(millis(), m)) {
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
    // Hasta la primera fase, lo de BT_PERDIDO
    textoConexion = "Esperando el adaptador";
    cambiarA(Estado::CONECTANDO_BT);
  }
}

// Mientras se ve Info. Lo da la radio de la placa: no usa el ELM.
static void medirRssi() {
  if (estado != Estado::LEYENDO || !paginas.muestraInfo() ||
      millis() - ultimoRssi < PERIODO_RSSI_MS) {
    return;
  }
  ultimoRssi = millis();
  rssi = elm.rssi();
}

// Tocar la espera: el próximo intento va ya, sin esperar. Si hay uno en
// curso, nada.
static void atenderReintento() {
  if (!pantalla.tomarReintento()) {
    return;
  }
  bool esperando = estado == Estado::BT_PERDIDO || estado == Estado::SIN_ECU ||
                   (estado == Estado::INICIANDO_ELM && !yaEs(proximoIntento));
  if (!esperando) {
    return;
  }
  Serial.println("Pantalla: reintento");
  if (estado == Estado::BT_PERDIDO) {
    // Pedida: se ven las fases
    avisarConexion = true;
  }
  proximoIntento = millis();
}

static void revisarAlarma(TipoAlarma tipo, bool hay, float valor, uint32_t momento) {
  CambioAlarma cambio = alarmas.revisar(tipo, hay, valor, momento);
  if (cambio == CambioAlarma::NINGUNO) {
    return;
  }
  char texto[32];
  textoAlarma(tipo, alarmas.alarma(tipo).valor(), texto, sizeof(texto));
  if (cambio == CambioAlarma::SE_PRENDIO) {
    Serial.printf("Alarma: %s\n", texto);
    anotar("Alarma: %s", texto);
    sonido.pitarAlarma();
  } else {
    Serial.printf("Alarma: termino (%s)\n", texto);
    anotar("Alarma: termino (%s)", texto);
  }
}

// Con los valores actuales de la ECU y del ELM, en cada vuelta. Fuera de
// LEYENDO no hay valores: se apagan. Cada una cuenta el tiempo con el
// momento de su lectura, así que revisar la misma lectura otra vez no cambia
// nada. El voltaje, solo con el motor en marcha (la regla de VIAJE).
static void revisarAlarmas() {
  uint32_t ahora = millis();
  bool leyendo = estado == Estado::LEYENDO && !descubriendo;
  float valor = 0;
  bool hay = leyendo && valorActual(REFRIGERANTE, ahora, &valor);
  revisarAlarma(REFRIGERANTE_ALTO, hay, valor, dato(REFRIGERANTE).actualizadoMs);
  hay = leyendo && valorActual(VOLT_ELM, ahora, &valor) &&
        viaje.enMarchaDesdeHace(ahora, ESPERA_VOLTAJE_VIAJE_MS);
  revisarAlarma(VOLTAJE_BAJO, hay, valor, dato(VOLT_ELM).actualizadoMs);
  revisarAlarma(VOLTAJE_ALTO, hay, valor, dato(VOLT_ELM).actualizadoMs);
}

// El aviso de cambio (E25), con la RPM y la marcha que se muestra. Como las
// alarmas, revisar la misma lectura otra vez no cambia nada. Al empezar, el
// pitido, y la pantalla parpadea enseguida.
static void revisarCambio() {
  uint32_t ahora = millis();
  bool leyendo = estado == Estado::LEYENDO && !descubriendo;
  float rpm = 0;
  bool hay = leyendo && valorActual(RPM, ahora, &rpm);
  uint8_t numero = marcha.actual(ahora) ? marcha.numero() : 0;
  if (avisoCambio.revisar(hay, rpm, numero, dato(RPM).actualizadoMs)) {
    Serial.printf("Cambio: %.0f rpm en %uª\n", rpm, numero);
    anotar("Aviso de cambio: %.0f rpm en %uª", rpm, numero);
    sonido.pitarCambio();
    dibujar();
  }
}

// Con la ECU en 0 km/h, el IMU recalibra si la placa está quieta
static void avisarParado() {
  float kmh = 0;
  bool leyendo = estado == Estado::LEYENDO && !descubriendo;
  imu.parado(leyendo && valorActual(VELOCIDAD, millis(), &kmh) && kmh == 0);
}

// El viaje de ahora empieza: al arrancar (la placa arranca con la llave) y
// al reiniciarlo. Guarda la hora para Anteriores (E22).
static void empezarViaje() {
  FechaHora f;
  guardados.empezar(rtc.ahora(&f) ? &f : nullptr);
}

// Mantener apretada la espera, por si la placa quedó trabada. Corre en un
// evento de LVGL, que puede ser de la otra tarea si loop() está bloqueado.
static void reiniciarPlaca() {
  Serial.println("Pantalla: reiniciando la placa");
  esp_restart();
}

// De Info (- y +) o de la consola (!brillo N). No usa el ELM. La luz se
// pone aunque sea el mismo nivel: después de un !luz, vuelve.
static void cambiarBrillo(uint8_t nivel) {
  bool cambio = brillo.poner(nivel);
  pantallaHw.ponerBrillo(brillo.valor());
  if (cambio) {
    dibujar();
  }
  Serial.printf("Pantalla: brillo %u de %u\n", brillo.nivel(), CANT_NIVELES_BRILLO);
}

// De Info (- y +) o de la consola (!cambio N). No usa el ELM. Un umbral
// nuevo queda listo para avisar.
static void cambiarUmbralCambio(uint16_t rpm) {
  if (umbralCambio.poner(rpm)) {
    avisoCambio.ponerUmbral(rpm);
    dibujar();
  }
  char texto[16];
  textoUmbralCambio(umbralCambio.rpm(), texto, sizeof(texto));
  Serial.printf("Aviso de cambio: %s\n", texto);
}

// !cambio: sin número lo muestra; con un número que no vale, dice cuáles valen
static void atenderCambioPedido(int32_t rpm) {
  if (rpm < 0) {
    char texto[16];
    textoUmbralCambio(umbralCambio.rpm(), texto, sizeof(texto));
    Serial.printf("Aviso de cambio: %s\n", texto);
  } else if (rpm > UINT16_MAX || !umbralCambioValido(REGLAS_CAMBIO, rpm)) {
    Serial.printf("Consola: el aviso de cambio va de %u a %u rpm, de a %u, o 0 (apagado)\n",
                  CAMBIO_MINIMO_RPM, CAMBIO_MAXIMO_RPM, PASO_CAMBIO_RPM);
  } else {
    cambiarUmbralCambio(rpm);
  }
}

// !luz V: un valor de la luz, de 0 a 255, sin guardar, para medir los
// niveles en el banco
static void probarLuz(uint8_t valor) {
  pantallaHw.ponerBrillo(valor);
  Serial.printf("Pantalla: luz %u de 255, sin guardar (!brillo N vuelve a un nivel)\n", valor);
}

// Reconectar y Olvidar, de Info (que se ve solo en LEYENDO) o de la
// consola. Acá y no mientras se espera al ELM: los dos lo sueltan. Y
// reiniciar el viaje, de la página Viaje, y el brillo y el aviso de cambio, de
// Info, que no usan el ELM. De la pantalla, todos los pedidos desde la vuelta
// anterior, en orden: desde E25, tocar rápido no pierde ninguno.
static void atenderAcciones() {
  bool reconectar = false;
  bool olvidar = false;
  for (Accion accion = paginas.tomarAccion(millis()); accion != Accion::NINGUNA;
       accion = paginas.tomarAccion(millis())) {
    if (estado != Estado::LEYENDO || descubriendo) {
      continue;
    }
    if (accion == Accion::RECONECTAR) {
      Serial.println("Pantalla: reconectando");
      reconectar = true;
    } else if (accion == Accion::OLVIDAR) {
      Serial.println("Pantalla: olvidando el adaptador");
      olvidar = true;
    } else if (accion == Accion::REINICIAR_VIAJE) {
      Serial.printf("Pantalla: viaje reiniciado (llevaba %.2f km en %lu s)\n", viaje.distanciaKm(),
                    (unsigned long)(viaje.tiempoEnMarchaMs() / 1000));
      // Si llegó a guardarse, pasa a Anteriores
      guardados.terminar(viaje);
      viaje.reiniciar();
      empezarViaje();
      imu.reiniciarMaximos();
      anotar("Viaje reiniciado");
      dibujar();
    } else if (accion == Accion::BAJAR_BRILLO || accion == Accion::SUBIR_BRILLO) {
      cambiarBrillo(brillo.nivel() + (accion == Accion::SUBIR_BRILLO ? 1 : -1));
    } else if (accion == Accion::BAJAR_CAMBIO || accion == Accion::SUBIR_CAMBIO) {
      cambiarUmbralCambio(otroUmbralCambio(REGLAS_CAMBIO, umbralCambio.rpm(),
                                           accion == Accion::SUBIR_CAMBIO ? 1 : -1));
    }
  }
  uint8_t valorLuz;
  if (consola.luzPedida(&valorLuz)) {
    probarLuz(valorLuz);
  }
  uint8_t nivel = consola.brilloPedido();
  if (nivel > CANT_NIVELES_BRILLO) {
    Serial.printf("Consola: el brillo va de 1 a %u\n", CANT_NIVELES_BRILLO);
  } else if (nivel > 0) {
    cambiarBrillo(nivel);
  }
  int32_t rpmCambio;
  if (consola.cambioPedido(&rpmCambio)) {
    atenderCambioPedido(rpmCambio);
  }
  if (consola.reconexionPedida() || reconectar) {
    volverAConectar(false);
  }
  if (consola.olvidoPedido() || olvidar) {
    volverAConectar(true);
  }
}

void setup() {
  // Escribir no espera nunca (E23): con un host USB que no lee, como la
  // radio del auto, cada línea esperaba hasta 2 s con la pantalla tomada
  Serial.setTxBufferSize(TAM_SALIDA_USB);
  Serial.setTxTimeoutMs(0);
  Serial.begin(115200);

  // La pantalla antes que el BLE: necesita RAM interna con DMA. Y el I2C de
  // la placa antes que la pantalla: en la V2, el reset del panel y la luz van
  // por el TCA9554. Lo que no anduvo se informa cuando abre el monitor (!info).
  iniciarPlaca();
  // Antes que la pantalla, para que la hora ya esté al prenderla
  rtc.iniciar();
  guardados.iniciar();
  empezarViaje();
  brillo.iniciar();
  umbralCambio.iniciar();
  avisoCambio.ponerUmbral(umbralCambio.rpm());
  bool conPantalla = pantallaHw.iniciar(brillo.valor());
  pantalla.iniciar();
  paginas.iniciar();
  paginas.alPedirSinDatos(atenderPaginas);
  pantalla.espera("Iniciando...");
  pantalla.alMantenerApretada(reiniciarPlaca);
  pantalla.alPedirHora(horaParaPantalla);
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

  boton.iniciar();
  // Lo que sigue tarda (la tarjeta, el BLE) y no usa LVGL: mientras tanto la
  // atiende la otra tarea, y el DS3 sigue entrando
  pantallaHw.soltar();
  sonido.iniciar();
  imu.iniciar();
  tarjeta.iniciar();
  elm.iniciar();
  pantallaHw.tomar();
  elm.mientrasEspera(alEsperar);
  consola.antesDeExplorar(alExplorar);
  consola.despuesDeExplorar(alTerminarExploracion);
  pantallaHw.alTrabarse(estadoActual);
  marcarSinConexion();
  actualizarVisibles();
  imprimirInfo();
  Serial.println("Escribi un comando para el ELM, !reconectar, !olvidar, !descubrir, !log, "
                 "!info, !pitar, !pitar cambio, !brillo N, !cambio N, !luz V, !imu, !sd, "
                 "!viajes, !hora AAAA-MM-DD HH:MM, !trabar, !escanear o !explorar N.");

  cambiarA(Estado::CONECTANDO_BT);
}

void loop() {
  atenderPaginas();
  consola.atender();
  atenderAcciones();
  atenderReintento();
  if (consola.descubrimientoPedido()) {
    atenderDescubrimiento();
  }
  if (consola.infoPedida()) {
    imprimirInfo();
  }
  if (consola.imuPedido()) {
    imprimirImu();
  }
  if (consola.sdPedido()) {
    tarjeta.imprimir();
  }
  if (consola.viajesPedido()) {
    guardados.imprimir();
  }
  const char *hora = consola.horaPedida();
  if (hora != nullptr) {
    ponerHora(hora);
  }
  if (consola.pitidoPedido()) {
    Serial.println(sonido.iniciado() ? "Consola: pitando" : "Consola: el sonido no anda");
    sonido.pitarAlarma();
  }
  if (consola.pitidoCambioPedido()) {
    Serial.println(sonido.iniciado() ? "Consola: el pitido del aviso de cambio"
                                     : "Consola: el sonido no anda");
    sonido.pitarCambio();
  }
  if (consola.trabaPedida()) {
    // Con LVGL tomado y sin atenderlo: el guardián tiene que reiniciar la placa
    Serial.printf("Consola: trabando la pantalla; el guardian reinicia a los %lu s\n",
                  (unsigned long)(GUARDIAN_MS / 1000));
    delay(GUARDIAN_MS + 5000);
    Serial.println("Consola: el guardian no reinicio la placa");
  }

  bool usaBt = estado != Estado::CONECTANDO_BT && estado != Estado::BT_PERDIDO;
  if (usaBt && !elm.conectado()) {
    perderConexion("Conexion perdida");
  }
  medirRssi();

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
  revisarAlarmas();
  revisarCambio();
  avisarParado();
  revisarTarjeta();
  registrarSegundo();
  guardados.actualizar(viaje, millis());
  brillo.guardar();
  umbralCambio.guardar();

  if (millis() - ultimoDibujo >= periodoDibujo()) {
    dibujar();
  }
  pantallaHw.atender();
  delay(1);
}
