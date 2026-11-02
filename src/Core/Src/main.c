#include "main.h"
#include "induSPI.h"
#include "openplc_glue.h"
#include "mbus_funcs.h"
#include "mbus_esclavo.h"

/*
 * Modulo CPU de InduCore ejecutando un programa IEC 61131-3 (Ladder, ST, FBD...)
 * generado con OpenPLC Editor + MatIEC.
 *
 * El programa vive en src/OpenPLC/generated y se regenera con:
 *     python tools/generar_programa.py programas/<tu_programa>.st
 *
 * Cada ciclo: espera el periodo de la TASK (atendiendo Modbus mientras tanto)
 * -> lee entradas por SPI -> ejecuta el programa IEC -> escribe salidas por SPI.
 *
 * Modbus: el PLC es esclavo por USB (USART1) para SCADA/HMI, y maestro por
 * RS-485 (USART3). Ver mbus_esclavo.h para el mapa de registros y para usar
 * el RS-485 como esclavo.
 */

/* Poner en 1 para probar el maestro Modbus con un esclavo real */
#define EJEMPLO_MODBUS 0

/* Mientras el CPU espera el proximo ciclo, se atienden las respuestas Modbus */
void plc_tarea_en_espera(void)
{
  mb_procesar();    /* respuestas del maestro */
  mbs_procesar();   /* peticiones al esclavo */
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

/*
 * PRUEBA DE PLACAS (protocolo v2)
 *   1 = corre prueba_logica() escrita en C, sin el programa de OpenPLC.
 *   0 = firmware normal, con el programa IEC generado.
 *
 * Usa MOD_IN_1 y MOD_OUT_1 (los modulos mas cercanos a la CPU).
 * Con todas las entradas en reposo, las salidas digitales se encienden de a
 * una cada 500 ms. Al activar una entrada, se enciende solo su salida.
 * AO0 copia InAn1 y AO1 hace una rampa de 0 a 100 % en 5 s.
 * Las salidas analogicas van de 0 a 255 (ANA_MAX); las entradas, de 0 a 4095.
 */
#define PRUEBA_PLACAS 1

#if PRUEBA_PLACAS
#define PRUEBA_PASO_MS     500U
#define PRUEBA_RAMPA_MS    5000U
#define PRUEBA_CICLOS_REPOSO 10U   /* ciclos antes de guardar el reposo */

/* Para ver con el depurador */
volatile uint8_t  prueba_entradas_crudas = 0;   /* tal como llegan del modulo */
volatile uint8_t  prueba_entradas_reposo = 0;   /* lectura tomada al arrancar */
volatile uint8_t  prueba_entradas_activas = 0;  /* bits distintos del reposo */
volatile uint16_t prueba_an1 = 0;
volatile uint16_t prueba_an2 = 0;
volatile bool     prueba_entradas_ok = false;
volatile bool     prueba_salidas_ok = false;

static const uint8_t prueba_ent[8] = {
  ENT_1, ENT_2, ENT_3, ENT_4, ENT_5, ENT_6, ENT_7, ENT_8
};
static const uint8_t prueba_sal[8] = {
  SAL_1, SAL_2, SAL_3, SAL_4, SAL_5, SAL_6, SAL_7, SAL_8
};

static void prueba_logica(void)
{
  static uint8_t ciclos_ok = 0;
  static bool reposo_guardado = false;
  uint32_t ahora = HAL_GetTick();

  prueba_entradas_ok = modEntradasOk(MOD_IN_1);
  prueba_salidas_ok = modSalidasOk(MOD_OUT_1);

  uint8_t crudas = 0;
  for (int i = 0; i < 8; i++) {
    if (digRead(MOD_IN_1, prueba_ent[i]))
      crudas |= (uint8_t)(1U << i);
  }
  prueba_entradas_crudas = crudas;

  /* El reposo se toma de la placa, asi la prueba sirve con cualquier
   * polaridad. Las entradas tienen que estar sin activar al arrancar. */
  if (!reposo_guardado && prueba_entradas_ok && ++ciclos_ok >= PRUEBA_CICLOS_REPOSO) {
    prueba_entradas_reposo = crudas;
    reposo_guardado = true;
  }
  uint8_t activas = reposo_guardado ? (uint8_t)(crudas ^ prueba_entradas_reposo) : 0;
  prueba_entradas_activas = activas;

  /* Salidas digitales: copia de las entradas, o recorrido si no hay ninguna */
  uint32_t paso = (ahora / PRUEBA_PASO_MS) % 8U;
  for (uint32_t i = 0; i < 8U; i++) {
    bool estado = activas ? ((activas >> i) & 1U) != 0U : (i == paso);
    digWrite(MOD_OUT_1, prueba_sal[i], estado);
  }

  /* Analogicas: AO0 copia InAn1 (de 12 a 8 bits); AO1 sube de 0 a 255 en 5 s */
  prueba_an1 = anRead(MOD_IN_1, AN_1);
  prueba_an2 = anRead(MOD_IN_1, AN_2);
  anWrite(MOD_OUT_1, AN_1, (uint16_t)(prueba_an1 >> 4));
  anWrite(MOD_OUT_1, AN_2,
          (uint16_t)(((ahora % PRUEBA_RAMPA_MS) * ANA_MAX) / (PRUEBA_RAMPA_MS - 1U)));

  /* Lo mismo por Modbus: holding registers 1024 a 1028 */
  plc_mw[0] = (uint16_t)plc_ciclos;
  plc_mw[1] = crudas;
  plc_mw[2] = prueba_an1;
  plc_mw[3] = prueba_an2;
  plc_mw[4] = (uint16_t)((prueba_salidas_ok ? 2U : 0U) | (prueba_entradas_ok ? 1U : 0U));

  plc_ciclos++;
}
#endif /* PRUEBA_PLACAS */

int main(void)
{
  induInit();          /* reloj, GPIO, SPI, UART y deteccion de modulos */
  plc_openplc_init();  /* inicializa el programa IEC y el periodo de ciclo */
  mbs_init();          /* Modbus esclavo */

#if EJEMPLO_MODBUS
  mb_set_callback(respuesta_modbus);
#endif

  while (1) {
    plc_openplc_wait_cycle();
#if PRUEBA_PLACAS
    plc_run_cycle(prueba_logica);
#else
    plc_run_cycle(plc_openplc_logic);
#endif
    mb_procesar();
    mbs_procesar();
#if EJEMPLO_MODBUS
    ejemplo_modbus();
#endif
  }
}
