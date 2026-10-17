#!/usr/bin/env python3
"""
Convierte un programa IEC 61131-3 (.st exportado desde OpenPLC Editor)
en codigo C para el modulo CPU de InduCore.

Uso:
    python tools/generar_programa.py programas/ejemplo.st
    python tools/generar_programa.py mi_programa.st --iec2c "C:/ruta/a/iec2c.exe"

El resultado se escribe en src/OpenPLC/generated. Despues se compila y
flashea normalmente con PlatformIO.

Para encontrar iec2c (el compilador IEC de OpenPLC / MatIEC) se prueba, en orden:
  1. el argumento --iec2c
  2. la variable de entorno IEC2C
  3. iec2c en el PATH
  4. las carpetas habituales de instalacion de OpenPLC Editor
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
DESTINO = RAIZ / "src" / "OpenPLC" / "generated"
LIB_INCLUIDA = RAIZ / "tools" / "matiec_lib"

# Opciones iguales a las que usa el runtime de OpenPLC
OPCIONES = ["-f", "-l", "-p", "-r", "-R", "-a"]

ARCHIVOS_ESPERADOS = ["Config0.c", "Res0.c", "POUS.c", "POUS.h", "LOCATED_VARIABLES.h"]


def candidatos_iec2c():
    exe = "iec2c.exe" if os.name == "nt" else "iec2c"
    env = os.environ.get("IEC2C")
    if env:
        yield Path(env)
    en_path = shutil.which("iec2c")
    if en_path:
        yield Path(en_path)
    home = Path.home()
    bases = [
        home / "OpenPLC_Editor",
        home / "OpenPLC Editor",
        Path(os.environ.get("LOCALAPPDATA", "")) / "OpenPLC_Editor",
        Path(os.environ.get("PROGRAMFILES", "")) / "OpenPLC_Editor",
        Path("/opt/OpenPLC_Editor"),
        home / "OpenPLC_Editor" / "editor",
    ]
    for base in bases:
        for sub in ("matiec", "editor/matiec", "", "bin"):
            yield base / sub / exe


def buscar_iec2c(explicito):
    if explicito:
        p = Path(explicito)
        if p.is_file():
            return p
        sys.exit(f"No encuentro iec2c en: {p}")
    for p in candidatos_iec2c():
        if str(p) and p.is_file():
            return p
    sys.exit(
        "No encontre iec2c.\n"
        "Instala OpenPLC Editor y pasa la ruta con --iec2c, por ejemplo:\n"
        '  python tools/generar_programa.py programa.st --iec2c "C:/Users/<vos>/OpenPLC_Editor/matiec/iec2c.exe"\n'
        "o defini la variable de entorno IEC2C."
    )


def buscar_lib(iec2c, explicita):
    if explicita:
        return Path(explicita)
    junto = iec2c.parent / "lib"
    if (junto / "ieclib.txt").is_file():
        return junto
    return LIB_INCLUIDA


def main():
    ap = argparse.ArgumentParser(description="Genera el codigo C del programa IEC para InduCore")
    ap.add_argument("programa", help="archivo .st exportado desde OpenPLC Editor")
    ap.add_argument("--iec2c", help="ruta al ejecutable iec2c")
    ap.add_argument("--lib", help="carpeta con ieclib.txt (por defecto la de iec2c o tools/matiec_lib)")
    args = ap.parse_args()

    programa = Path(args.programa).resolve()
    if not programa.is_file():
        sys.exit(f"No existe el archivo: {programa}")

    iec2c = buscar_iec2c(args.iec2c)
    lib = buscar_lib(iec2c, args.lib)

    # Se limpia la carpeta para no mezclar archivos de programas anteriores
    if DESTINO.exists():
        for f in DESTINO.iterdir():
            if f.is_file():
                f.unlink()
    DESTINO.mkdir(parents=True, exist_ok=True)

    cmd = [str(iec2c), *OPCIONES, "-I", str(lib), "-T", str(DESTINO), str(programa)]
    print("Ejecutando:", " ".join(cmd))
    r = subprocess.run(cmd)
    if r.returncode != 0:
        sys.exit("iec2c reporto errores (ver arriba). No se genero el programa.")

    faltan = [n for n in ARCHIVOS_ESPERADOS if not (DESTINO / n).is_file()]
    if faltan:
        sys.exit("Faltan archivos generados: " + ", ".join(faltan))

    shutil.copy(programa, DESTINO / "programa.st")
    print(f"\nListo: codigo generado en {DESTINO.relative_to(RAIZ)}")
    print("Ahora compila y flashea con PlatformIO (pio run -t upload).")


if __name__ == "__main__":
    main()
