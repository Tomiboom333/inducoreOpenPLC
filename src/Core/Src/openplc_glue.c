/*
 * openplc_glue.c
 *
 * Une el programa IEC 61131-3 generado por OpenPLC/MatIEC
 * (src/OpenPLC/generated) con la imagen de proceso estAct de InduCore.
 *
 * El codigo generado accede a cada variable con direccion (%IX0.0, %QW1, ...)
 * mediante un puntero global con el nombre __IX0_0, __QW1, etc. Aca se crean
 * esos punteros a partir de LOCATED_VARIABLES.h y, en cada ciclo, se copian
 * los valores entre estAct y las variables IEC.
 */

#include "openplc_glue.h"
#include "iec_std_lib.h"
#include "induSPI.h"

/* Imagen de proceso definida en induSPI.c */
extern estAct_t estAct;

/* Definidos por el codigo generado (Config0.c) */
extern unsigned long long common_ticktime__;
void config_init__(void);
void config_run__(unsigned long tick);

/* Variables globales que exige la libreria IEC */
TIME __CURRENT_TIME;
BOOL __DEBUG;

/* ---- Variables con direccion ------------------------------------------ */

/* 1) Almacenamiento: BOOL ____IX0_0; INT ____QW0; ... */
#define __LOCATED_VAR(type, name, ...) type __##name;
#include "LOCATED_VARIABLES.h"
#undef __LOCATED_VAR

/* 2) Punteros que usa el codigo generado: BOOL *__IX0_0 = &____IX0_0; ... */
#define __LOCATED_VAR(type, name, ...) type *name = &__##name;
#include "LOCATED_VARIABLES.h"
#undef __LOCATED_VAR

/* 3) Tabla para recorrerlas en cada ciclo */
typedef enum {
    LOC_FIN = 0,
    LOC_IX, LOC_IB, LOC_IW, LOC_ID, LOC_IL,
    LOC_QX, LOC_QB, LOC_QW, LOC_QD, LOC_QL,
    LOC_MX, LOC_MB, LOC_MW, LOC_MD, LOC_ML
} loc_tipo_t;

typedef struct {
    loc_tipo_t tipo;
    void      *ptr;
    uint16_t   dir[2]; /* %IX<dir0>.<dir1> o %IW<dir0> */
} loc_var_t;

#define __LOCATED_VAR(type, name, area, size, ...) \
    { LOC_##area##size, (void *)&__##name, { __VA_ARGS__ } },
static const loc_var_t loc_vars[] = {
#include "LOCATED_VARIABLES.h"
    { LOC_FIN, 0, { 0, 0 } }
};
#undef __LOCATED_VAR

/* ---- Mapeo con estAct --------------------------------------------------- */

#define N_MODULOS  5
#define N_DIG      8
#define N_ANA      2

/* %QX<m>.0 es la salida 1 fisica (SAL_1), %QX<m>.1 la salida 2, etc.
 * Respeta el orden de bits de induSPI.h (SAL_1 = bit 3, ...). */
static const uint8_t bit_salida[N_DIG] = {
    SAL_1, SAL_2, SAL_3, SAL_4, SAL_5, SAL_6, SAL_7, SAL_8
};

static void copiar_entradas(void)
{
    for (const loc_var_t *v = loc_vars; v->tipo != LOC_FIN; v++) {
        uint16_t m, c;
        switch (v->tipo) {
        case LOC_IX:
            m = v->dir[0]; c = v->dir[1];
            if (m < N_MODULOS && c < N_DIG)
                *(BOOL *)v->ptr = estAct.modId[m][c] ? 1 : 0;
            break;
        case LOC_IW:
            m = v->dir[0] / N_ANA; c = v->dir[0] % N_ANA;
            if (m < N_MODULOS)
                *(INT *)v->ptr = (INT)estAct.modIa[m][c];
            break;
        case LOC_MW:
            /* %MW: memoria compartida con el Modbus esclavo (holding 1024+n) */
            if (v->dir[0] < PLC_N_MW)
                *(INT *)v->ptr = (INT)plc_mw[v->dir[0]];
            break;
        default:
            break;
        }
    }
}

static void copiar_salidas(void)
{
    for (const loc_var_t *v = loc_vars; v->tipo != LOC_FIN; v++) {
        uint16_t m, c;
        switch (v->tipo) {
        case LOC_QX:
            m = v->dir[0]; c = v->dir[1];
            if (m < N_MODULOS && c < N_DIG)
                estAct.modOd[m][bit_salida[c]] = (*(BOOL *)v->ptr) ? true : false;
            break;
        case LOC_QW: {
            INT val = *(INT *)v->ptr;
            m = v->dir[0] / N_ANA; c = v->dir[0] % N_ANA;
            if (val < 0)       val = 0;
            if (val > ANA_MAX) val = ANA_MAX;
            if (m < N_MODULOS)
                estAct.modOa[m][c] = (uint16_t)val;
            break;
        }
        case LOC_MW:
            if (v->dir[0] < PLC_N_MW)
                plc_mw[v->dir[0]] = (uint16_t)(*(INT *)v->ptr);
            break;
        default:
            break;
        }
    }
}

/* Memoria %MW compartida con el Modbus esclavo */
uint16_t plc_mw[PLC_N_MW];

/* ---- Ciclo -------------------------------------------------------------- */

volatile uint32_t plc_ciclos = 0;
volatile uint32_t plc_ciclos_excedidos = 0;
volatile uint32_t plc_periodo_ms = 1;

static unsigned long tick_iec = 0;

/* Se ejecuta repetidamente mientras se espera el proximo ciclo.
 * Definila en tu codigo (por ejemplo en main.c) para aprovechar ese tiempo,
 * como hace main.c con mb_procesar(). */
__attribute__((weak)) void plc_tarea_en_espera(void)
{
}
static uint32_t proximo_ciclo = 0;

void plc_openplc_init(void)
{
    __CURRENT_TIME.tv_sec = 0;
    __CURRENT_TIME.tv_nsec = 0;
    __DEBUG = 0;

    config_init__();

    plc_periodo_ms = (uint32_t)(common_ticktime__ / 1000000ULL);
    if (plc_periodo_ms == 0)
        plc_periodo_ms = 1;

    proximo_ciclo = HAL_GetTick();
}

void plc_openplc_wait_cycle(void)
{
    uint32_t ahora = HAL_GetTick();

    if ((int32_t)(ahora - proximo_ciclo) > 0) {
        /* El ciclo anterior se paso del periodo: se reprograma desde ahora */
        plc_ciclos_excedidos++;
        proximo_ciclo = ahora;
    } else {
        while ((int32_t)(HAL_GetTick() - proximo_ciclo) < 0) {
            plc_tarea_en_espera();
            induWatchdogRefresh();
        }
    }
    proximo_ciclo += plc_periodo_ms;
}

void plc_openplc_logic(void)
{
    copiar_entradas();

    config_run__(tick_iec++);

    /* Avanza el reloj IEC (lo usan TON, TOF, TP, etc.) */
    __CURRENT_TIME.tv_sec  += (long)(common_ticktime__ / 1000000000ULL);
    __CURRENT_TIME.tv_nsec += (long)(common_ticktime__ % 1000000000ULL);
    if (__CURRENT_TIME.tv_nsec >= 1000000000L) {
        __CURRENT_TIME.tv_nsec -= 1000000000L;
        __CURRENT_TIME.tv_sec  += 1;
    }

    copiar_salidas();
    plc_ciclos++;
}
