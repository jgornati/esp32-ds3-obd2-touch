"""Las animaciones del DS3 (E9) a arreglos de C para LVGL.

Lee fuentes/entrada (el auto que entra a la pantalla de espera) y
fuentes/paseo (la página DS3) y escribe src/imagenes/entrada.c y paseo.c.
Las animaciones vienen como cuadros PNG enteros de 640x172, que no entran en
la flash: se guardan las partes y la placa las mueve. Las llantas se giran en
la placa (src/llanta.cpp).

Desde la carpeta del proyecto, con Python 3 y nada más:

    python3 herramientas/imagenes/convertir.py
"""

import json
import os
import struct
import zlib

AQUI = os.path.dirname(os.path.abspath(__file__))
FUENTES = os.path.join(AQUI, "fuentes")
SALIDA = os.path.join(AQUI, "..", "..", "src", "imagenes")

ANCHO = 640
ALTO = 172

# Paseo: las nubes y las rayas del camino se corren todo el ancho en cada
# vuelta (en el cuadro 30, la mitad). Las llantas giran 24 grados por cuadro:
# el cuadro 15 es igual al 0.
CUADRO_PASEO_MITAD = 30
GRADOS_POR_CUADRO_PASEO = 24
# Hasta dónde llegan las nubes y desde dónde empiezan las rayas del camino
ALTO_NUBES = 50
Y_RAYAS = 159
# Columnas por donde el techo del auto asoma entre las nubes, con margen
X_TECHO = (165, 475)
# En el techo: un píxel es del auto si se aleja más que esto de las nubes, y
# los huecos de hasta HUECO_TECHO píxeles en una fila se rellenan (el auto
# tiene partes casi del color del cielo)
TOLERANCIA_TECHO = 3
HUECO_TECHO = 3


