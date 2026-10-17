#include "main.h"
#include "induSPI.h"
#include "openplc_glue.h"

/*
 * Modulo CPU de InduCore ejecutando un programa IEC 61131-3 (Ladder, ST, FBD...)
 * generado con OpenPLC Editor + MatIEC.
 *
 * El programa vive en src/OpenPLC/generated y se regenera con:
 *     python tools/generar_programa.py programas/<tu_programa>.st
 *
 * Cada ciclo: espera el periodo de la TASK -> lee entradas por SPI ->
 * ejecuta el programa IEC -> escribe salidas por SPI.
 */

int main(void)
{
  induInit();          /* reloj, GPIO, SPI, UART y deteccion de modulos */
  plc_openplc_init();  /* inicializa el programa IEC y el periodo de ciclo */

  while (1) {
    plc_openplc_wait_cycle();
    plc_run_cycle(plc_openplc_logic);
  }
}
