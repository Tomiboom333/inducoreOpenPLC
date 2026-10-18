/*
 * Test en PC del Modbus esclavo (mbus_esclavo.c + mbus_mapa.c): lecturas,
 * escrituras, excepciones, CRC, direccion y broadcast, contra el mapa de
 * registros documentado en mbus_esclavo.h.
 */
#include <stdio.h>
#include <string.h>
#include "mbus_esclavo.h"
#include "mbus_parser.h"
#include "induSPI.h"
#include "openplc_glue.h"

/* Datos que normalmente definen induSPI.c y openplc_glue.c */
USART_TypeDef fake_usart1, fake_usart3;
GPIO_TypeDef fake_gpioa;
estAct_t estAct;
slot_t slots[N_MODULOS];
spi_estadisticas_t spiEstadisticas;
int contModulosIn = 0, contModulosOut = 0;
uint16_t plc_mw[PLC_N_MW];
volatile uint32_t plc_ciclos = 0, plc_ciclos_excedidos = 0, plc_periodo_ms = 20;

static int fallas = 0, chequeos = 0;
#define CHECK(c, msg) do { chequeos++; if (!(c)) { printf("  FALLA: %s\n", msg); fallas++; } } while (0)

static uint8_t req[256], resp[256];

static uint16_t con_crc(uint8_t *b, uint16_t n)
{
    uint16_t c = crc16(b, n);
    b[n] = (uint8_t)(c & 0xFF); b[n + 1] = (uint8_t)(c >> 8);
    return (uint16_t)(n + 2);
}

/* Arma [dir][fc][a hi][a lo][b hi][b lo] + CRC y lo procesa */
static uint16_t pedir(uint8_t dir, uint8_t fc, uint16_t a, uint16_t b)
{
    req[0] = dir; req[1] = fc;
    req[2] = (uint8_t)(a >> 8); req[3] = (uint8_t)a;
    req[4] = (uint8_t)(b >> 8); req[5] = (uint8_t)b;
    return mbs_procesar_trama(1, req, con_crc(req, 6), resp);
}
static uint16_t reg(int i) { return (uint16_t)((resp[3 + 2 * i] << 8) | resp[4 + 2 * i]); }
static bool resp_valida(uint16_t n) { return n >= 4 && mb_crc_valido(resp, n); }

