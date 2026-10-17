#ifndef INDUSPI_H
#define INDUSPI_H

#define LOW 0
#define HIGH 1

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

#define SAL_1 3
#define SAL_2 2
#define SAL_3 1
#define SAL_4 0
#define SAL_5 4
#define SAL_6 5
#define SAL_7 6
#define SAL_8 7

#include "stm32f103xb.h"
#include "stdbool.h"
#include "stdint.h"
#include "main.h"
typedef struct 
{
    bool modId[5][8];
    bool modOd[5][8];
    uint8_t modIa[5][2];
    uint8_t modOa[5][2];
}estAct_t;
extern SPI_HandleTypeDef hspi1;

#ifdef __cplusplus
extern "C" {
#endif

//empezamos
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi);
void induInit();

void digWrite(int modulo, int salida, bool estado);

void anWrite(int modulo, int salida, uint8_t valor);

bool digRead(int modulo, int entrada);

uint8_t anRead(int modulo, int entrada);

void moduleDet();
void plc_read_inputs();
void plc_write_outputs();
void plc_run_cycle(void (*fuser)(void));
void plc_store_spi_inputs(int j);


#ifdef __cplusplus
}
#endif//c++

#endif//.h

