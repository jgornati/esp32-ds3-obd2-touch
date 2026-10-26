# Render de las pantallas en la PC (ver render.cpp), con el LVGL que baja
# PlatformIO: hace falta un `pio run` antes. Desde la carpeta del proyecto:
#
#   make -C herramientas/render -j8
#
# Deja un PNG por pantalla, y todas.png, en herramientas/render/salida/.
# LVGL se compila la primera vez y de nuevo si cambia include/lv_conf.h.
# Necesita un compilador de C++17 y zlib.

PROYECTO := $(abspath ../..)
LVGL := $(PROYECTO)/.pio/libdeps/esp32s3/lvgl
OBJ := obj
SALIDA := salida

ifeq ($(wildcard $(LVGL)/lvgl.h),)
$(error Falta LVGL en .pio: correr antes ~/.platformio/penv/bin/pio run)
endif

INCLUIR := -DLV_CONF_INCLUDE_SIMPLE -DLV_KCONFIG_IGNORE -I$(PROYECTO)/include -I$(LVGL) \
           -I$(LVGL)/src -I$(PROYECTO)/lib/obd/src -I$(PROYECTO)/src

# Lo del firmware que dibuja. PantallaHw es de mentira (render.cpp).
CODIGO := render.cpp $(addprefix $(PROYECTO)/src/,paginas.cpp pantalla.cpp dibujo.cpp datos.cpp) \
          $(wildcard $(PROYECTO)/lib/obd/src/*.cpp)
CABECERAS := $(wildcard $(PROYECTO)/src/*.h $(PROYECTO)/include/*.h $(PROYECTO)/lib/obd/src/*.h)
FUENTES_O := $(patsubst $(PROYECTO)/src/fuentes/%.c,$(OBJ)/fuentes/%.o,$(wildcard $(PROYECTO)/src/fuentes/*.c))
LVGL_O := $(patsubst $(LVGL)/src/%.c,$(OBJ)/lvgl/%.o,$(shell find $(LVGL)/src -name '*.c'))

.PHONY: render limpiar

render: $(OBJ)/render
	@rm -rf $(SALIDA)
	@mkdir -p $(SALIDA)
	@$(OBJ)/render $(SALIDA)

$(OBJ)/render: $(CODIGO) $(CABECERAS) $(FUENTES_O) $(LVGL_O)
	@echo "Compilando el render"
	@$(CXX) -std=c++17 -O1 -Wall $(INCLUIR) $(CODIGO) $(FUENTES_O) $(LVGL_O) -lz -o $@

$(OBJ)/fuentes/%.o: $(PROYECTO)/src/fuentes/%.c $(PROYECTO)/include/lv_conf.h
	@mkdir -p $(@D)
	@$(CC) -O1 -w $(INCLUIR) -c $< -o $@

$(OBJ)/lvgl/%.o: $(LVGL)/src/%.c $(PROYECTO)/include/lv_conf.h
	@mkdir -p $(@D)
	@$(CC) -O1 -w $(INCLUIR) -c $< -o $@

limpiar:
	rm -rf $(OBJ) $(SALIDA)
