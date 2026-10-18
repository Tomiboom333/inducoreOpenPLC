#ifndef OPENPLC_GLUE_H
#define OPENPLC_GLUE_H

/*
 * Capa de union entre el codigo generado por OpenPLC/MatIEC y la imagen
 * de proceso de InduCore (estAct, ver induSPI.h).
 *
 * Mapa de direcciones IEC 61131-3:
 *   %IX<m>.<e>  -> estAct.modId[m][e]          m = modulo de entradas 0..4, e = 0..7
 *   %IW<n>      -> estAct.modIa[n/2][n%2]      valores 0..4095 (12 bits)
 *   %QX<m>.<s>  -> estAct.modOd[m][SAL_(s+1)]  m = modulo de salidas 0..4,  s = 0..7
 *   %QW<n>      -> estAct.modOa[n/2][n%2]      se recorta a 0..4095
 *   %MW<n>      -> plc_mw[n]                   n = 0..63, compartida con Modbus
 *
 * El indice de modulo es el orden en que moduleDet() los detecta.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* %MW0..%MW63: memoria del programa IEC compartida con el Modbus esclavo
 * (holding registers 1024..1087, como en el runtime de OpenPLC). */
#define PLC_N_MW 64
extern uint16_t plc_mw[PLC_N_MW];

/* Inicializa el programa IEC. Llamar una vez despues de induInit(). */
void plc_openplc_init(void);

/* Logica de un ciclo: copia entradas, ejecuta el programa IEC y copia salidas.
 * Se pasa a plc_run_cycle(). */
void plc_openplc_logic(void);

/* Espera hasta el inicio del proximo ciclo segun el INTERVAL de la TASK. */
void plc_openplc_wait_cycle(void);

/* Tarea que se ejecuta mientras se espera el proximo ciclo (por defecto vacia).
 * Se puede redefinir en cualquier archivo .c. */
void plc_tarea_en_espera(void);

/* Diagnostico */
extern volatile uint32_t plc_ciclos;          /* ciclos ejecutados */
extern volatile uint32_t plc_ciclos_excedidos; /* ciclos que tardaron mas que el periodo */
extern volatile uint32_t plc_periodo_ms;       /* periodo de ciclo en ms */

#ifdef __cplusplus
}
#endif

#endif /* OPENPLC_GLUE_H */
