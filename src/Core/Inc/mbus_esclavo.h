#ifndef MBUS_ESCLAVO_H
#define MBUS_ESCLAVO_H

/*
 * Modbus RTU ESCLAVO: permite que un SCADA, un HMI o una PC lean y escriban
 * las entradas, salidas y memoria del PLC.
 *
 * Puerto (elegir con MBS_PUERTO):
 *   MBS_PUERTO_USB   -> USART1 (PA9/PA10) -> CH340G -> USB mini-B de la CPU.
 *                       El RS-485 sigue disponible para el maestro.  (por defecto)
 *   MBS_PUERTO_RS485 -> USART3 + MAX485. En este modo el maestro Modbus queda
 *                       deshabilitado (sus peticiones devuelven MB_ERR_PUERTO).
 *
 * Mapa de registros (direcciones base 0, igual criterio que el runtime de OpenPLC):
 *
 *   Coils            (01, 05, 0F)  0..39      %QX m.s  -> 8*m + s   (salidas digitales)
 *   Discrete inputs  (02)          0..39      %IX m.e  -> 8*m + e   (entradas digitales)
 *   Input registers  (04)          0..9       %IW n                  (entradas analogicas 0..4095)
 *                                  100..111   diagnostico (ver mbus_mapa.c)
 *   Holding registers(03, 06, 10)  0..9       %QW n                  (salidas analogicas 0..4095)
 *                                  1024..1087 %MW 0..63              (memoria del programa)
 *
 * Ojo: las salidas que escribe el programa del PLC se pisan en cada ciclo.
 * Escribir por Modbus solo las que el programa no usa, o usar %MW como
 * "consignas" que el programa lee.
 */

#include <stdint.h>
#include <stdbool.h>

#define MBS_PUERTO_USB    1
#define MBS_PUERTO_RS485  2

#ifndef MBS_PUERTO
#define MBS_PUERTO        MBS_PUERTO_USB
#endif
#ifndef MBS_DIRECCION
#define MBS_DIRECCION     1        /* direccion de esclavo del PLC (1..247) */
#endif
#ifndef MBS_BAUDIOS
#define MBS_BAUDIOS       115200   /* solo para el puerto USB; el RS-485 usa la config de USART3 */
#endif

/* Codigos de excepcion */
#define MBS_EXC_FUNCION   0x01
#define MBS_EXC_DIRECCION 0x02
#define MBS_EXC_VALOR     0x03

typedef enum {
    MBS_COILS = 0,
    MBS_ENTRADAS,     /* discrete inputs */
    MBS_HOLDING,
    MBS_INPUT_REGS
} mbs_tabla_t;

typedef struct {
    uint32_t tramas;          /* tramas recibidas con CRC correcto para esta direccion */
    uint32_t respuestas;
    uint32_t excepciones;
    uint32_t errores_crc;
    uint32_t otras_direcciones;
    uint32_t broadcast;
} mbs_estadisticas_t;
extern mbs_estadisticas_t mbsEstadisticas;

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Nucleo (sin HAL) ----------------------------------------------------- */

/* Procesa una peticion completa (con CRC). Devuelve el largo de la respuesta
 * en 'resp' (con CRC), o 0 si no hay que responder (CRC incorrecto, otra
 * direccion o broadcast). */
uint16_t mbs_procesar_trama(uint8_t direccion, const uint8_t *req, uint16_t n, uint8_t *resp);

/* Acceso a los datos (implementado en mbus_mapa.c). Devuelven false si la
 * direccion no existe; mbs_escribir_registro tambien si el valor es invalido
 * (en ese caso *valor_invalido = true). */
bool mbs_leer_bit(mbs_tabla_t tabla, uint16_t dir, bool *valor);
bool mbs_escribir_bit(uint16_t dir, bool valor);
bool mbs_leer_registro(mbs_tabla_t tabla, uint16_t dir, uint16_t *valor);
bool mbs_escribir_registro(uint16_t dir, uint16_t valor, bool *valor_invalido);

/* ---- Puerto serie (mbus_esclavo_uart.c) ----------------------------------- */
void mbs_init(void);           /* despues de induInit() */
void mbs_procesar(void);       /* llamar seguido (main.c lo hace mientras espera el ciclo) */
bool mbs_usa_rs485(void);

/* Llamadas desde los callbacks del HAL (mbus_funcs.c) */
bool mbs_es_su_uart(const void *huart);
void mbs_rx_evento(uint16_t largo);
void mbs_rx_error(void);

#ifdef __cplusplus
}
#endif

#endif /* MBUS_ESCLAVO_H */
