#include "animaciones.h"

#include <stdlib.h>

#include "dibujo.h"
#include "imagenes/imagenes.h"
#include "llanta.h"

#ifdef ESP_PLATFORM
#include <esp_heap_caps.h>
#endif

static const int32_t ANCHO = 640;
static const int32_t ALTO = 172;

// Los búferes de las llantas van a la PSRAM: la RAM interna es para el BLE
static uint8_t *pedirBufer(uint32_t bytes) {
#ifdef ESP_PLATFORM
  return (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
#else
  return (uint8_t *)malloc(bytes);
#endif
}

static lv_obj_t *imagen(lv_obj_t *padre, const void *fuente, int32_t x, int32_t y) {
  lv_obj_t *img = lv_image_create(padre);
  lv_obj_remove_flag(img, LV_OBJ_FLAG_CLICKABLE);
  lv_image_set_src(img, fuente);
  lv_obj_set_pos(img, x, y);
  return img;
}

static void crearLlanta(LlantaGirando &l, lv_obj_t *padre, const LlantaFuente &fuente) {
  l.bufer = pedirBufer(bytesLlanta(fuente));
  if (l.bufer == nullptr) {
    return;
  }
  l.dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
  l.dsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
  l.dsc.header.w = fuente.ancho;
  l.dsc.header.h = fuente.alto;
  l.dsc.header.stride = fuente.ancho * 2;
  l.dsc.data_size = bytesLlanta(fuente);
  l.dsc.data = l.bufer;
  girarLlanta(fuente, 0, l.bufer);
  l.grados = 0;
  l.imagen = imagen(padre, &l.dsc, fuente.x, fuente.y);
}

// Sin caché de imágenes (LV_CACHE_DEF_SIZE 0), LVGL lee el búfer en cada
// dibujo: alcanza con invalidarla
static void girar(LlantaGirando &l, const LlantaFuente &fuente, float grados) {
  if (l.imagen == nullptr || grados == l.grados) {
    return;
  }
  girarLlanta(fuente, grados, l.bufer);
  l.grados = grados;
  lv_obj_invalidate(l.imagen);
}

void Entrada::crear(lv_obj_t *padre, uint32_t desdeCuando, int32_t yAuto) {
  aparece = desdeCuando;
  y = yAuto;
  contenedor = caja(padre, entrada_cuadros[0].x, y, entrada_auto.header.w, entrada_auto.header.h);
  imagen(contenedor, &entrada_auto, 0, 0);
  for (int i = 0; i < 2; i++) {
    crearLlanta(llantas[i], contenedor, entrada_llantas[i]);
  }
  lv_obj_add_flag(contenedor, LV_OBJ_FLAG_HIDDEN);
}

void Entrada::ponerCuadro(uint8_t cuadro) {
  if (cuadro == cuadroPuesto) {
    return;
  }
  cuadroPuesto = cuadro;
  ponerPosicion(contenedor, entrada_cuadros[cuadro].x, y);
  for (int i = 0; i < 2; i++) {
    girar(llantas[i], entrada_llantas[i], entrada_cuadros[cuadro].grados);
  }
}

void Entrada::mover(uint32_t ahora, bool visible) {
  if (contenedor == nullptr) {
    return;
  }
  if (etapa == Etapa::OCULTO && (int32_t)(ahora - aparece) >= 0) {
    etapa = visible ? Etapa::ENTRANDO : Etapa::QUIETO;
    desde = ahora;
  }
  uint8_t ultimo = entrada_cantidad - 1;
  if (etapa == Etapa::ENTRANDO) {
    uint32_t cuadro = (ahora - desde) / entrada_ms_cuadro;
    if (cuadro >= ultimo) {
      etapa = Etapa::QUIETO;
    } else {
      ponerCuadro(cuadro);
    }
  }
  if (etapa == Etapa::QUIETO) {
    ponerCuadro(ultimo);
  }
  mostrar(contenedor, etapa != Etapa::OCULTO);
}

void Paseo::crear(lv_obj_t *padre) {
  contenedor = caja(padre, 0, 0, ANCHO, ALTO);
  // Dos veces las nubes, una al lado de la otra: se corren un ancho por vuelta
  nubes[0] = imagen(contenedor, &paseo_nubes, 0, 0);
  nubes[1] = imagen(contenedor, &paseo_nubes, ANCHO, 0);
  imagen(contenedor, &paseo_fondo, 0, paseo.y_fondo);
  rectangulo(contenedor, 0, paseo.y_camino, ANCHO, ALTO - paseo.y_camino,
             lv_color_hex(paseo.color_camino));
  // Una raya de más, para cubrir el ancho al correrse
  rayas = caja(contenedor, 0, paseo.y_raya, ANCHO + paseo.periodo_raya, paseo.alto_raya);
  for (int32_t x = 0; x <= ANCHO; x += paseo.periodo_raya) {
    rectangulo(rayas, x, 0, paseo.largo_raya, paseo.alto_raya, lv_color_hex(paseo.color_raya));
  }
  imagen(contenedor, &paseo_techo, paseo.techo_x, paseo.techo_y);
  for (int i = 0; i < 2; i++) {
    crearLlanta(llantas[i], contenedor, paseo_llantas[i]);
  }
}

void Paseo::mover(uint32_t ahora) {
  if (contenedor == nullptr || !lv_obj_is_visible(contenedor)) {
    return;
  }
  int32_t corrido = (int32_t)((ahora % paseo.duracion_ms) * ANCHO / paseo.duracion_ms);
  ponerPosicion(nubes[0], -corrido, 0);
  ponerPosicion(nubes[1], ANCHO - corrido, 0);
  // Entre -periodo_raya y 0
  int32_t x = (paseo.x_raya - corrido) % paseo.periodo_raya;
  ponerPosicion(rayas, x > 0 ? x - paseo.periodo_raya : x, paseo.y_raya);
  uint32_t vuelta = 360000 / paseo.grados_por_segundo; // ms por vuelta de las llantas
  float grados = (ahora % vuelta) * 360.0f / vuelta;
  for (int i = 0; i < 2; i++) {
    girar(llantas[i], paseo_llantas[i], grados);
  }
}
