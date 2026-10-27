# Arduino-ESP32 3.3 compila el proyecto y las bibliotecas sin ningún -O: su
# platformio-build.py toma los flags de flags/c_flags y flags/cpp_flags, que
# no lo traen, así que GCC queda en -O0 (el ESP-IDF viene ya compilado, a
# -Os). Esto sube a -O2 solo LVGL y el giro de la imagen para el panel, que
# son lo que cuesta cada cuadro de la pantalla (E17). Lo demás queda como se
# probó. -O2 va al final y le gana a cualquier -O anterior.
Import("env")

OPTIMIZADOS = ["*/lvgl/src/*", "*src/giro.cpp"]


def optimizar(env, nodo):
    return env.Object(nodo, CCFLAGS=env["CCFLAGS"] + ["-O2"])


for patron in OPTIMIZADOS:
    env.AddBuildMiddleware(optimizar, patron)
