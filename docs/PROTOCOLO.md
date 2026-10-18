# Protocolo del backplane InduCore (versión 2)

Comunicación entre el módulo CPU y los módulos de entradas y salidas por el
bus SPI del riel. El formato está implementado en `induproto.h`, que es **el
mismo archivo** en los tres firmwares (inducoreOpenPLC, Modulo_IN y
Modulo_OUT). Si se cambia en uno, hay que copiarlo a los otros dos y subir
`PROTO_VERSION`.

## Capa física

| | |
|---|---|
| Bus | SPI1, modo 0 (CPOL = 0, CPHA = 0), MSB primero, 8 bits |
| Velocidad | 2,25 MHz (72 MHz / 32) |
| Maestro | CPU. Un chip select por slot (PA0–PA4) |
| Esclavos | Módulos de E/S. Los 5 pines SS del conector están unidos al PA4 del módulo, así que cualquier módulo funciona en cualquier slot |

### Slots y chip select

Por el espejado del conector, el nombre de los chip select de la CPU está
invertido respecto del riel. El firmware numera los slots en orden físico:

| Slot (firmware) | Posición en el riel | Línea del riel | Pin de la CPU |
|---|---|---|---|
| 0 | M1 (primero a la derecha de la CPU) | SS0 | PA4 (`SPI1_NSS5`) |
| 1 | M2 | SS1 | PA3 (`SPI1_NSS4`) |
| 2 | ME1 | SS2 | PA2 (`SPI1_NSS3`) |
| 3 | ME2 | SS3 | PA1 (`SPI1_NSS2`) |
| 4 | ME3 (último) | SS4 | PA0 (`SPI1_NSS1`) |

Los índices `MOD_IN_x` y `MOD_OUT_x` se asignan en ese orden al arrancar:
`MOD_IN_1` es el primer módulo de entradas a la derecha de la CPU.

## Transacción

Los esclavos atienden el SPI por *polling*, así que el maestro deja una pausa
después de cada comando para que el esclavo lo procese y cargue la respuesta:

```
NSS ‾‾\_________________________________________________/‾‾
       [comando]  ← pausa ≥ 100 µs →  [datos]  (pausa) [ACK]
```

`PROTO_PAUSA_US` = 100 µs. Al subir NSS, el esclavo descarta cualquier byte a
medias y queda listo para la próxima trama, así un error nunca se arrastra.

## Tramas

Todos los valores analógicos son de **12 bits** (0–4095), byte alto primero.
El **CRC-8** (polinomio 0x07, valor inicial 0x00, sin reflexión: CRC-8/SMBUS)
se calcula sobre el comando y todos los bytes de datos.

### Detección (`0x48`)

| Dirección | Bytes |
|---|---|
| CPU → módulo | `0x48` |
| módulo → CPU | `ID` · `VERSION` · `CRC` |

`ID` = `0x12` (entradas) o `0x02` (salidas). `VERSION` = `PROTO_VERSION` (2).
La CPU ignora respuestas con CRC incorrecto, versión distinta o ID desconocido,
por eso un slot vacío (MISO en `0x00` o `0xFF`) nunca se toma como módulo.

### Lectura de entradas (`0x01`)

| Dirección | Bytes |
|---|---|
| CPU → módulo | `0x01` |
| módulo → CPU | `DIG` · `AN1 H` · `AN1 L` · `AN2 H` · `AN2 L` · `CRC` |

`DIG`: bit *i* = entrada digital *i*+1. `AN1` = bornera InAn1 (PB1),
`AN2` = InAn2 (PB0).

### Escritura de salidas (`0x02`)

| Dirección | Bytes |
|---|---|
| CPU → módulo | `0x02` · (pausa) · `DIG` · `AO1 H` · `AO1 L` · `AO2 H` · `AO2 L` · `CRC` |
| módulo → CPU | (pausa) · `ACK` (`0x06`) o `NAK` (`0x15`) |

