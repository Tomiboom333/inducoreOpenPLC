/*
 * induSPI.c - Backplane SPI del modulo CPU (maestro).
 *
 * Cada ciclo del PLC:
 *   plc_read_inputs()  -> lee los modulos de entradas  (comando 0x01)
 *   logica del usuario
 *   plc_write_outputs()-> escribe los modulos de salidas (comando 0x02 + ACK)
 *
 * Todas las tramas llevan CRC-8 (induproto.h). Si un modulo falla
 * SPI_FALLAS_OFFLINE veces seguidas pasa a "offline"; cada
 * SPI_REDETECCION_MS se vuelven a buscar modulos offline y slots vacios,
 * asi un modulo que se reinicia o se enchufa despues vuelve solo.
 */

#include "induSPI.h"
#include "mbus_funcs.h"
#include <string.h>

estAct_t estAct;
slot_t slots[N_MODULOS];
spi_estadisticas_t spiEstadisticas;
int contModulosIn = 0, contModulosOut = 0;

/* Slot fisico (0 = el primero a la derecha de la CPU) -> chip select.
 * SPI1_SSn es la linea SSn del riel (ver main.h). */
static GPIO_TypeDef *const cs_puerto[N_MODULOS] = {
  SPI1_SS0_GPIO_Port, SPI1_SS1_GPIO_Port, SPI1_SS2_GPIO_Port,
  SPI1_SS3_GPIO_Port, SPI1_SS4_GPIO_Port
};
static const uint16_t cs_pin[N_MODULOS] = {
  SPI1_SS0_Pin, SPI1_SS1_Pin, SPI1_SS2_Pin, SPI1_SS3_Pin, SPI1_SS4_Pin
};

/* Slot de cada indice de modulo (-1 = no asignado) */
static int8_t slotDeEntrada[N_MODULOS];
static int8_t slotDeSalida[N_MODULOS];
static uint32_t ultimaRedeteccion = 0;

/* ---- Transporte SPI ------------------------------------------------------ */

static void cs_todos_alto(void)
{
  for (int i = 0; i < N_MODULOS; i++)
    HAL_GPIO_WritePin(cs_puerto[i], cs_pin[i], GPIO_PIN_SET);
}

/* Espera aproximada de al menos 'us' microsegundos a 72 MHz */
static void pausa_us(uint32_t us)
{
  for (volatile uint32_t i = 0; i < us * 12U; i++) {
  }
}

static void cs_bajo(int slot)
{
  cs_todos_alto();
  while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
  }
  HAL_GPIO_WritePin(cs_puerto[slot], cs_pin[slot], GPIO_PIN_RESET);
}

/* Envia el comando, espera a que el esclavo lo procese y recibe n bytes */
static bool trans_leer(int slot, uint8_t cmd, uint8_t *rx, uint16_t n)
{
  memset(rx, 0, n);
  cs_bajo(slot);
  HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi1, &cmd, 1, 5);
  pausa_us(PROTO_PAUSA_US);
  if (st == HAL_OK)
    st = HAL_SPI_Receive(&hspi1, rx, n, 5);
  cs_todos_alto();
  return st == HAL_OK;
}

/* Detecta que modulo hay en un slot */
static slot_tipo_t detectar_slot(int slot)
{
  uint8_t rx[PROTO_LARGO_DETECCION];
  if (!trans_leer(slot, PROTO_CMD_DETECCION, rx, sizeof(rx)))
    return SLOT_VACIO;
  switch (proto_leer_deteccion(rx)) {
  case PROTO_ID_ENTRADAS: return SLOT_ENTRADAS;
  case PROTO_ID_SALIDAS:  return SLOT_SALIDAS;
  default:                return SLOT_VACIO;
  }
}

/* ---- Estado de los slots -------------------------------------------------- */

static void entradas_en_cero(int idx)
{
#if ENTRADAS_EN_CERO_SI_FALLA
  for (int i = 0; i < 8; i++)
    estAct.modId[idx][i] = false;
  estAct.modIa[idx][AN_1] = 0;
  estAct.modIa[idx][AN_2] = 0;
#else
  (void)idx;
#endif
}

