/*
 * mbus_mapa.c - Mapa de registros del Modbus esclavo (ver mbus_esclavo.h).
 * Se separa del nucleo para poder cambiar el mapa sin tocar el protocolo.
 */

#include "mbus_esclavo.h"
#include "induSPI.h"
#include "openplc_glue.h"

/* Bit de la trama SPI de cada salida fisica (igual que %QX en openplc_glue.c) */
static const uint8_t bit_salida[8] = { SAL_1, SAL_2, SAL_3, SAL_4, SAL_5, SAL_6, SAL_7, SAL_8 };

#define N_BITS      (N_MODULOS * 8)   /* 40 */
#define N_ANALOG    (N_MODULOS * 2)   /* 10 */
#define DIAG_INI    100
#define DIAG_N      12
#define MW_INI      1024

static uint16_t mascara_online(slot_tipo_t tipo)
{
    uint16_t m = 0;
    for (int s = 0; s < N_MODULOS; s++) {
        if (slots[s].tipo == tipo && slots[s].online)
            m |= (uint16_t)(1U << slots[s].indice);
    }
    return m;
}

/* Input registers 100..111 */
static uint16_t diagnostico(uint16_t i)
{
    switch (i) {
    case 0:  return PROTO_VERSION;                         /* 100 version del protocolo SPI */
    case 1:  return (uint16_t)contModulosIn;               /* 101 modulos de entradas */
    case 2:  return (uint16_t)contModulosOut;              /* 102 modulos de salidas */
    case 3:  return mascara_online(SLOT_ENTRADAS);         /* 103 bit i = MOD_IN_i responde */
    case 4:  return mascara_online(SLOT_SALIDAS);          /* 104 bit i = MOD_OUT_i responde */
    case 5:  return (uint16_t)plc_periodo_ms;              /* 105 periodo de ciclo (ms) */
    case 6:  return (uint16_t)(plc_ciclos & 0xFFFF);       /* 106 ciclos (parte baja) */
    case 7:  return (uint16_t)(plc_ciclos >> 16);          /* 107 ciclos (parte alta) */
    case 8:  return (uint16_t)plc_ciclos_excedidos;        /* 108 ciclos excedidos */
    case 9:  return (uint16_t)spiEstadisticas.lecturasError;   /* 109 */
    case 10: return (uint16_t)spiEstadisticas.escriturasError; /* 110 */
    case 11: return (uint16_t)spiEstadisticas.desconexiones;   /* 111 */
    default: return 0;
    }
}

bool mbs_leer_bit(mbs_tabla_t tabla, uint16_t dir, bool *valor)
{
    if (dir >= N_BITS)
        return false;
    uint16_t m = dir / 8, c = dir % 8;
    if (tabla == MBS_COILS)
        *valor = estAct.modOd[m][bit_salida[c]];
    else if (tabla == MBS_ENTRADAS)
        *valor = estAct.modId[m][c];
    else
        return false;
    return true;
}

bool mbs_escribir_bit(uint16_t dir, bool valor)
{
    if (dir >= N_BITS)
        return false;
    estAct.modOd[dir / 8][bit_salida[dir % 8]] = valor;
    return true;
}

bool mbs_leer_registro(mbs_tabla_t tabla, uint16_t dir, uint16_t *valor)
{
    if (tabla == MBS_INPUT_REGS) {
        if (dir < N_ANALOG) { *valor = estAct.modIa[dir / 2][dir % 2]; return true; }
        if (dir >= DIAG_INI && dir < DIAG_INI + DIAG_N) { *valor = diagnostico((uint16_t)(dir - DIAG_INI)); return true; }
        return false;
    }
    if (tabla == MBS_HOLDING) {
        if (dir < N_ANALOG) { *valor = estAct.modOa[dir / 2][dir % 2]; return true; }
        if (dir >= MW_INI && dir < MW_INI + PLC_N_MW) { *valor = plc_mw[dir - MW_INI]; return true; }
        return false;
    }
    return false;
}

bool mbs_escribir_registro(uint16_t dir, uint16_t valor, bool *valor_invalido)
{
    *valor_invalido = false;
    if (dir < N_ANALOG) {
        if (valor > ANA_MAX) { *valor_invalido = true; return false; }
        estAct.modOa[dir / 2][dir % 2] = valor;
        return true;
    }
    if (dir >= MW_INI && dir < MW_INI + PLC_N_MW) {
        plc_mw[dir - MW_INI] = valor;
        return true;
    }
    return false;
}