int main(void)
{
    /* Estado de ejemplo */
    estAct.modId[0][0] = true; estAct.modId[0][7] = true; estAct.modId[1][2] = true;
    estAct.modIa[0][0] = 1234; estAct.modIa[4][1] = 4095;
    estAct.modOa[2][1] = 777;
    estAct.modOd[0][SAL_1] = true;
    contModulosIn = 2; contModulosOut = 1;
    slots[0] = (slot_t){ SLOT_ENTRADAS, 0, true, 0, 0, 0 };
    slots[1] = (slot_t){ SLOT_SALIDAS, 0, true, 0, 0, 0 };
    slots[2] = (slot_t){ SLOT_ENTRADAS, 1, false, 3, 0, 0 };
    plc_ciclos = 0x00012345;

    printf("1) Discrete inputs (02) = %%IX\n");
    uint16_t n = pedir(1, 0x02, 0, 16);
    CHECK(resp_valida(n) && n == 7 && resp[2] == 2, "16 bits en 2 bytes");
    CHECK(resp[3] == 0x81 && resp[4] == 0x04, "%IX0.0, %IX0.7 y %IX1.2");

    printf("2) Coils (01) = %%QX, en orden de salida fisica\n");
    n = pedir(1, 0x01, 0, 8);
    CHECK(resp_valida(n) && resp[3] == 0x01, "%QX0.0 = SAL_1");

    printf("3) Input registers (04): analogicas y diagnostico\n");
    n = pedir(1, 0x04, 0, 10);
    CHECK(resp_valida(n) && reg(0) == 1234 && reg(9) == 4095, "%IW0 y %IW9");
    n = pedir(1, 0x04, 100, 12);
    CHECK(resp_valida(n) && reg(0) == PROTO_VERSION && reg(1) == 2 && reg(2) == 1, "version y cantidad de modulos");
    CHECK(reg(3) == 0x01 && reg(4) == 0x01, "mascaras online (MOD_IN_2 offline)");
    CHECK(reg(5) == 20 && reg(6) == 0x2345 && reg(7) == 0x0001, "periodo y contador de ciclos");

    printf("4) Holding (03/06/10): salidas analogicas y %%MW\n");
    n = pedir(1, 0x03, 5, 1);
    CHECK(resp_valida(n) && reg(0) == 777, "%QW5");
    n = pedir(1, 0x06, 0, 2500);
    CHECK(resp_valida(n) && n == 8 && memcmp(resp, req, 6) == 0 && estAct.modOa[0][0] == 2500, "06 escribe %QW0 y responde eco");
    n = pedir(1, 0x06, 1024 + 3, 0xBEEF);
    CHECK(resp_valida(n) && plc_mw[3] == 0xBEEF, "06 escribe %MW3 (holding 1027)");
    req[0] = 1; req[1] = 0x10; req[2] = 0x04; req[3] = 0x00; req[4] = 0; req[5] = 2; req[6] = 4;
    req[7] = 0x00; req[8] = 0x11; req[9] = 0x00; req[10] = 0x22;
    n = mbs_procesar_trama(1, req, con_crc(req, 11), resp);
    CHECK(resp_valida(n) && plc_mw[0] == 0x11 && plc_mw[1] == 0x22, "10 escribe %MW0..1");

    printf("5) Coils (05/0F)\n");
    n = pedir(1, 0x05, 8 + 1, 0xFF00);
    CHECK(resp_valida(n) && estAct.modOd[1][SAL_2], "05 enciende %QX1.1");
    req[0] = 1; req[1] = 0x0F; req[2] = 0; req[3] = 16; req[4] = 0; req[5] = 10; req[6] = 2; req[7] = 0x05; req[8] = 0x02;
    n = mbs_procesar_trama(1, req, con_crc(req, 9), resp);
    CHECK(resp_valida(n) && estAct.modOd[2][SAL_1] && !estAct.modOd[2][SAL_2] && estAct.modOd[2][SAL_3] &&
          estAct.modOd[3][SAL_2], "0F escribe 10 coils desde %QX2.0");

    printf("6) Excepciones\n");
    n = pedir(1, 0x03, 50, 1);
    CHECK(resp_valida(n) && resp[1] == 0x83 && resp[2] == MBS_EXC_DIRECCION, "direccion inexistente -> 02");
    n = pedir(1, 0x04, 8, 4);
    CHECK(resp[1] == 0x84 && resp[2] == MBS_EXC_DIRECCION, "rango que se pasa del final -> 02");
    n = pedir(1, 0x06, 0, 5000);
    CHECK(resp[1] == 0x86 && resp[2] == MBS_EXC_VALOR && estAct.modOa[0][0] == 2500, "valor > 4095 -> 03, sin cambios");
    n = pedir(1, 0x05, 0, 0x1234);
    CHECK(resp[1] == 0x85 && resp[2] == MBS_EXC_VALOR, "coil con valor invalido -> 03");
    n = pedir(1, 0x03, 0, 200);
    CHECK(resp[1] == 0x83 && resp[2] == MBS_EXC_VALOR, "cantidad > 125 -> 03");
    n = pedir(1, 0x2B, 0, 0);
    CHECK(resp_valida(n) && resp[1] == 0xAB && resp[2] == MBS_EXC_FUNCION, "funcion no soportada -> 01");

    printf("7) Tramas que no se responden\n");
    n = pedir(2, 0x03, 0, 1);
    CHECK(n == 0, "otra direccion");
    req[0] = 1; req[1] = 3; req[2] = 0; req[3] = 0; req[4] = 0; req[5] = 1; req[6] = 0x12; req[7] = 0x34;
    CHECK(mbs_procesar_trama(1, req, 8, resp) == 0 && mbsEstadisticas.errores_crc == 1, "CRC incorrecto");
    n = pedir(0, 0x06, 1025, 42);
    CHECK(n == 0 && plc_mw[1] == 42, "broadcast: se aplica pero no se responde");
    n = pedir(0, 0x03, 0, 1);
    CHECK(n == 0, "lectura broadcast: se ignora");

    printf("%d chequeos, %s\n", chequeos, fallas ? "HAY FALLAS" : "TODO OK");
    return fallas;
}
