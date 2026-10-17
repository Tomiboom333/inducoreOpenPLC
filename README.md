# Modulo_CPU_OpenPLC

Firmware del módulo CPU de **InduCore** que ejecuta programas IEC 61131-3
(Ladder, ST, FBD, IL, SFC) creados con **OpenPLC Editor**.

Parte del código de [Modulo_CPU](https://github.com/Tomiboom333/Modulo_CPU):
el bus SPI con los módulos de entradas y salidas (`induSPI.c`) y las funciones
Modbus se mantienen. Lo nuevo es que la lógica ya no se escribe en C (`algo()`),
sino que la genera el compilador IEC de OpenPLC (MatIEC).

## Flujo de trabajo

```
OpenPLC Editor  ──►  programa.st  ──►  tools/generar_programa.py  ──►  PlatformIO  ──►  STM32
 (Ladder, ST...)      (exportado)       (iec2c → código C)             (compilar y flashear)
```

1. **Crear el programa en OpenPLC Editor.** En la tabla de variables, asignale
   a cada entrada/salida su dirección (`%IX0.0`, `%QX0.2`, `%IW0`, ...) según el
   mapa de abajo. La TASK define el tiempo de ciclo (por ejemplo `T#20ms`).
2. **Exportar el programa como `.st`** con la opción del editor que genera el
   programa para el OpenPLC Runtime. Guardalo, por ejemplo, en `programas/`.
3. **Generar el código C:**
   ```
   python tools/generar_programa.py programas/mi_programa.st
   ```
   El script busca `iec2c` en la instalación de OpenPLC Editor. Si no lo
   encuentra, pasale la ruta:
   ```
   python tools/generar_programa.py programas/mi_programa.st --iec2c "C:/ruta/a/iec2c.exe"
   ```
   (o definí la variable de entorno `IEC2C`). El resultado queda en
   `src/OpenPLC/generated/`.
4. **Compilar y flashear** con PlatformIO, como siempre (`pio run -t upload`
   o el botón Upload de VS Code).

## Mapa de direcciones

| Dirección IEC | Tipo | Hardware | Variable en `estAct` |
|---|---|---|---|
| `%IX<m>.<e>` | BOOL | Entrada digital `e` (0–7) del módulo de entradas `m` (0–4) | `modId[m][e]` |
| `%IW<n>` | INT | Entrada analógica: módulo `n/2`, canal `n%2` (0–255) | `modIa[n/2][n%2]` |
| `%QX<m>.<s>` | BOOL | Salida digital `s` (0–7) del módulo de salidas `m` (0–4) | `modOd[m][SAL_(s+1)]` |
| `%QW<n>` | INT | Salida analógica: módulo `n/2`, canal `n%2` (se recorta a 0–255) | `modOa[n/2][n%2]` |

Ejemplos:

- `%IX0.0` → entrada 1 del primer módulo de entradas.
- `%QX0.2` → salida 3 del primer módulo de salidas.
- `%IW1` → analógica 2 del primer módulo de entradas.
- `%QW2` → analógica 1 del segundo módulo de salidas.

Notas:

- El número de módulo es el **orden en que `moduleDet()` los detecta**,
  contando por separado los de entradas y los de salidas.
- `%QX<m>.0` corresponde a la salida física 1 respetando el orden de bits de
  `induSPI.h` (`SAL_1` = bit 3, `SAL_2` = bit 2, ...), igual que con `digWrite()`.
- Las analógicas son de 8 bits en el hardware actual (0–255).
- Las direcciones fuera de rango (por ejemplo `%IX7.0`) se ignoran.
- Las variables de memoria (`%MW`, `%MX`, ...) funcionan como variables
  internas del programa.

## Tiempo de ciclo

Cada ciclo: espera el período → lee entradas por SPI → ejecuta el programa IEC
→ escribe salidas por SPI. El período sale del `INTERVAL` de la TASK del
programa y se controla con `HAL_GetTick()` (1 ms de resolución), por lo que ya
no se usa el `HAL_Delay(10)` fijo del firmware original.

Variables de diagnóstico (se pueden ver con el debugger):

- `plc_ciclos`: ciclos ejecutados.
- `plc_ciclos_excedidos`: ciclos que tardaron más que el período (si crece,
  aumentá el `INTERVAL`).
- `plc_periodo_ms`: período en ms.

## Estructura

```
programas/ejemplo.st          Programa de ejemplo (equivalente a algo() + parpadeo con TON)
src/Core/Src/openplc_glue.c   Unión entre el programa IEC y estAct  ← nuevo
src/Core/Src/main.c           Bucle principal                      ← reescrito
src/Core/Src/induSPI.c        Bus SPI con los módulos (sin HAL_Delay(10) al final del ciclo)
src/OpenPLC/generated/        Código generado por iec2c (no editar a mano)
src/OpenPLC/lib/              Librería IEC de MatIEC (LGPL)
tools/generar_programa.py     .st → código C
tools/matiec_lib/             Librería estándar IEC para iec2c (si tu instalación no la trae)
test_pc/                      Test del ejemplo en la PC, sin hardware
```

## Uso de memoria (programa de ejemplo)

Compilado para STM32F103C8 con `arm-none-eabi-gcc 13.3`, `-Os`:

| | Usado | Disponible |
|---|---|---|
| Flash | ≈ 10 KB | 64 KB |
| RAM (estática) | ≈ 2,7 KB | 20 KB |

Programas más grandes ocupan más, pero hay bastante margen. Si algún día no
alcanza, el paso natural es un STM32F401/F411 (Black Pill).

## Test en la PC

Compila el programa de ejemplo junto con `openplc_glue.c` para la PC, simulando
`estAct` y el reloj, y verifica las salidas y los tiempos del parpadeo
(requiere `gcc`; en Windows, MinGW-w64 o MSYS2):

```
python test_pc/correr_test.py
```

## Pendiente de probar en hardware

Esto se compiló y se probó en simulación, **no en las placas reales**. Al
flashear, conviene verificar:

1. Que los módulos se detecten igual que antes.
2. Que `%QX0.0` encienda la salida física 1 (orden de bits `SAL_x`).
3. Que el parpadeo del ejemplo sea de ~0,5 s y que `plc_ciclos_excedidos` no crezca.
4. Que la comunicación SPI siga estable sin el `HAL_Delay(10)` (ahora la pausa
   entre ciclos es el resto del período de 20 ms).

## Cambios respecto a Modulo_CPU

- `main.c`: ejecuta el programa IEC en lugar de `algo()`.
- `induSPI.c`: se quitó el `HAL_Delay(10)` de `plc_run_cycle()`.
- `modbus_crc.h`: se agregó `#include <stdint.h>` (faltaba para compilar con GCC).
- `src/OpenPLC/lib/iec_std_lib.h`: se agregó `#include <time.h>` (necesario con newlib).
- `platformio.ini`: rutas de include de OpenPLC y exclusión de `POUS.c`.

## Licencias

La librería IEC de `src/OpenPLC/lib` y `tools/matiec_lib` pertenece al proyecto
MatIEC (usado por OpenPLC y Beremiz) y se distribuye bajo LGPL
(ver `COPYING.LESSER`).
