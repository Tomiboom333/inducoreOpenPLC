(* Programa de ejemplo para InduCore.
   Reproduce la funcion algo() del firmware original y agrega un
   parpadeo con temporizadores TON para probar el tiempo de ciclo.

   Mapa de direcciones (ver README):
     %IX<m>.<e>  entrada digital e (0..7) del modulo de entradas m (0..4)
     %IW<n>      entrada analogica: modulo n/2, canal n%2   (0..255)
     %QX<m>.<s>  salida digital s (0..7) del modulo de salidas m (0..4)
     %QW<n>      salida analogica: modulo n/2, canal n%2    (0..255)  *)

PROGRAM prueba
  VAR
    ent1   AT %IX0.0 : BOOL;   (* Entrada 1 del modulo de entradas 1 *)
    an1    AT %IW0   : INT;    (* Analogica 1 del modulo de entradas 1 *)
    sal1   AT %QX0.0 : BOOL;   (* Salida 1 del modulo de salidas 1 *)
    sal3   AT %QX0.2 : BOOL;   (* Salida 3 del modulo de salidas 1 *)
    anout1 AT %QW0   : INT;    (* Analogica 1 del modulo de salidas 1 *)
    anout2 AT %QW1   : INT;    (* Analogica 2 del modulo de salidas 1 *)
  END_VAR
  VAR
    t1 : TON;
    t2 : TON;
    parpadeo : BOOL;
  END_VAR

  (* Igual que algo(): salida 3 activa si la analogica supera la mitad *)
  sal3 := an1 >= 128;

  (* Parpadeo de 0,5 s encendido / 0,5 s apagado *)
  t1(IN := NOT t2.Q, PT := T#500ms);
  t2(IN := t1.Q, PT := T#500ms);
  parpadeo := t1.Q;

  (* Salida 1 parpadea, o queda fija si la entrada 1 esta activa *)
  sal1 := parpadeo OR ent1;

  anout1 := 255;
  anout2 := 127;
END_PROGRAM

CONFIGURATION Config0
  RESOURCE Res0 ON PLC
    TASK task0(INTERVAL := T#20ms, PRIORITY := 0);
    PROGRAM instance0 WITH task0 : prueba;
  END_RESOURCE
END_CONFIGURATION