static void transaccion_ok(slot_t *s)
{
  if (!s->online)
    spiEstadisticas.reconexiones++;
  s->online = true;
  s->fallas = 0;
  s->ok++;
}

static void transaccion_fallida(slot_t *s)
{
  s->errores++;
  if (s->fallas < 255)
    s->fallas++;
  if (s->online && s->fallas >= SPI_FALLAS_OFFLINE) {
    s->online = false;
    spiEstadisticas.desconexiones++;
    if (s->tipo == SLOT_ENTRADAS)
      entradas_en_cero(s->indice);
  }
}

/* Asigna un modulo recien detectado al slot */
static void asignar(int slot, slot_tipo_t tipo)
{
  slot_t *s = &slots[slot];
  if (tipo == SLOT_ENTRADAS && contModulosIn < N_MODULOS) {
    s->indice = (uint8_t)contModulosIn;
    slotDeEntrada[contModulosIn++] = (int8_t)slot;
  } else if (tipo == SLOT_SALIDAS && contModulosOut < N_MODULOS) {
    s->indice = (uint8_t)contModulosOut;
    slotDeSalida[contModulosOut++] = (int8_t)slot;
  } else {
    return;
  }
  s->tipo = tipo;
  s->online = true;
  s->fallas = 0;
}

void moduleDet(void)
{
  contModulosIn = 0;
  contModulosOut = 0;
  memset(slots, 0, sizeof(slots));
  memset(slotDeEntrada, -1, sizeof(slotDeEntrada));
  memset(slotDeSalida, -1, sizeof(slotDeSalida));

  /* Los indices se asignan en orden fisico: MOD_IN_1 es el primer modulo de
   * entradas a la derecha de la CPU. */
  for (int slot = 0; slot < N_MODULOS; slot++) {
    slot_tipo_t t = detectar_slot(slot);
    if (t != SLOT_VACIO)
      asignar(slot, t);
  }
  ultimaRedeteccion = HAL_GetTick();
}

void plc_redetectar(void)
{
  for (int slot = 0; slot < N_MODULOS; slot++) {
    slot_t *s = &slots[slot];
    if (s->tipo != SLOT_VACIO && s->online)
      continue;

    slot_tipo_t t = detectar_slot(slot);
    if (t == SLOT_VACIO)
      continue;

    if (s->tipo == SLOT_VACIO) {
      /* Modulo nuevo: recibe el proximo indice libre de su tipo.
       * (Para reordenar los indices, reiniciar el CPU.) */
      asignar(slot, t);
    } else if (s->tipo == t) {
      /* El mismo tipo de modulo volvio a responder */
      transaccion_ok(s);
    }
    /* Si en el slot aparecio un modulo de otro tipo, se ignora hasta
     * reiniciar el CPU, para no mover los indices de los demas. */
  }
}

/* ---- Lectura y escritura -------------------------------------------------- */

void plc_read_inputs(void)
{
  for (int idx = 0; idx < contModulosIn; idx++) {
    int slot = slotDeEntrada[idx];
    slot_t *s = &slots[slot];
    if (!s->online)
      continue;   /* se reintenta en plc_redetectar() */

    uint8_t rx[PROTO_LARGO_ENTRADAS];
    proto_datos_t d;
    if (trans_leer(slot, PROTO_CMD_LEER, rx, sizeof(rx)) &&
        proto_leer_datos(PROTO_CMD_LEER, rx, &d)) {
      for (int i = 0; i < 8; i++)
        estAct.modId[idx][i] = (d.dig >> i) & 0x1;
      estAct.modIa[idx][AN_1] = d.ana[0];
      estAct.modIa[idx][AN_2] = d.ana[1];
      transaccion_ok(s);
      spiEstadisticas.lecturasOk++;
    } else {
      /* Se conserva la ultima lectura valida hasta que pase a offline */
      transaccion_fallida(s);
      spiEstadisticas.lecturasError++;
    }
  }
}

