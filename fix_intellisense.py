# Arduino-ESP32 3.x pasa los includes del framework (FreeRTOS, esp_*, etc.)
# con "-iprefix <dir>" + "@flags/includes", que el IntelliSense de VS Code no
# entiende. Solo al generar la config del IDE, los expandimos a rutas normales.
# No afecta la compilacion.
import os

Import("env", "projenv")

if env.IsIntegrationDump():
    flags = [str(f) for f in env.Flatten(env.get("CCFLAGS", []))]
    prefix = None
    paths = []
    for i, flag in enumerate(flags):
        if flag == "-iprefix" and i + 1 < len(flags):
            prefix = env.subst(flags[i + 1])
        elif flag.startswith("@") and prefix:
            with open(env.subst(flag[1:])) as fp:
                tokens = fp.read().split()
            for j, tok in enumerate(tokens[:-1]):
                if tok == "-iwithprefixbefore":
                    paths.append(os.path.join(prefix, tokens[j + 1]))
    projenv.Append(CPPPATH=paths)
