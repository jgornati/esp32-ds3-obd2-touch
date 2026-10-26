#include "dibujo.h"

#include <string.h>

void ponerTexto(lv_obj_t *e, const char *texto) {
  if (strcmp(lv_label_get_text(e), texto) != 0) {
    lv_label_set_text(e, texto);
  }
}

void ponerColor(lv_obj_t *e, lv_color_t color) {
  if (!lv_color_eq(lv_obj_get_style_text_color(e, 0), color)) {
    lv_obj_set_style_text_color(e, color, 0);
  }
}

void ponerFuente(lv_obj_t *e, const lv_font_t *fuente) {
  if (lv_obj_get_style_text_font(e, 0) != fuente) {
    lv_obj_set_style_text_font(e, fuente, 0);
  }
}

void ponerFondo(lv_obj_t *obj, lv_color_t color) {
  if (!lv_color_eq(lv_obj_get_style_bg_color(obj, 0), color)) {
    lv_obj_set_style_bg_color(obj, color, 0);
  }
}

void ponerPosicion(lv_obj_t *obj, int32_t x, int32_t y) {
  if (lv_obj_get_style_x(obj, 0) != x || lv_obj_get_style_y(obj, 0) != y) {
    lv_obj_set_pos(obj, x, y);
  }
}

void ponerAncho(lv_obj_t *obj, int32_t ancho) {
  if (lv_obj_get_style_width(obj, 0) != ancho) {
    lv_obj_set_width(obj, ancho);
  }
}

void mostrar(lv_obj_t *obj, bool visible) {
  bool oculto = lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN);
  if (visible && oculto) {
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else if (!visible && !oculto) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

lv_obj_t *caja(lv_obj_t *padre, int32_t x, int32_t y, int32_t ancho, int32_t alto) {
  lv_obj_t *obj = lv_obj_create(padre);
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, ancho, alto);
  return obj;
}

lv_obj_t *rectangulo(lv_obj_t *padre, int32_t x, int32_t y, int32_t ancho, int32_t alto,
                     lv_color_t color, int32_t radio) {
  lv_obj_t *obj = caja(padre, x, y, ancho, alto);
  lv_obj_set_style_bg_color(obj, color, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(obj, radio, 0);
  return obj;
}

lv_obj_t *etiqueta(lv_obj_t *padre, const lv_font_t *fuente, lv_color_t color) {
  lv_obj_t *e = lv_label_create(padre);
  lv_obj_set_style_text_font(e, fuente, 0);
  lv_obj_set_style_text_color(e, color, 0);
  lv_label_set_text(e, "");
  return e;
}

int32_t yDeBase(const lv_font_t *fuente, int32_t base) {
  return base - (fuente->line_height - fuente->base_line);
}
