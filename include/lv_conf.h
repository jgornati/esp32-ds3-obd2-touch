/* Configuración de LVGL 9.3. Solo lo que cambia: lo demás toma el valor
 * por defecto de lv_conf_internal.h (el mismo de lv_conf_template.h). */

#ifndef LV_CONF_H
#define LV_CONF_H

/* RGB565. pantalla_hw da vuelta los bytes de cada píxel para el panel. */
#define LV_COLOR_DEPTH 16

/* malloc del sistema: lo grande va a la PSRAM */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/* Sin tarea propia: lv_timer_handler() se llama desde loop() */
#define LV_USE_OS LV_OS_NONE

/* Un assert reinicia la placa (con el motivo en el monitor serie) en vez de colgarla */
#define LV_ASSERT_HANDLER_INCLUDE <stdlib.h>
#define LV_ASSERT_HANDLER abort();

/* Avisos al monitor serie, con lv_log_register_print_cb() en pantalla_hw */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 0

/* Fuentes: Montserrat (traen el °, pero no tildes) y los números grandes de src/fuentes */
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_36 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(fuente_numeros_110)
#define LV_FONT_DEFAULT &lv_font_montserrat_20

#define LV_THEME_DEFAULT_DARK 1

/* Widgets: solo la etiqueta (el objeto base siempre está) */
#define LV_USE_ANIMIMG 0
#define LV_USE_ARC 0
#define LV_USE_BAR 0
#define LV_USE_BUTTON 0
#define LV_USE_BUTTONMATRIX 0
#define LV_USE_CALENDAR 0
#define LV_USE_CANVAS 0
#define LV_USE_CHART 0
#define LV_USE_CHECKBOX 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMAGE 1
#define LV_USE_IMAGEBUTTON 0
#define LV_USE_KEYBOARD 0
#define LV_USE_LABEL 1
#define LV_USE_LED 0
#define LV_USE_LINE 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_MSGBOX 0
#define LV_USE_ROLLER 0
#define LV_USE_SCALE 0
#define LV_USE_SLIDER 0
#define LV_USE_SPAN 0
#define LV_USE_SPINBOX 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TEXTAREA 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 0

#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_DEMOS 0

#endif /* LV_CONF_H */
