#!/usr/bin/env python3
"""
Compila y corre en la PC el test del programa de ejemplo, sin hardware.
Requiere gcc (en Windows: MinGW-w64 o MSYS2).

    python test_pc/correr_test.py

Valido para programas/ejemplo.st. Si cambias el programa generado,
adapta los chequeos de test_ejemplo.c.
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
INC = RAIZ / "src" / "Core" / "Inc"

with tempfile.TemporaryDirectory() as tmp:
    fake = Path(tmp) / "fake"
    shutil.copytree(RAIZ / "test_pc" / "fake", fake)
    # induSPI.h incluye "main.h": se copia junto a los headers falsos del HAL
    for h in ("induSPI.h", "openplc_glue.h"):
        shutil.copy(INC / h, fake / h)
    exe = Path(tmp) / "test_ejemplo"
    gen = RAIZ / "src" / "OpenPLC" / "generated"
    cmd = [
        "gcc", "-I", str(fake),
        "-I", str(RAIZ / "src" / "OpenPLC" / "lib"), "-I", str(gen),
        str(RAIZ / "test_pc" / "test_ejemplo.c"),
        str(RAIZ / "src" / "Core" / "Src" / "openplc_glue.c"),
        str(gen / "Config0.c"), str(gen / "Res0.c"),
        "-lm", "-o", str(exe),
    ]
    if subprocess.run(cmd).returncode != 0:
        sys.exit("Fallo la compilacion del test.")
    sys.exit(subprocess.run([str(exe)]).returncode)
