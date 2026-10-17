void PRUEBA_init__(PRUEBA *data__, BOOL retain) {
  __INIT_LOCATED(BOOL,__IX0_0,data__->ENT1,retain)
  __INIT_LOCATED_VALUE(data__->ENT1,__BOOL_LITERAL(FALSE))
  __INIT_LOCATED(INT,__IW0,data__->AN1,retain)
  __INIT_LOCATED_VALUE(data__->AN1,0)
  __INIT_LOCATED(BOOL,__QX0_0,data__->SAL1,retain)
  __INIT_LOCATED_VALUE(data__->SAL1,__BOOL_LITERAL(FALSE))
  __INIT_LOCATED(BOOL,__QX0_2,data__->SAL3,retain)
  __INIT_LOCATED_VALUE(data__->SAL3,__BOOL_LITERAL(FALSE))
  __INIT_LOCATED(INT,__QW0,data__->ANOUT1,retain)
  __INIT_LOCATED_VALUE(data__->ANOUT1,0)
  __INIT_LOCATED(INT,__QW1,data__->ANOUT2,retain)
  __INIT_LOCATED_VALUE(data__->ANOUT2,0)
  TON_init__(&data__->T1,retain);
  TON_init__(&data__->T2,retain);
  __INIT_VAR(data__->PARPADEO,__BOOL_LITERAL(FALSE),retain)
}

// Code part
void PRUEBA_body__(PRUEBA *data__) {
  // Initialise TEMP variables

  __SET_LOCATED(data__->,SAL3,,(__GET_LOCATED(data__->AN1,) >= 128));
  __SET_VAR(data__->T1.,IN,,!(__GET_VAR(data__->T2.Q,)));
  __SET_VAR(data__->T1.,PT,,__time_to_timespec(1, 500, 0, 0, 0, 0));
  TON_body__(&data__->T1);
  __SET_VAR(data__->T2.,IN,,__GET_VAR(data__->T1.Q,));
  __SET_VAR(data__->T2.,PT,,__time_to_timespec(1, 500, 0, 0, 0, 0));
  TON_body__(&data__->T2);
  __SET_VAR(data__->,PARPADEO,,__GET_VAR(data__->T1.Q,));
  __SET_LOCATED(data__->,SAL1,,(__GET_VAR(data__->PARPADEO,) || __GET_LOCATED(data__->ENT1,)));
  __SET_LOCATED(data__->,ANOUT1,,255);
  __SET_LOCATED(data__->,ANOUT2,,127);

  goto __end;

__end:
  return;
} // PRUEBA_body__() 





