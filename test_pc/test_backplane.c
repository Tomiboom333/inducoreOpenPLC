/*
 * Test en PC del backplane del CPU (induSPI.c) con modulos de entradas y
 * salidas simulados que responden igual que el firmware de Modulo_IN y
 * Modulo_OUT: deteccion, orden de los slots, lectura, escritura con ACK,
 * tramas corruptas, desconexion, reconexion y modulos nuevos.
 */
#include <stdio.h>
#include <string.h>
#include "induSPI.h"

/* ---- HAL falso ------------------------------------------------------------ */
USART_TypeDef fake_usart1, fake_usart3;
GPIO_TypeDef fake_gpioa;
SPI_HandleTypeDef hspi1;
static uint32_t reloj = 0;
uint32_t HAL_GetTick(void) { return reloj; }
void HAL_Init(void) {}
void HAL_Delay(uint32_t ms) { reloj += ms; }
HAL_SPI_StateTypeDef HAL_SPI_GetState(SPI_HandleTypeDef *h) { (void)h; return HAL_SPI_STATE_READY; }
void SystemClock_Config(void) {}
void MX_GPIO_Init(void) {}
void MX_SPI1_Init(void) {}
void MX_USART3_UART_Init(void) {}
void induWatchdogInit(void) {}
static int refrescos = 0;
void induWatchdogRefresh(void) { refrescos++; }

/* ---- Modulos simulados ---------------------------------------------------- */
typedef struct {
    slot_tipo_t tipo;
    bool conectado;
    bool corromper;          /* da vuelta un bit de la proxima respuesta */
    proto_datos_t entradas;  /* lo que mide un modulo de entradas */
    proto_datos_t salidas;   /* lo ultimo que aplico un modulo de salidas */
    int escrituras;
} esclavo_t;
static esclavo_t esc[N_MODULOS];

/* slot 0 = primer modulo a la derecha de la CPU = PA4 (NSS5) */
static int slot_de_pin(uint16_t pin)
{
    switch (pin) {
    case SPI1_NSS5_Pin: return 0;
    case SPI1_NSS4_Pin: return 1;
    case SPI1_NSS3_Pin: return 2;
    case SPI1_NSS2_Pin: return 3;
    case SPI1_NSS1_Pin: return 4;
    }
    return -1;
}

static int activo = -1, fase = 0;
static uint8_t cmd = 0, ack = 0;

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState st)
{
    (void)port;
    int s = slot_de_pin(pin);
    if (s < 0) return;
    if (st == GPIO_PIN_RESET) { activo = s; fase = 0; }
    else if (activo == s) activo = -1;
}

HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *h, uint8_t *d, uint16_t n, uint32_t t)
{
    (void)h; (void)t;
    if (activo < 0 || !esc[activo].conectado) return HAL_OK;   /* nadie escucha */
    esclavo_t *e = &esc[activo];
    if (fase == 0) { cmd = d[0]; fase = 1; return HAL_OK; }
    if (fase == 1 && cmd == PROTO_CMD_ESCRIBIR && e->tipo == SLOT_SALIDAS && n == PROTO_LARGO_SALIDAS) {
        proto_datos_t r;
        if (proto_leer_datos(PROTO_CMD_ESCRIBIR, d, &r)) { e->salidas = r; e->escrituras++; ack = PROTO_ACK; }
        else ack = PROTO_NAK;
        fase = 2;
    }
    return HAL_OK;
}

HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *h, uint8_t *d, uint16_t n, uint32_t t)
{
    (void)h; (void)t;
    if (activo < 0 || !esc[activo].conectado) { memset(d, 0xFF, n); return HAL_OK; }  /* MISO flotante */
    esclavo_t *e = &esc[activo];
    memset(d, 0xFF, n);
    if (cmd == PROTO_CMD_DETECCION && n == PROTO_LARGO_DETECCION)
        proto_armar_deteccion(e->tipo == SLOT_ENTRADAS ? PROTO_ID_ENTRADAS : PROTO_ID_SALIDAS, d);
    else if (cmd == PROTO_CMD_LEER && e->tipo == SLOT_ENTRADAS && n == PROTO_LARGO_ENTRADAS)
        proto_armar_datos(PROTO_CMD_LEER, &e->entradas, d);
    else if (cmd == PROTO_CMD_ESCRIBIR && fase == 2 && n == 1)
        d[0] = ack;
    if (e->corromper) { d[0] ^= 0x04; e->corromper = false; }
    return HAL_OK;
}

static void conectar(int slot, slot_tipo_t tipo) { esc[slot].tipo = tipo; esc[slot].conectado = true; }
static void sin_logica(void) {}

static int fallas = 0, chequeos = 0;
#define CHECK(c, msg) do { chequeos++; if (!(c)) { printf("  FALLA: %s\n", msg); fallas++; } } while (0)

