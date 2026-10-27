#include "tarjeta.h"

#include <Arduino.h>
#include <dirent.h>
#include <driver/sdmmc_host.h>
#include <errno.h>
#include <esp_heap_caps.h>
#include <esp_vfs_fat.h>
#include <fcntl.h>
#include <sdmmc_cmd.h>
#include <unistd.h>

#include "config.h"

static const char *const MONTAJE = "/sdcard";
static const uint32_t PILA = 4096;

static bool listo = false;
static SemaphoreHandle_t cerrojo = nullptr;
static TaskHandle_t tarea = nullptr;
static sdmmc_card_t *tarjeta = nullptr;

// Lo que comparten loop() y la tarea, con el cerrojo
static BuferTexto pendientes;
static bool abrirPedido = false;
static EstadoTarjeta estadoActual = EstadoTarjeta::BUSCANDO;
static uint16_t numero = 0;
static esp_err_t errorMontaje = ESP_OK;
static uint64_t totalBytes = 0;
static uint64_t libresBytes = 0;
static uint32_t grabados = 0; // bytes en el archivo
static uint32_t erroresEscritura = 0;

// Solo de la tarea: el bloque que está escribiendo, y cuánto ya escribió
static char *escritura = nullptr;
static size_t porEscribir = 0;
static size_t escritos = 0;
static int archivo = -1;
static char ruta[32];

static void ponerEstado(EstadoTarjeta nuevo) {
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  estadoActual = nuevo;
  xSemaphoreGive(cerrojo);
}

static void medirEspacio() {
  uint64_t total = 0;
  uint64_t libres = 0;
  if (esp_vfs_fat_info(MONTAJE, &total, &libres) == ESP_OK) {
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    totalBytes = total;
    libresBytes = libres;
    xSemaphoreGive(cerrojo);
  }
}

// En modo SD de 1 bit, a 20 MHz (el ejemplo de Waveshare usa 40). Sin
// tarjeta, el ESP-IDF da ESP_ERR_TIMEOUT; sin FAT, ESP_FAIL.
static void montar() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t ranura = SDMMC_SLOT_CONFIG_DEFAULT();
  ranura.width = 1;
  ranura.clk = (gpio_num_t)PIN_SD_CLK;
  ranura.cmd = (gpio_num_t)PIN_SD_CMD;
  ranura.d0 = (gpio_num_t)PIN_SD_D0;
  esp_vfs_fat_mount_config_t config = {};
  config.format_if_mount_failed = false;
  config.max_files = 2;
  esp_err_t error = esp_vfs_fat_sdmmc_mount(MONTAJE, &host, &ranura, &config, &tarjeta);
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  errorMontaje = error;
  if (error == ESP_OK) {
    estadoActual = EstadoTarjeta::LISTA;
  } else if (error == ESP_ERR_TIMEOUT) {
    estadoActual = EstadoTarjeta::SIN_TARJETA;
  } else {
    estadoActual = EstadoTarjeta::ILEGIBLE;
  }
  xSemaphoreGive(cerrojo);
  if (error == ESP_OK) {
    medirEspacio();
  }
}

// El número siguiente al más alto de la tarjeta, así nunca pisa un archivo.
// Con O_EXCL, tampoco si el número ya está.
static bool abrirArchivo() {
  DIR *carpeta = opendir(MONTAJE);
  if (carpeta == nullptr) {
    return false;
  }
  uint16_t mayor = 0;
  uint16_t n;
  while (dirent *entrada = readdir(carpeta)) {
    if (numeroDeViaje(entrada->d_name, &n) && n > mayor) {
      mayor = n;
    }
  }
  closedir(carpeta);
  if (mayor >= MAX_VIAJES) {
    errno = ENOSPC;
    return false;
  }
  char nombre[16];
  nombreDeViaje(mayor + 1, nombre, sizeof(nombre));
  snprintf(ruta, sizeof(ruta), "%s/%s", MONTAJE, nombre);
  archivo = open(ruta, O_WRONLY | O_CREAT | O_EXCL, 0666);
  if (archivo < 0) {
    return false;
  }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  numero = mayor + 1;
  xSemaphoreGive(cerrojo);
  return true;
}

// Escribe el bloque y lo asienta en la FAT (fsync): si se corta la luz, el
// archivo queda hasta el último bloque. false si falló; lo que faltó queda
// para la vuelta siguiente.
static bool escribirBloque() {
  while (escritos < porEscribir) {
    ssize_t cantidad = write(archivo, escritura + escritos, porEscribir - escritos);
    if (cantidad <= 0) {
      return false;
    }
    escritos += cantidad;
  }
  if (fsync(archivo) != 0) {
    return false;
  }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  grabados += porEscribir;
  xSemaphoreGive(cerrojo);
  porEscribir = 0;
  escritos = 0;
  return true;
}

// Cierra el archivo; en la vuelta siguiente lo vuelve a abrir. Llena, no
// sigue.
static void fallar() {
  bool llena = errno == ENOSPC;
  if (archivo >= 0) {
    close(archivo);
    archivo = -1;
  }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  erroresEscritura++;
  estadoActual = llena ? EstadoTarjeta::LLENA : EstadoTarjeta::ERROR;
  xSemaphoreGive(cerrojo);
}

