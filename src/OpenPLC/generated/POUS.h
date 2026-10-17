#ifndef __POUS_H
#define __POUS_H

#include "accessor.h"
#include "iec_std_lib.h"

// PROGRAM PRUEBA
// Data part
typedef struct {
  // PROGRAM Interface - IN, OUT, IN_OUT variables

  // PROGRAM private variables - TEMP, private and located variables
  __DECLARE_LOCATED(BOOL,ENT1)
  __DECLARE_LOCATED(INT,AN1)
  __DECLARE_LOCATED(BOOL,SAL1)
  __DECLARE_LOCATED(BOOL,SAL3)
  __DECLARE_LOCATED(INT,ANOUT1)
  __DECLARE_LOCATED(INT,ANOUT2)
  TON T1;
  TON T2;
  __DECLARE_VAR(BOOL,PARPADEO)

} PRUEBA;

void PRUEBA_init__(PRUEBA *data__, BOOL retain);
// Code part
void PRUEBA_body__(PRUEBA *data__);
#endif //__POUS_H