void plc_write_outputs(void)
{
  for (int idx = 0; idx < contModulosOut; idx++) {
    int slot = slotDeSalida[idx];
    slot_t *s = &slots[slot];
    if (!s->online)
      continue;

    proto_datos_t d = { 0, { 0, 0 } };
    for (int i = 0; i < 8; i++) {
      if (estAct.modOd[idx][i])
        d.dig |= (uint8_t)(1U << i);
    }
    d.ana[0] = estAct.modOa[idx][AN_1];
    d.ana[1] = estAct.modOa[idx][AN_2];

    uint8_t tx[PROTO_LARGO_SALIDAS];
    uint8_t cmd = PROTO_CMD_ESCRIBIR;
    uint8_t ack = 0;
    proto_armar_datos(PROTO_CMD_ESCRIBIR, &d, tx);

    /* comando -> pausa -> datos -> pausa -> confirmacion */
    cs_bajo(slot);
    HAL_StatusTypeDef st = HAL_SPI_Transmit(&hspi1, &cmd, 1, 5);
    pausa_us(PROTO_PAUSA_US);
    if (st == HAL_OK)
      st = HAL_SPI_Transmit(&hspi1, tx, sizeof(tx), 5);
    pausa_us(PROTO_PAUSA_US);
    if (st == HAL_OK)
      st = HAL_SPI_Receive(&hspi1, &ack, 1, 5);
    cs_todos_alto();

    if (st == HAL_OK && ack == PROTO_ACK) {
      transaccion_ok(s);
      spiEstadisticas.escriturasOk++;
    } else {
      transaccion_fallida(s);
      spiEstadisticas.escriturasError++;
    }
  }
}

void plc_run_cycle(void (*fuser)(void))
{
  plc_read_inputs();
  fuser();   /* la logica nunca se ejecuta dentro de una interrupcion */
  plc_write_outputs();

  if ((HAL_GetTick() - ultimaRedeteccion) >= SPI_REDETECCION_MS) {
    ultimaRedeteccion = HAL_GetTick();
    plc_redetectar();
  }
  induWatchdogRefresh();
  /* El tiempo de ciclo lo fija plc_openplc_wait_cycle() (openplc_glue.c)
     segun el INTERVAL de la TASK del programa IEC. */
}

/* ---- API para la logica del usuario --------------------------------------- */

void digWrite(int modulo, int salida, bool estado)
{
  if (modulo < 0 || modulo >= N_MODULOS || salida < 0 || salida >= 8)
    return;
  estAct.modOd[modulo][salida] = estado;
}

void anWrite(int modulo, int salida, uint16_t valor)
{
  if (modulo < 0 || modulo >= N_MODULOS || salida < 0 || salida >= 2)
    return;
  estAct.modOa[modulo][salida] = valor > ANA_MAX ? ANA_MAX : valor;
}

bool digRead(int modulo, int entrada)
{
  if (modulo < 0 || modulo >= N_MODULOS || entrada < 0 || entrada >= 8)
    return false;
  return estAct.modId[modulo][entrada];
}

uint16_t anRead(int modulo, int entrada)
{
  if (modulo < 0 || modulo >= N_MODULOS || entrada < 0 || entrada >= 2)
    return 0;
  return estAct.modIa[modulo][entrada];
}

bool modEntradasOk(int modulo)
{
  if (modulo < 0 || modulo >= contModulosIn)
    return false;
  return slots[slotDeEntrada[modulo]].online;
}

bool modSalidasOk(int modulo)
{
  if (modulo < 0 || modulo >= contModulosOut)
    return false;
  return slots[slotDeSalida[modulo]].online;
}

/* ---- Inicializacion ------------------------------------------------------- */

void induInit(void)
{
  HAL_Init();
  SystemClock_Config();
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_AFIO_REMAP_SWJ_NOJTAG();
  MX_GPIO_Init();
  MX_SPI1_Init();
  MX_USART3_UART_Init();
  cs_todos_alto();
  HAL_Delay(1000);      /* los modulos terminan de arrancar */
  moduleDet();
  induWatchdogInit();   /* despues de la deteccion (HAL_Delay largo) */
}
