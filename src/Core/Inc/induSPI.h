#ifndef INDUSPI_H
#define INDUSPI_H

/*
 * induSPI.h - Backplane SPI del modulo CPU de InduCore: deteccion de modulos,
 * lectura de entradas, escritura de salidas y API para la logica del usuario.
 * El formato de las tramas esta en induproto.h y docs/PROTOCOLO.md.
 */

#define LOW 0
#define HIGH 1

/* Indice de modulo: orden fisico en el riel (de izquierda a derecha),
 * contando por separado los de entradas y los de salidas. */
#define MOD_IN_1 0
#define MOD_IN_2 1
#define MOD_IN_3 2
#define MOD_IN_4 3
#define MOD_IN_5 4

#define MOD_OUT_1 0
#define MOD_OUT_2 1
#define MOD_OUT_3 2
#define MOD_OUT_4 3
#define MOD_OUT_5 4

#define ENT_1 0
#define ENT_2 1
#define ENT_3 2
#define ENT_4 3
#define ENT_5 4
#define ENT_6 5
#define ENT_7 6
#define ENT_8 7

/* Bit de la trama SPI que corresponde a cada salida fisica */
#define SAL_1 3
#define SAL_2 2
#define SAL_3 1
#define SAL_4 0
#define SAL_5 4
#define SAL_6 5
#define SAL_7 6
#define SAL_8 7

/* Canales analogicos (entradas y salidas) */
#define AN_1 0   /* entradas: InAn1 | salidas: AO0 */
#define AN_2 1   /* entradas: InAn2 | salidas: AO1 */

#define N_MODULOS 5
#define ANA_MAX   4095   /* analogicas de 12 bits */

#include "stm32f103xb.h"
#include "stdbool.h"
#include "stdint.h"
#include "main.h"
#include "induproto.h"

/* Imagen de proceso */
typedef struct
{
    bool modId[N_MODULOS][8];       /* entradas digitales */
    bool modOd[N_MODULOS][8];       /* salidas digitales (indice = bit SAL_x) */
    uint16_t modIa[N_MODULOS][2];   /* entradas analogicas 0..4095 */
    uint16_t modOa[N_MODULOS][2];   /* salidas analogicas 0..4095 */
} estAct_t;

/* Estado de cada slot del riel */
typedef enum {
    SLOT_VACIO = 0,
    SLOT_ENTRADAS,
    SLOT_SALIDAS
} slot_tipo_t;

typedef struct {
    slot_tipo_t tipo;        /* tipo de modulo asignado a este slot */
    uint8_t     indice;      /* indice dentro de su tipo (MOD_IN_x / MOD_OUT_x) */
    bool        online;      /* responde correctamente */
    uint8_t     fallas;      /* transacciones fallidas seguidas */
    uint32_t    ok;          /* transacciones correctas */
    uint32_t    errores;     /* transacciones fallidas en total */
} slot_t;

/* Diagnostico del backplane (se puede ver con el depurador o por Modbus) */
typedef struct {
    uint32_t lecturasOk;
    uint32_t lecturasError;
    uint32_t escriturasOk;
    uint32_t escriturasError;   /* sin respuesta o NAK */
    uint32_t desconexiones;     /* veces que un modulo paso a offline */
    uint32_t reconexiones;      /* veces que un modulo volvio a responder */
} spi_estadisticas_t;

extern estAct_t estAct;
extern slot_t slots[N_MODULOS];
extern spi_estadisticas_t spiEstadisticas;
extern int contModulosIn, contModulosOut;
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;

/* Fallas seguidas para considerar un modulo desconectado */
#define SPI_FALLAS_OFFLINE      3
/* Cada cuanto se buscan modulos nuevos o desconectados (ms) */
#define SPI_REDETECCION_MS      1000
/* 1: las entradas de un modulo desconectado se ponen en 0 (falla segura).
 * 0: se conserva la ultima lectura valida. */
#define ENTRADAS_EN_CERO_SI_FALLA 1

#ifdef __cplusplus
extern "C" {
#endif

void induInit(void);

/* API para la logica del usuario */
void     digWrite(int modulo, int salida, bool estado);
void     anWrite(int modulo, int salida, uint16_t valor);   /* 0..4095 */
bool     digRead(int modulo, int entrada);
uint16_t anRead(int modulo, int entrada);                  /* 0..4095 */
bool     modEntradasOk(int modulo);   /* true si el modulo de entradas responde */
bool     modSalidasOk(int modulo);    /* true si el modulo de salidas responde */

/* Backplane */
void moduleDet(void);            /* deteccion completa (al arrancar) */
void plc_redetectar(void);       /* revisa slots vacios u offline (se llama solo cada SPI_REDETECCION_MS) */
void plc_read_inputs(void);
void plc_write_outputs(void);
void plc_run_cycle(void (*fuser)(void));

/* Hardware (induHW.c) */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_SPI1_Init(void);
void MX_USART1_UART_Init(uint32_t baudrate);
void MX_USART3_UART_Init(void);
void induWatchdogInit(void);
void induWatchdogRefresh(void);

#ifdef __cplusplus
}
#endif

#endif /* INDUSPI_H */
