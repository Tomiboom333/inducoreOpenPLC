#include "main.h"
#include "induSPI.h"
#include "openplc_glue.h"
#include "mbus_funcs.h"

/*
 * Modulo CPU de InduCore ejecutando un programa IEC 61131-3 (Ladder, ST, FBD...)
 * generado con OpenPLC Editor + MatIEC.
 *
 * El programa vive en src/OpenPLC/generated y se regenera con:
 *     python tools/generar_programa.py programas/<tu_programa>.st
 *
 * Cada ciclo: espera el periodo de la TASK (atendiendo Modbus mientras tanto)
 * -> lee entradas por SPI -> ejecuta el programa IEC -> escribe salidas por SPI.
 */

/* Poner en 1 para probar el maestro Modbus con un esclavo real */
#define EJEMPLO_MODBUS 0

/* Mientras el CPU espera el proximo ciclo, se atienden las respuestas Modbus */
void plc_tarea_en_espera(void)
{
  mb_procesar();
}

#if EJEMPLO_MODBUS
/* Ejemplo: cada 500 ms lee 10 holding registers (desde la direccion 0)
 * del esclavo 1 y guarda los valores cuando la respuesta es valida. */
#define ESCLAVO        1
#define CANT_REGISTROS 10

uint16_t registros_esclavo1[CANT_REGISTROS];   /* ultimos valores validos */
volatile mb_estado_t ultimo_estado = MB_SIN_PETICION;

static void respuesta_modbus(const mb_respuesta_t *r)
{
  ultimo_estado = r->estado;
  if (r->estado == MB_OK && r->funcion == MB_FC_LEER_HOLDING) {
    for (int i = 0; i < CANT_REGISTROS; i++)
      registros_esclavo1[i] = r->registros[i];
  }
  /* Si hubo error (timeout, CRC, excepcion) se conservan los valores anteriores.
     El detalle queda en r->estado, r->excepcion y en mbEstadisticas. */
}

static void ejemplo_modbus(void)
{
  static uint32_t ultimo = 0;
  if (!mb_ocupado() && (HAL_GetTick() - ultimo) >= 500) {
    ultimo = HAL_GetTick();
    readHoldingRegisters(ESCLAVO, 0, CANT_REGISTROS);
  }
}
#endif

int main(void)
{
  induInit();          /* reloj, GPIO, SPI, UART y deteccion de modulos */
  plc_openplc_init();  /* inicializa el programa IEC y el periodo de ciclo */

#if EJEMPLO_MODBUS
  mb_set_callback(respuesta_modbus);
#endif

  while (1) {
    plc_openplc_wait_cycle();
    plc_run_cycle(plc_openplc_logic);
    mb_procesar();
#if EJEMPLO_MODBUS
    ejemplo_modbus();
#endif
  }
}
