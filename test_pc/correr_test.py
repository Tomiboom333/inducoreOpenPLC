#!/usr/bin/env python3
"""
Compila y corre en la PC los tests, sin hardware.
Requiere gcc (en Windows: MinGW-w64 o MSYS2).

    python test_pc/correr_test.py

- test_protocolo: tramas del backplane (induproto.h): CRC-8, 12 bits, rechazo de errores.
- test_backplane: induSPI.c con modulos de E/S simulados: deteccion, lectura,
  escritura con ACK, desconexion, reconexion y modulos nuevos.
- test_esclavo: Modbus esclavo y su mapa de registros.
- test_ejemplo: programa de ejemplo (programas/ejemplo.st) + openplc_glue.c.
  Valido para el ejemplo; si cambias el programa generado, adapta los chequeos.
- test_modbus: maestro Modbus RTU (envio, CRC, respuestas, errores y timeout).
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

RAIZ = Path(__file__).resolve().parent.parent
INC = RAIZ / "src" / "Core" / "Inc"
SRC = RAIZ / "src" / "Core" / "Src"
GEN = RAIZ / "src" / "OpenPLC" / "generated"
LIB = RAIZ / "src" / "OpenPLC" / "lib"
NO_COPIAR = {"main.h", "stm32f1xx_hal_conf.h", "stm32f1xx_it.h"}

TESTS = {
    "test_protocolo": [],
    "test_backplane": [SRC / "induSPI.c"],
    "test_ejemplo": [SRC / "openplc_glue.c", GEN / "Config0.c", GEN / "Res0.c"],
    "test_modbus": [SRC / "mbus_funcs.c", SRC / "mbus_parser.c", SRC / "modbus_crc.c"],
    "test_esclavo": [SRC / "mbus_esclavo.c", SRC / "mbus_mapa.c", SRC / "mbus_parser.c", SRC / "modbus_crc.c"],
}

fallas = 0
with tempfile.TemporaryDirectory() as tmp:
    fake = Path(tmp) / "fake"
    shutil.copytree(RAIZ / "test_pc" / "fake", fake)
    # Los headers del proyecto incluyen "main.h": se copian junto al HAL falso
    for h in INC.glob("*.h"):
        if h.name not in NO_COPIAR:
            shutil.copy(h, fake / h.name)

    for nombre, fuentes in TESTS.items():
        print(f"===== {nombre} =====")
        exe = Path(tmp) / nombre
        cmd = ["gcc", "-I", str(fake), "-I", str(LIB), "-I", str(GEN),
               str(RAIZ / "test_pc" / f"{nombre}.c"), *map(str, fuentes),
               "-lm", "-o", str(exe)]
        if subprocess.run(cmd).returncode != 0:
            print("Fallo la compilacion del test.")
            fallas += 1
            continue
        if subprocess.run([str(exe)]).returncode != 0:
            fallas += 1
        print()

print("RESULTADO:", "HAY FALLAS" if fallas else "TODOS LOS TESTS OK")
sys.exit(1 if fallas else 0)