`DIG`: bit *n* = salida física según `SAL_x` (`SAL_1` = bit 3, `SAL_2` = bit 2,
`SAL_3` = bit 1, `SAL_4` = bit 0, `SAL_5..8` = bits 4..7). `AO1` = bornera AO0
(PA10), `AO2` = AO1 (PA11). El módulo aplica las salidas **solo si el CRC es
correcto** y responde `ACK`; si no, responde `NAK` y no cambia nada.

## Tiempos de un ciclo

| Transacción | Bytes de reloj | Duración aprox. |
|---|---|---|
| Detección | 4 | ~115 µs |
| Lectura | 7 | ~125 µs |
| Escritura | 8 | ~230 µs (dos pausas) |

Con dos módulos de entradas y dos de salidas, el backplane ocupa menos de
1 ms por ciclo (el ciclo por defecto es de 20 ms).

## Fallas y recuperación

**En la CPU** (`induSPI.c`):

- Una lectura con CRC incorrecto se descarta y se conserva la última lectura
  válida.
- Una escritura sin `ACK` cuenta como falla.
- Con `SPI_FALLAS_OFFLINE` (3) fallas seguidas, el módulo pasa a *offline*.
  Si es de entradas, sus entradas se ponen en 0 (`ENTRADAS_EN_CERO_SI_FALLA`).
- Cada `SPI_REDETECCION_MS` (1 s) la CPU vuelve a detectar los slots vacíos y
  los módulos offline:
  - Si un módulo offline responde, vuelve a *online* con el mismo índice.
  - Si aparece un módulo en un slot vacío, recibe el **próximo índice libre**
    de su tipo, sin mover los demás. Para reordenar, reiniciar la CPU.
  - Si en un slot aparece un módulo de otro tipo, se ignora hasta reiniciar.
- La lógica puede consultar el estado con `modEntradasOk(n)` y
  `modSalidasOk(n)`. Por Modbus, en los input registers 103 y 104.

**En el módulo de salidas** (`Modulo_OUT`):

- Si no recibe una escritura válida en `SALIDAS_TIMEOUT_MS` (500 ms), apaga
  todas las salidas. Esto cubre una CPU colgada, reiniciada o desconectada.
- Al arrancar, PA10/PA11 se ponen en el nivel que apaga las AO antes de
  iniciar el PWM.

**Watchdog (IWDG) en los tres módulos:**

| Módulo | Tiempo nominal | Rango según el LSI |
|---|---|---|
| CPU | 1 s | 0,66–1,5 s |
| Entradas y salidas | 250 ms | 166–375 ms |

En los tres, el watchdog se detiene mientras el depurador tiene el micro en
pausa.

> **Limitación de hardware:** con el micro en reset, las salidas AO quedan
> encendidas por la lógica invertida del optoacoplador. El firmware lo
> minimiza (estado seguro al arrancar y por timeout), pero el reset en sí
> solo se resuelve en la placa.

## Contadores de diagnóstico

| Firmware | Variable | Qué cuenta |
|---|---|---|
| CPU | `spiEstadisticas` | lecturas y escrituras OK/error, desconexiones, reconexiones |
| CPU | `slots[i]` | tipo, índice, online, fallas seguidas, totales |
| Entradas / salidas | `tramasOk`, `tramasError` | tramas atendidas y fallidas |
| Salidas | `tramasCrcError` | escrituras rechazadas por CRC |
| Salidas | `timeoutsComunicacion` | veces que pasó a estado seguro por timeout |

## Historial

- **v1**: tramas sin CRC, analógicas de 8 bits, escritura sin confirmación.
- **v2**: CRC-8 en todas las tramas, versión en la detección, analógicas de
  12 bits, `ACK`/`NAK` en la escritura, redetección, timeout de salidas y
  watchdog. **No es compatible con v1**: hay que actualizar los tres
  firmwares juntos.