def leer_png(ruta):
    """PNG de 8 bits, RGB o RGBA, sin entrelazado: filas de (r, g, b, a)."""
    datos = open(ruta, "rb").read()
    assert datos[:8] == b"\x89PNG\r\n\x1a\n", ruta
    pos = 8
    idat = b""
    while pos < len(datos):
        largo, tipo = struct.unpack(">I4s", datos[pos:pos + 8])
        cuerpo = datos[pos + 8:pos + 8 + largo]
        if tipo == b"IHDR":
            ancho, alto, bits, color, _, _, entrelazado = struct.unpack(">IIBBBBB", cuerpo)
            assert bits == 8 and color in (2, 6) and entrelazado == 0, ruta
        elif tipo == b"IDAT":
            idat += cuerpo
        pos += 12 + largo
    canales = 4 if color == 6 else 3
    crudo = zlib.decompress(idat)
    paso = ancho * canales
    filas = []
    anterior = bytearray(paso)
    i = 0
    for _ in range(alto):
        filtro = crudo[i]
        linea = bytearray(crudo[i + 1:i + 1 + paso])
        i += 1 + paso
        for x in range(paso):
            a = linea[x - canales] if x >= canales else 0
            b = anterior[x]
            c = anterior[x - canales] if x >= canales else 0
            if filtro == 1:
                linea[x] = (linea[x] + a) & 255
            elif filtro == 2:
                linea[x] = (linea[x] + b) & 255
            elif filtro == 3:
                linea[x] = (linea[x] + (a + b) // 2) & 255
            elif filtro == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                linea[x] = (linea[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        anterior = linea
        if canales == 4:
            filas.append([tuple(linea[x:x + 4]) for x in range(0, paso, 4)])
        else:
            filas.append([tuple(linea[x:x + 3]) + (255,) for x in range(0, paso, 3)])
    return filas


def rgb565(p):
    r, g, b = p[:3]
    return ((r * 31 + 127) // 255) << 11 | ((g * 63 + 127) // 255) << 5 | (b * 31 + 127) // 255


def color_lvgl(p):
    """0xRRGGBB que LVGL (que trunca) pasa al mismo RGB565 que las imágenes (que redondean)."""
    v = rgb565(p)
    r, g, b = v >> 11, (v >> 5) & 0x3F, v & 0x1F
    return "0x%02X%02X%02X" % (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def sobre_negro(p):
    a = p[3]
    return tuple((c * a + 127) // 255 for c in p[:3]) + (255,)


def distinto(p, q, tolerancia=3):
    return max(abs(p[i] - q[i]) for i in range(3)) > tolerancia


def recortar(filas, x, y, ancho, alto):
    return [fila[x:x + ancho] for fila in filas[y:y + alto]]


def caja(puntos):
    xs = [p[0] for p in puntos]
    ys = [p[1] for p in puntos]
    return min(xs), min(ys), max(xs), max(ys)


class Archivo:
    """Un .c con los arreglos y las estructuras de imagenes.h."""

    def __init__(self, nombre, origen):
        self.nombre = nombre
        self.partes = [
            "// Generado por herramientas/imagenes/convertir.py desde %s.\n"
            "// No editar a mano: cambiar las fuentes y volver a correrlo.\n\n"
            '#include "imagenes.h"\n' % origen
        ]
        self.bytes = 0

    def arreglo(self, nombre, tipo, valores, formato):
        self.bytes += len(valores) * (2 if tipo == "uint16_t" else 1)
        lineas = []
        por_linea = 12 if tipo == "uint16_t" else 16
        for i in range(0, len(valores), por_linea):
            lineas.append("    " + ", ".join(formato % v for v in valores[i:i + por_linea]) + ",")
        self.partes.append("\nstatic const %s %s[] = {\n%s\n};\n" % (tipo, nombre, "\n".join(lineas)))

    def imagen(self, nombre, filas, con_alfa):
        """lv_image_dsc_t en RGB565 (opaca) o RGB565A8 (los colores y después el alfa)."""
        alto = len(filas)
        ancho = len(filas[0])
        datos = []
        for fila in filas:
            for p in fila:
                v = rgb565(p)
                datos += [v & 0xFF, v >> 8]
        if con_alfa:
            datos += [p[3] for fila in filas for p in fila]
        self.arreglo(nombre + "_datos", "uint8_t", datos, "0x%02X")
        self.partes.append(
            "\nconst lv_image_dsc_t %s = {\n"
            "    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = %s, .w = %d, .h = %d, .stride = %d},\n"
            "    .data_size = sizeof(%s_datos),\n"
            "    .data = %s_datos,\n"
            "};\n" % (nombre, "LV_COLOR_FORMAT_RGB565A8" if con_alfa else "LV_COLOR_FORMAT_RGB565",
                      ancho, alto, ancho * 2, nombre, nombre))

    def llantas(self, nombre, filas, elipses):
        """Las llantas quietas, con su elipse: x, y del recorte en filas; cx, cy, rx, ry adentro."""
        estructuras = []
        for i, (cx, cy, rx, ry) in enumerate(elipses):
            # Un píxel de margen para el muestreo
            x0 = int(cx - rx) - 1
            y0 = int(cy - ry) - 1
            ancho = int(cx + rx + 0.999) + 1 - x0
            alto = int(cy + ry + 0.999) + 1 - y0
            datos = [rgb565(p) for fila in recortar(filas, x0, y0, ancho, alto) for p in fila]
            self.arreglo("%s_%d" % (nombre, i), "uint16_t", datos, "0x%04X")
            estructuras.append("    {%s_%d, %d, %d, %d, %d, %.2ff, %.2ff, %.2ff, %.2ff}," %
                               (nombre, i, ancho, alto, x0, y0, cx - x0, cy - y0, rx, ry))
        self.partes.append("\nconst LlantaFuente %s[2] = {\n%s\n};\n" % (nombre, "\n".join(estructuras)))

    def texto(self, texto):
        self.partes.append(texto)

    def escribir(self):
        ruta = os.path.join(SALIDA, self.nombre)
        with open(ruta, "w") as f:
            f.write("".join(self.partes))
        print("%s: %d KB de imagenes" % (os.path.relpath(ruta), self.bytes // 1024))


def elipses_de(diferencias):
    """Las dos llantas que giraron: centro y semiejes de cada lado."""
    medio = (min(p[0] for p in diferencias) + max(p[0] for p in diferencias)) / 2
    elipses = []
    for lado in ([p for p in diferencias if p[0] < medio], [p for p in diferencias if p[0] >= medio]):
        x0, y0, x1, y1 = caja(lado)
        elipses.append(((x0 + x1 + 1) / 2, (y0 + y1 + 1) / 2, (x1 - x0 + 1) / 2, (y1 - y0 + 1) / 2))
    return elipses


def entrada():
    carpeta = os.path.join(FUENTES, "entrada")
    animacion = json.load(open(os.path.join(carpeta, "animacion.json")))
    auto = leer_png(os.path.join(carpeta, "auto.png"))
    cuadros = animacion["positions"]
    assert animacion["sprite_size"] == [len(auto[0]), len(auto)]

    # La pantalla de espera es negra: el auto va mezclado con el negro y sin
    # alfa, recortado a lo que tiene algo
    x0, y0, x1, y1 = caja([(x, y) for y, fila in enumerate(auto) for x, p in enumerate(fila) if p[3] > 0])
    auto = [[sobre_negro(p) for p in fila[x0:x1 + 1]] for fila in auto[y0:y1 + 1]]

    # Las llantas: lo que cambia entre el auto y el último cuadro
    ultimo = cuadros[-1]
    cuadro = leer_png(os.path.join(carpeta, "frame_%03d.png" % ultimo["frame"]))
    diferencias = [(x, y) for y in range(len(auto)) for x in range(len(auto[0]))
                   if distinto(sobre_negro(cuadro[ultimo["y"] + y0 + y][ultimo["x"] + x0 + x]), auto[y][x], 0)]
    elipses = elipses_de(diferencias)

    c = Archivo("entrada.c", "fuentes/entrada")
    c.imagen("entrada_auto", auto, False)
    c.llantas("entrada_llantas", auto, elipses)
    filas = ["    {%d, %.1ff}," % (p["x"] + x0, p["wheel_angle_degrees_clockwise"]) for p in cuadros]
    c.texto("\nconst CuadroEntrada entrada_cuadros[] = {\n%s\n};\n" % "\n".join(filas))
    c.texto("const uint8_t entrada_cantidad = %d;\n" % len(cuadros))
    c.texto("const uint16_t entrada_ms_cuadro = %d;\n" % animacion["frame_ms"])
    c.texto("const int16_t entrada_y = %d;\n" % (cuadros[0]["y"] + y0))
    c.escribir()


def paseo():
    carpeta = os.path.join(FUENTES, "paseo")
    animacion = json.load(open(os.path.join(carpeta, "animacion.json")))
    assert animacion["frames"] == 2 * CUADRO_PASEO_MITAD
    f0 = leer_png(os.path.join(carpeta, "frame_000.png"))
    f5 = leer_png(os.path.join(carpeta, "frame_005.png"))
    mitad = leer_png(os.path.join(carpeta, "frame_%03d.png" % CUADRO_PASEO_MITAD))
    corrimiento = ANCHO // 2

    # Las nubes sin el auto: lo que el techo tapa en el cuadro 0 sale del
    # cuadro de la mitad, donde ese pedazo del cielo está corrido medio ancho
    nubes = []
    for y in range(ALTO_NUBES):
        fila = []
        for u in range(ANCHO):
            if X_TECHO[0] <= u <= X_TECHO[1]:
                x = (u - corrimiento) % ANCHO
                assert not (X_TECHO[0] <= x <= X_TECHO[1]), "en la mitad, ese pedazo no puede estar atrás del auto"
                fila.append(mitad[y][x])
            else:
                fila.append(f0[y][u])
        nubes.append(fila)

    # El techo: lo que en el cuadro 0 no son las nubes, con los huecos chicos rellenos
    techo = set()
    for y in range(ALTO_NUBES):
        xs = [x for x in range(X_TECHO[0], X_TECHO[1] + 1) if distinto(f0[y][x], nubes[y][x], TOLERANCIA_TECHO)]
        for a, b in zip(xs, xs[1:]):
            if b - a <= HUECO_TECHO + 1:
                techo.update((x, y) for x in range(a, b))
        techo.update((x, y) for x in xs)
    tx0, ty0, tx1, _ = caja(techo)
    assert X_TECHO[0] < tx0 and tx1 < X_TECHO[1], "el techo tiene que quedar adentro de X_TECHO"
    techo_img = [[p[:3] + ((255,) if (tx0 + i, ty0 + j) in techo else (0,)) for i, p in enumerate(fila)]
                 for j, fila in enumerate(recortar(f0, tx0, ty0, tx1 - tx0 + 1, ALTO_NUBES - ty0))]

    # Las llantas: lo que cambia entre el cuadro 0 y el 5, fuera de las rayas
    diferencias = [(x, y) for y in range(ALTO_NUBES, Y_RAYAS) for x in range(ANCHO)
                   if distinto(f0[y][x], f5[y][x], 0)]
    elipses = elipses_de(diferencias)

    # El camino, desde Y_RAYAS: un color de fondo y rayas de otro, iguales
    camino = f0[ALTO - 1][0]
    con_raya = [(x, y) for y in range(Y_RAYAS, ALTO) for x in range(ANCHO) if f0[y][x] != camino]
    raya = f0[con_raya[0][1]][con_raya[0][0]]
    assert all(f0[y][x] == raya for x, y in con_raya), "abajo hay algo más que el camino y las rayas"
    x_raya, y_raya = con_raya[0]
    fila = f0[y_raya]
    largo = next(x for x in range(x_raya, ANCHO) if fila[x] != raya) - x_raya
    periodo = next(x for x in range(x_raya + largo, ANCHO) if fila[x] == raya) - x_raya
    alto_raya = len({y for _, y in con_raya})
    assert ANCHO % periodo == 0, "las rayas tienen que cerrar la vuelta"

    c = Archivo("paseo.c", "fuentes/paseo")
    c.imagen("paseo_nubes", nubes, False)
    c.imagen("paseo_fondo", f0[ALTO_NUBES:Y_RAYAS], False)
    c.imagen("paseo_techo", techo_img, True)
    c.llantas("paseo_llantas", f0, elipses)
    campos = [
        ("duracion_ms", animacion["duration_ms"]),
        ("ms_cuadro", animacion["frame_ms"]),
        ("grados_por_segundo", GRADOS_POR_CUADRO_PASEO * 1000 // animacion["frame_ms"]),
        ("y_fondo", ALTO_NUBES),
        ("techo_x", tx0),
        ("techo_y", ty0),
        ("y_camino", Y_RAYAS),
        ("color_camino", color_lvgl(camino)),
        ("x_raya", x_raya),
        ("y_raya", y_raya),
        ("alto_raya", alto_raya),
        ("largo_raya", largo),
        ("periodo_raya", periodo),
        ("color_raya", color_lvgl(raya)),
    ]
    c.texto("\nconst DatosPaseo paseo = {\n%s\n};\n" % "\n".join("    .%s = %s," % campo for campo in campos))
    c.escribir()


if __name__ == "__main__":
    os.makedirs(SALIDA, exist_ok=True)
    entrada()
    paseo()