int main(void)
{
    printf("1) Deteccion en orden fisico (slot 0 = PA4)\n");
    conectar(0, SLOT_ENTRADAS);
    conectar(1, SLOT_SALIDAS);
    /* slot 2 vacio */
    conectar(3, SLOT_ENTRADAS);
    conectar(4, SLOT_SALIDAS);
    induInit();
    CHECK(contModulosIn == 2 && contModulosOut == 2, "2 de entradas y 2 de salidas");
    CHECK(slots[0].tipo == SLOT_ENTRADAS && slots[0].indice == MOD_IN_1, "slot 0 -> MOD_IN_1");
    CHECK(slots[3].indice == MOD_IN_2 && slots[4].indice == MOD_OUT_2, "slot 3 -> MOD_IN_2, slot 4 -> MOD_OUT_2");
    CHECK(slots[2].tipo == SLOT_VACIO, "slot 2 vacio");

    printf("2) Lectura de entradas (12 bits)\n");
    esc[0].entradas = (proto_datos_t){ 0x81, { 100, 4000 } };
    esc[3].entradas = (proto_datos_t){ 0x02, { 4095, 0 } };
    plc_read_inputs();
    CHECK(digRead(MOD_IN_1, ENT_1) && digRead(MOD_IN_1, ENT_8) && !digRead(MOD_IN_1, ENT_2), "digitales MOD_IN_1");
    CHECK(anRead(MOD_IN_1, AN_1) == 100 && anRead(MOD_IN_1, AN_2) == 4000, "analogicas MOD_IN_1");
    CHECK(digRead(MOD_IN_2, ENT_2) && anRead(MOD_IN_2, AN_1) == 4095, "MOD_IN_2");

    printf("3) Escritura de salidas con confirmacion\n");
    digWrite(MOD_OUT_2, SAL_1, HIGH);
    anWrite(MOD_OUT_1, AN_1, 3000);
    anWrite(MOD_OUT_1, AN_2, 5000);   /* se recorta a 4095 */
    plc_write_outputs();
    CHECK(esc[4].salidas.dig == (1 << SAL_1), "SAL_1 llega como bit 3");
    CHECK(esc[1].salidas.ana[0] == 3000 && esc[1].salidas.ana[1] == 4095, "analogicas de salida (y recorte)");
    CHECK(spiEstadisticas.escriturasOk == 2, "2 escrituras confirmadas con ACK");

    printf("4) Trama corrupta: se conserva la ultima lectura\n");
    esc[0].entradas.ana[0] = 999;
    esc[0].corromper = true;
    plc_read_inputs();
    CHECK(anRead(MOD_IN_1, AN_1) == 100 && spiEstadisticas.lecturasError == 1, "lectura rechazada por CRC");
    CHECK(modEntradasOk(MOD_IN_1), "una falla sola no lo desconecta");
    plc_read_inputs();
    CHECK(anRead(MOD_IN_1, AN_1) == 999, "la siguiente lectura buena se toma");

    printf("5) Modulo de entradas desconectado\n");
    esc[3].conectado = false;
    for (int i = 0; i < SPI_FALLAS_OFFLINE; i++) plc_read_inputs();
    CHECK(!modEntradasOk(MOD_IN_2) && spiEstadisticas.desconexiones == 1, "pasa a offline");
    CHECK(!digRead(MOD_IN_2, ENT_2) && anRead(MOD_IN_2, AN_1) == 0, "sus entradas quedan en 0");
    CHECK(modEntradasOk(MOD_IN_1), "los demas siguen");

    printf("6) Reconexion automatica\n");
    esc[3].conectado = true;
    esc[3].entradas = (proto_datos_t){ 0x10, { 1, 2 } };
    reloj += 500;
    plc_run_cycle(sin_logica);
    CHECK(!modEntradasOk(MOD_IN_2), "antes de SPI_REDETECCION_MS sigue offline");
    reloj += 600;
    plc_run_cycle(sin_logica);
    CHECK(modEntradasOk(MOD_IN_2) && spiEstadisticas.reconexiones == 1, "vuelve a responder");
    plc_run_cycle(sin_logica);
    CHECK(digRead(MOD_IN_2, ENT_5) && anRead(MOD_IN_2, AN_2) == 2, "y se leen sus entradas");

    printf("7) Modulo nuevo en un slot vacio\n");
    conectar(2, SLOT_SALIDAS);
    reloj += 1100;
    plc_run_cycle(sin_logica);
    CHECK(contModulosOut == 3 && slots[2].indice == MOD_OUT_3, "recibe el proximo indice (MOD_OUT_3)");
    CHECK(slots[1].indice == MOD_OUT_1 && slots[4].indice == MOD_OUT_2, "los indices existentes no cambian");
    digWrite(MOD_OUT_3, SAL_8, HIGH);
    plc_write_outputs();
    CHECK(esc[2].salidas.dig == (1 << SAL_8), "se le escribe");

    printf("8) Modulo de salidas desconectado\n");
    esc[1].conectado = false;
    for (int i = 0; i < SPI_FALLAS_OFFLINE; i++) plc_write_outputs();
    CHECK(!modSalidasOk(MOD_OUT_1) && modSalidasOk(MOD_OUT_2), "sin ACK pasa a offline");

    printf("9) Watchdog\n");
    CHECK(refrescos >= 4, "plc_run_cycle refresca el watchdog");

    printf("%d chequeos, %s\n", chequeos, fallas ? "HAY FALLAS" : "TODO OK");
    return fallas;
}
