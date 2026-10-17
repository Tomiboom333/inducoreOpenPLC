/*
 * Test en PC (sin hardware) del programa de ejemplo (programas/ejemplo.st)
 * y de la capa openplc_glue.c. Simula estAct y el reloj HAL_GetTick().
 * Se corre con:  python test_pc/correr_test.py
 */
#include <stdio.h>
#include <stdbool.h>
#include "induSPI.h"
#include "openplc_glue.h"
estAct_t estAct;
static uint32_t fake_ms = 0;
uint32_t HAL_GetTick(void){ return fake_ms; }
int fallas = 0;
#define CHECK(c,msg) do{ if(!(c)){ printf("FALLA: %s\n",msg); fallas++; } }while(0)
int main(void){
  plc_openplc_init();
  printf("periodo = %u ms\n", (unsigned)plc_periodo_ms);
  CHECK(plc_periodo_ms==20,"periodo 20 ms");
  /* analogica alta -> %QX0.2 (SAL_3 = bit 1) */
  estAct.modIa[0][0]=200; plc_openplc_logic(); fake_ms+=20;
  CHECK(estAct.modOd[0][SAL_3]==1,"sal3 con an1=200");
  estAct.modIa[0][0]=100; plc_openplc_logic(); fake_ms+=20;
  CHECK(estAct.modOd[0][SAL_3]==0,"sal3 con an1=100");
  CHECK(estAct.modOa[0][0]==255 && estAct.modOa[0][1]==127,"salidas analogicas");
  /* parpadeo: contar cambios de %QX0.0 (SAL_1 = bit 3) en 3 s */
  int cambios=0; bool prev=estAct.modOd[0][SAL_1];
  for(int i=0;i<150;i++){ plc_openplc_logic(); bool v=estAct.modOd[0][SAL_1]; if(v!=prev){cambios++; printf("t=%4u ms  sal1=%d\n",(unsigned)(i*20),v);} prev=v; }
  CHECK(cambios>=5 && cambios<=7,"parpadeo ~0.5 s en 3 s");
  /* entrada 1 fuerza sal1 */
  estAct.modId[0][0]=1; int on=0; for(int i=0;i<60;i++){ plc_openplc_logic(); on+=estAct.modOd[0][SAL_1]; }
  CHECK(on==60,"ent1 fuerza sal1");
  /* salidas que el programa no usa quedan en 0 */
  CHECK(estAct.modOd[0][SAL_2]==0 && estAct.modOd[1][0]==0,"salidas no usadas");
  printf("ciclos=%u  %s\n",(unsigned)plc_ciclos, fallas? "HAY FALLAS":"TODO OK");
  return fallas;
}
