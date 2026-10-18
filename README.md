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
| `%IW<n>` | INT | Entrada analógica: módulo `n/2`, canal `n%2` (0–4095) | `modIa[n/2][n%2]` |
| `%QX<m>.<s>` | BOOL | Salida digital `s` (0–7) del módulo de salidas `m` (0–4) | `modOd[m][SAL_(s+1)]` |
| `%QW<n>` | INT | Salida analógica: módulo `n/2`, canal `n%2` (se recorta a 0–4095) | `modOa[n/2][n%2]` |
| `%MW<n>` | INT | Memoria `n` (0–63), compartida con el Modbus esclavo (holding 1024+`n`) | `plc_mw[n]` |

Ejemplos:

- `%IX0.0` → entrada 1 del primer módulo de entradas.
- `%QX0.2` → salida 3 del primer módulo de salidas.
- `%IW1` → analógica 2 del primer módulo de entradas.
- `%QW2` → analógica 1 del segundo módulo de salidas.

Notas:

- El número de módulo es el **orden físico en el riel** (de izquierda a
  derecha, empezando por el slot pegado a la CPU), contando por separado los de
  entradas y los de salidas. Ver `docs/PROTOCOLO.md`.
- `%QX<m>.0` corresponde a la salida física 1 respetando el orden de bits de
  `induSPI.h` (`SAL_1` = bit 3, `SAL_2` = bit 2, ...), igual que con `digWrite()`.
- Las analógicas son de **12 bits** (0–4095). El canal 0 de cada módulo es
  `AN_1` (bornera InAn1 / AO0) y el canal 1 es `AN_2` (InAn2 / AO1).
- Las direcciones fuera de rango (por ejemplo `%IX7.0`) se ignoran.
- `%MW0`–`%MW63` se pueden leer y escribir desde un SCADA o HMI por Modbus.
  Las demás variables de memoria funcionan como variables internas del programa.

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

## Backplane SPI y seguridad

El protocolo completo está en [`docs/PROTOCOLO.md`](docs/PROTOCOLO.md). En resumen:

- Todas las tramas llevan **CRC-8**, y las escrituras se confirman con `ACK`.
  Una trama con error se descarta y nunca cambia una salida.
- Si un módulo falla 3 veces seguidas pasa a *offline*; sus entradas quedan en 0.
  Cada 1 s la CPU busca módulos offline o nuevos, así que **un módulo que se
  reinicia o se enchufa después vuelve solo**. Desde la lógica:
  `modEntradasOk(MOD_IN_1)`, `modSalidasOk(MOD_OUT_1)`.
- **Watchdog** en la CPU (~1 s) y en los módulos (~250 ms).
- El módulo de salidas **apaga todo si no recibe escrituras válidas durante
  500 ms** (CPU colgada, reiniciada o desconectada).

Requiere las ramas `claude` de **Modulo_IN y Modulo_OUT**: el protocolo v2
no es compatible con el firmware anterior de los módulos.

## Modbus RTU esclavo (SCADA / HMI)

El PLC responde como **esclavo Modbus RTU** para que un SCADA, un HMI o una PC
lean y escriban sus entradas, salidas y memoria. Por defecto usa el **USB** de
la CPU (USART1 → CH340G), a 115200 8N1, dirección 1. El RS-485 queda libre para
el maestro. Para usar el RS-485 como esclavo, compilar con
`-DMBS_PUERTO=MBS_PUERTO_RS485` (el maestro queda deshabilitado).

| Tabla | Funciones | Direcciones | Contenido |
|---|---|---|---|
| Coils | 01, 05, 0F | 0–39 | `%QX m.s` → `8·m + s` (salidas digitales) |
| Discrete inputs | 02 | 0–39 | `%IX m.e` → `8·m + e` (entradas digitales) |
| Input registers | 04 | 0–9 | `%IW n` (entradas analógicas, 0–4095) |
| | | 100–111 | diagnóstico (abajo) |
| Holding registers | 03, 06, 10 | 0–9 | `%QW n` (salidas analógicas, 0–4095) |
| | | 1024–1087 | `%MW 0–63` |