static void alEscribir(void *) {
  montar();
  for (;;) {
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(PERIODO_REGISTRO_MS));
    xSemaphoreTake(cerrojo, portMAX_DELAY);
    EstadoTarjeta estado = estadoActual;
    bool pedido = abrirPedido;
    xSemaphoreGive(cerrojo);
    if (!pedido || estado == EstadoTarjeta::SIN_TARJETA || estado == EstadoTarjeta::ILEGIBLE ||
        estado == EstadoTarjeta::LLENA) {
      continue;
    }
    if (archivo < 0) {
      if (numero == 0) {
        abrirArchivo();
      } else {
        archivo = open(ruta, O_WRONLY | O_APPEND);
      }
      if (archivo < 0) {
        fallar();
        continue;
      }
    }
    if (porEscribir == 0) {
      xSemaphoreTake(cerrojo, portMAX_DELAY);
      porEscribir = pendientes.tomar(escritura, TAM_BUFER_REGISTRO);
      xSemaphoreGive(cerrojo);
    }
    if (porEscribir > 0 && !escribirBloque()) {
      fallar();
      continue;
    }
    ponerEstado(EstadoTarjeta::GRABANDO);
    medirEspacio();
  }
}

void Tarjeta::iniciar() {
  // Los dos búferes, en la PSRAM: la RAM interna es para la pantalla y el BLE
  char *memoria = (char *)heap_caps_malloc(2 * TAM_BUFER_REGISTRO, MALLOC_CAP_SPIRAM);
  cerrojo = xSemaphoreCreateMutex();
  if (memoria == nullptr || cerrojo == nullptr) {
    Serial.println("Tarjeta: no se pudo iniciar (sin memoria)");
    return;
  }
  pendientes.usar(memoria, TAM_BUFER_REGISTRO);
  escritura = memoria + TAM_BUFER_REGISTRO;
  // Con prioridad 1, debajo del IMU y del sonido, en el núcleo 0 como ellos
  if (xTaskCreatePinnedToCore(alEscribir, "tarjeta", PILA, nullptr, 1, &tarea, 0) != pdPASS) {
    Serial.println("Tarjeta: no se pudo iniciar (la tarea)");
    return;
  }
  listo = true;
}

void Tarjeta::abrir(const char *encabezado, size_t largo) {
  if (!listo) {
    return;
  }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  if (!abrirPedido) {
    abrirPedido = true;
    pendientes.agregar(encabezado, largo);
  }
  xSemaphoreGive(cerrojo);
  xTaskNotifyGive(tarea);
}

void Tarjeta::agregar(const char *linea, size_t largo) {
  if (!listo) {
    return;
  }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  if (estadoActual != EstadoTarjeta::SIN_TARJETA && estadoActual != EstadoTarjeta::ILEGIBLE &&
      estadoActual != EstadoTarjeta::LLENA) {
    pendientes.agregar(linea, largo);
  }
  xSemaphoreGive(cerrojo);
}

EstadoTarjeta Tarjeta::estado() {
  if (!listo) {
    return EstadoTarjeta::SIN_TARJETA;
  }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  EstadoTarjeta e = estadoActual;
  xSemaphoreGive(cerrojo);
  return e;
}

uint16_t Tarjeta::viaje() {
  if (!listo) {
    return 0;
  }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  uint16_t n = numero;
  xSemaphoreGive(cerrojo);
  return n;
}

void Tarjeta::imprimir() {
  if (!listo) {
    Serial.println("Tarjeta: no se pudo iniciar");
    return;
  }
  xSemaphoreTake(cerrojo, portMAX_DELAY);
  EstadoTarjeta e = estadoActual;
  esp_err_t error = errorMontaje;
  uint16_t n = numero;
  uint64_t total = totalBytes;
  uint64_t libres = libresBytes;
  uint32_t bytes = grabados;
  uint32_t fallas = erroresEscritura;
  uint32_t perdidas = pendientes.perdidas();
  xSemaphoreGive(cerrojo);

  if (e == EstadoTarjeta::BUSCANDO) {
    Serial.println("Tarjeta: buscandola");
    return;
  }
  if (e == EstadoTarjeta::SIN_TARJETA) {
    Serial.printf("Tarjeta: no hay, o no contesta (0x%x). Se busca al prender.\n", error);
    return;
  }
  if (e == EstadoTarjeta::ILEGIBLE) {
    Serial.printf("Tarjeta: no se puede leer (0x%x): formatearla en FAT32\n", error);
    return;
  }
  Serial.printf("Tarjeta: %.8s, %.1f GB, %.1f GB libres; ", tarjeta->cid.name, total / 1e9,
                libres / 1e9);
  if (n == 0) {
    Serial.print("el archivo se abre con la primera lectura de la ECU");
  } else {
    Serial.printf("VIAJE%04u.CSV, %lu KB", (unsigned)n, (unsigned long)(bytes / 1024));
  }
  Serial.printf(", %lu lineas perdidas, %lu errores al escribir%s; pila libre %u bytes\n",
                (unsigned long)perdidas, (unsigned long)fallas,
                e == EstadoTarjeta::ERROR   ? " (ahora con error)"
                : e == EstadoTarjeta::LLENA ? " (llena)"
                                            : "",
                (unsigned)uxTaskGetStackHighWaterMark(tarea));
}