Diagnóstico (input registers): 100 versión del protocolo · 101/102 cantidad de
módulos de entradas/salidas · 103/104 máscara de módulos que responden (bit *i* =
`MOD_x_(i+1)`) · 105 período de ciclo (ms) · 106/107 ciclos (parte baja/alta) ·
108 ciclos excedidos · 109 lecturas SPI con error · 110 escrituras SPI con error ·
111 desconexiones.

Direcciones o valores inválidos devuelven las excepciones estándar (01 función,
02 dirección, 03 valor). Las salidas que escribe el programa se pisan en cada
ciclo: escribir por Modbus solo las que el programa no usa, o usar `%MW` como
consignas que el programa lee.

## Maestro Modbus RTU (RS-485)

`mbus_funcs.c` funciona como **maestro Modbus RTU** por USART3 (115200 8N1,
`TX_EN` en PA8), **sin bloquear el ciclo del PLC**:

1. Una función de petición arma la trama, le agrega el **CRC-16**, la envía y
   deja la recepción armada por interrupción (fin de trama por línea inactiva).
2. `mb_procesar()` valida la respuesta (**CRC**, esclavo, función, longitud y,
   en escrituras, el eco) y la **guarda en `mbRespuesta`**. Si no llega nada en
   `mb_timeout_ms` (100 ms por defecto) la marca como timeout. `main.c` la llama
   mientras el CPU espera el próximo ciclo, así la respuesta se procesa enseguida.
3. Si no hubo respuesta, llegó con CRC incorrecto o mal formada, **reenvía la
   petición** hasta `mb_reintentos` veces (2 por defecto). Las excepciones del
   esclavo no se reintentan.
4. Al terminar, bien o mal, se llama al callback registrado con `mb_set_callback()`.

| Función | Código | Resultado en `mbRespuesta` |
|---|---|---|
| `readCoils(esclavo, dir, cant)` | 01 | `mb_bit(&mbRespuesta, i)` |
| `readInputContacts(esclavo, dir, cant)` | 02 | `mb_bit(&mbRespuesta, i)` |
| `readHoldingRegisters(esclavo, dir, cant)` | 03 | `registros[i]` |
| `readAnalogInputs(esclavo, dir, cant)` | 04 | `registros[i]` |
| `writeSingleCoil(esclavo, dir, valor)` | 05 | se verifica el eco |
| `writeSingleRegister(esclavo, dir, valor)` | 06 | se verifica el eco |
| `writeMultipleCoils(esclavo, dir, valores, cant)` | 0F | se verifica dirección y cantidad |
| `writeMultipleRegisters(esclavo, dir, valores, cant)` | 10 | se verifica dirección y cantidad |

Cada petición devuelve `MB_PENDIENTE` si se envió, o un error inmediato
(`MB_ERR_OCUPADO` si ya hay una en curso, `MB_ERR_PARAM` si los parámetros no
son válidos). El estado final queda en `mbRespuesta.estado`:

| Estado | Significado |
|---|---|
| `MB_OK` | Respuesta válida y guardada |
| `MB_ERR_TIMEOUT` | El esclavo no respondió a tiempo |
| `MB_ERR_CRC` | La respuesta llegó con el CRC incorrecto |
| `MB_ERR_EXCEPCION` | El esclavo respondió con una excepción (código en `mbRespuesta.excepcion`) |
| `MB_ERR_TRAMA` | Respuesta mal formada, de otro esclavo o con ruido en la línea |
| `MB_ERR_TX` | Falló la transmisión |
| `MB_ERR_PUERTO` | El RS-485 está configurado como esclavo |

Además se guarda la trama cruda (`mbRespuesta.trama`, `largo_trama`, y una copia
en `Data[]` con `uart_rx_flag = 1`) y hay contadores en `mbEstadisticas`
(peticiones, ok, timeouts, errores de CRC, excepciones...). Con el esclavo 0
(broadcast) las escrituras no esperan respuesta.

Ejemplo de uso con callback (está en `main.c`; se activa con `#define EJEMPLO_MODBUS 1`):

```c
void respuesta_modbus(const mb_respuesta_t *r) {
  if (r->estado == MB_OK && r->funcion == MB_FC_LEER_HOLDING)
    memcpy(mis_registros, r->registros, 10 * sizeof(uint16_t));
}
...
mb_set_callback(respuesta_modbus);
if (!mb_ocupado()) readHoldingRegisters(1, 0, 10);
```

Para pruebas simples se puede esperar en forma bloqueante:

```c
readHoldingRegisters(1, 0, 10);
if (mb_esperar() == MB_OK) { /* usar mbRespuesta.registros */ }
```

## Estructura

```
programas/ejemplo.st          Programa de ejemplo (equivalente a algo() + parpadeo con TON)
src/Core/Src/openplc_glue.c   Unión entre el programa IEC y estAct  ← nuevo
src/Core/Src/main.c           Bucle principal                      ← reescrito
src/Core/Inc/induproto.h      Protocolo del backplane (igual en los tres firmwares)
src/Core/Src/induSPI.c        Backplane: detección, lectura, escritura, redetección, API
src/Core/Src/induHW.c         Reloj, GPIO, SPI, UART y watchdog
src/Core/Src/mbus_funcs.c     Maestro Modbus RTU: envío, recepción, timeout, reintentos
src/Core/Src/mbus_parser.c    Validación de respuestas Modbus (CRC, longitud, excepciones)
src/Core/Src/mbus_esclavo.c   Modbus esclavo: procesamiento de peticiones
src/Core/Src/mbus_mapa.c      Modbus esclavo: mapa de registros
src/Core/Src/mbus_esclavo_uart.c  Modbus esclavo: puerto serie (USB o RS-485)
docs/PROTOCOLO.md             Protocolo del backplane
src/OpenPLC/generated/        Código generado por iec2c (no editar a mano)
src/OpenPLC/lib/              Librería IEC de MatIEC (LGPL)
tools/generar_programa.py     .st → código C
tools/matiec_lib/             Librería estándar IEC para iec2c (si tu instalación no la trae)
test_pc/                      Tests en la PC, sin hardware
```

## Uso de memoria (programa de ejemplo)

Compilado para STM32F103C8 con `arm-none-eabi-gcc 13.3`, `-Os`:

| | Usado | Disponible |
|---|---|---|
| Flash | ≈ 15 KB | 64 KB |
| RAM (estática) | ≈ 4,5 KB | 20 KB |

(Incluye el maestro y el esclavo Modbus.)

Programas más grandes ocupan más, pero hay bastante margen. Si algún día no
alcanza, el paso natural es un STM32F401/F411 (Black Pill).

## Tests en la PC

Compilan el código para la PC simulando el hardware (requiere `gcc`; en
Windows, MinGW-w64 o MSYS2):

- `test_protocolo`: tramas del backplane (`induproto.h`): CRC-8 contra el
  vector de referencia, 12 bits, detección de todos los errores de 1 y 2 bits,
  rechazo de un slot vacío.
- `test_backplane`: `induSPI.c` con módulos de entradas y salidas simulados:
  orden de slots, lectura, escritura con ACK, tramas corruptas, desconexión,
  reconexión y módulos nuevos.
- `test_ejemplo`: el programa de ejemplo con `openplc_glue.c`, simulando `estAct`
  y el reloj; verifica salidas y tiempos del parpadeo.
- `test_modbus`: el maestro Modbus con una UART simulada; verifica el CRC (contra
  un cálculo bit a bit y la trama de referencia `01 03 00 00 00 0A C5 CD`),
  el manejo de `TX_EN`, respuestas válidas, CRC incorrecto, excepciones,
  timeout, esclavo equivocado, coils, ecos de escritura, broadcast y reintentos.
- `test_esclavo`: el Modbus esclavo y su mapa: todas las funciones,
  excepciones, CRC incorrecto, otra dirección y broadcast.

```
python test_pc/correr_test.py
```

## Pendiente de probar en hardware

Esto se compiló y se probó en simulación, **no en las placas reales**. Al
flashear, conviene verificar:

1. Que los módulos se detecten (`contModulosIn`, `contModulosOut`, `slots[]`) y
   que `MOD_IN_1` / `MOD_OUT_1` sean los módulos pegados a la CPU.
2. Que `%QX0.0` encienda la salida física 1 (orden de bits `SAL_x`).
3. Que el parpadeo del ejemplo sea de ~0,5 s y que `plc_ciclos_excedidos` no crezca.
4. Que la comunicación SPI siga estable sin el `HAL_Delay(10)` (ahora la pausa
   entre ciclos es el resto del período de 20 ms).
5. Modbus con un esclavo real (`EJEMPLO_MODBUS 1`): que `mbEstadisticas.ok`
   suba y `registros_esclavo1` tenga los valores del esclavo. Si aparecen
   timeouts, revisar cableado A/B, terminación, dirección del esclavo y
   velocidad; si aparecen errores de CRC o de trama, revisar ruido y
   polarización de la línea RS-485.
6. Backplane: que `spiEstadisticas.lecturasError` y `escriturasError` no crezcan.
   Sacar un módulo con todo andando: tiene que pasar a offline y volver solo al
   enchufarlo (`desconexiones` / `reconexiones`).
7. Módulo de salidas: con salidas encendidas, desconectar o resetear la CPU;
   en ~0,5 s tienen que apagarse todas.
8. Salidas analógicas: confirmar que 0 = apagada y 4095 = 100 %. Si quedan
   invertidas, poner `AO_LOGICA_INVERTIDA 0` en Modulo_OUT.
9. Entradas analógicas: confirmar que `AN_1` lee la bornera InAn1.
10. Modbus esclavo: conectarse por USB con un cliente Modbus (por ejemplo
    QModMaster o mbpoll), leer los input registers 100–111 y escribir un `%MW`.

## Cambios respecto a Modulo_CPU

- `main.c`: ejecuta el programa IEC en lugar de `algo()`.
- `induSPI.c`: se quitó el `HAL_Delay(10)` de `plc_run_cycle()`.
- `modbus_crc.h`: se agregó `#include <stdint.h>` (faltaba para compilar con GCC).
- `mbus_funcs.c`: reescrito. Ahora gestiona y guarda la respuesta, verifica el
  CRC y tiene timeout. Además se corrigió que la recepción empezaba recién
  5 ms después de transmitir (`HAL_Delay(5)`), lo que hacía perder la
  respuesta, y que las escrituras no esperaban la respuesta del esclavo.
  Las funciones conservan los nombres y parámetros; ahora devuelven `mb_estado_t`.
- `src/OpenPLC/lib/iec_std_lib.h`: se agregó `#include <time.h>` (necesario con newlib).
- `platformio.ini`: rutas de include de OpenPLC y exclusión de `POUS.c`.
- Backplane v2 (`induSPI.c`, `induproto.h`): CRC-8, analógicas de 12 bits,
  ACK en las escrituras, orden físico de slots, redetección y watchdog.
  Incluye las mejoras de la rama `claude` de Modulo_CPU (pausa de 100 µs entre
  comando y datos, SPI a 2,25 MHz y verificación de lecturas).
- `induHW.c`: la inicialización de hardware pasó a su propio archivo y se
  agregaron USART1 y el watchdog.
- Modbus esclavo nuevo y reintentos en el maestro.
- Se eliminó la carpeta `EWARM` (IAR): el proyecto se compila solo con PlatformIO.

## Licencias

La librería IEC de `src/OpenPLC/lib` y `tools/matiec_lib` pertenece al proyecto
MatIEC (usado por OpenPLC y Beremiz) y se distribuye bajo LGPL
(ver `COPYING.LESSER`).
