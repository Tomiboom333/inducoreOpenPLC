/*
 * Test en PC (sin hardware) del maestro Modbus RTU: mbus_funcs.c + mbus_parser.c
 * + modbus_crc.c. Simula la UART, el pin TX_EN y el reloj.
 * Se corre con:  python test_pc/correr_test.py
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mbus_funcs.h"

/* ---- HAL falso ---------------------------------------------------------- */
USART_TypeDef fake_usart3;
GPIO_TypeDef fake_gpioa;
UART_HandleTypeDef huart3 = { USART3, { 115200 } };

static uint32_t reloj = 1000;
uint32_t HAL_GetTick(void) { return reloj++; }   /* avanza 1 ms por llamada */

static int tx_en = 0, tx_en_al_transmitir = 0;
void HAL_GPIO_WritePin(GPIO_TypeDef *p, uint16_t pin, GPIO_PinState s)
{
    (void)p;
    if (pin == TX_EN_Pin) tx_en = (s == GPIO_PIN_SET);
}

static uint8_t tx[256]; static uint16_t tx_n = 0;
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, const uint8_t *d, uint16_t n, uint32_t t)
{
    (void)h; (void)t;
    tx_en_al_transmitir = tx_en;
    memcpy(tx, d, n); tx_n = n;
    return HAL_OK;
}

static int rx_armada = 0;
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t n)
{
    (void)h; (void)d; (void)n; rx_armada = 1; return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h) { (void)h; rx_armada = 0; return HAL_OK; }

/* Simula la llegada de una trama del esclavo (lo que haria la interrupcion) */
static void llega(const uint8_t *trama, uint16_t n)
{
    memcpy(RxData, trama, n);
    rx_armada = 0;
    HAL_UARTEx_RxEventCallback(&huart3, n);
}

/* CRC Modbus calculado bit a bit (independiente de la tabla de modbus_crc.c) */
static uint16_t crc_ref(const uint8_t *b, int n)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < n; i++) {
        crc ^= b[i];
        for (int k = 0; k < 8; k++)
            crc = (crc & 1) ? (uint16_t)((crc >> 1) ^ 0xA001) : (uint16_t)(crc >> 1);
    }
    return crc;
}
static uint16_t con_crc(uint8_t *b, int n)
{
    uint16_t c = crc_ref(b, n);
    b[n] = (uint8_t)(c & 0xFF); b[n + 1] = (uint8_t)(c >> 8);
    return (uint16_t)(n + 2);
}

/* ---- Mini framework ----------------------------------------------------- */
static int fallas = 0, chequeos = 0;
#define CHECK(c, msg) do { chequeos++; if (!(c)) { printf("  FALLA: %s\n", msg); fallas++; } } while (0)

static int llamadas_cb = 0;
static void cb(const mb_respuesta_t *r) { (void)r; llamadas_cb++; }

int main(void)
{
    mb_set_callback(cb);

    printf("1) CRC de la tabla contra calculo bit a bit\n");
    srand(1);
    int dif = 0;
    for (int i = 0; i < 2000; i++) {
        uint8_t b[64]; int n = 1 + rand() % 60;
        for (int k = 0; k < n; k++) b[k] = (uint8_t)rand();
        if (crc16(b, (uint16_t)n) != crc_ref(b, n)) dif++;
    }
    CHECK(dif == 0, "crc16 coincide con la referencia en 2000 tramas al azar");

    printf("2) Trama enviada por readHoldingRegisters(1, 0, 10)\n");
    const uint8_t esperado[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A, 0xC5, 0xCD };
    CHECK(readHoldingRegisters(1, 0, 10) == MB_PENDIENTE, "devuelve PENDIENTE");
    CHECK(tx_n == 8 && memcmp(tx, esperado, 8) == 0, "trama = 01 03 00 00 00 0A C5 CD (ejemplo clasico)");
    CHECK(tx_en_al_transmitir == 1 && tx_en == 0, "TX_EN alto al transmitir y bajo despues");
    CHECK(rx_armada && mb_ocupado(), "recepcion armada y esperando");
    CHECK(readCoils(1, 0, 8) == MB_ERR_OCUPADO, "segunda peticion rechazada mientras espera");

    printf("3) Respuesta valida de 10 registros\n");
    uint8_t r[64] = { 0x01, 0x03, 20 };
    for (int i = 0; i < 10; i++) { r[3 + 2 * i] = 0x12; r[4 + 2 * i] = (uint8_t)i; }
    llega(r, con_crc(r, 23));
    mb_procesar();
    CHECK(mb_estado() == MB_OK, "estado OK");
    CHECK(mbRespuesta.registros[0] == 0x1200 && mbRespuesta.registros[9] == 0x1209, "registros guardados");
    CHECK(!mb_ocupado() && llamadas_cb == 1 && uart_rx_flag == 1, "libre, callback llamado y flag en 1");
    CHECK(mbRespuesta.largo_trama == 25 && Data[0] == 0x01, "trama cruda guardada");

    printf("4) Respuesta con CRC incorrecto\n");
    readHoldingRegisters(1, 0, 10);
    r[5] ^= 0xFF;   /* se corrompe un dato, el CRC queda viejo */
    llega(r, 25);
    mb_procesar();
    CHECK(mb_estado() == MB_ERR_CRC, "detecta error de CRC");
    CHECK(mbRespuesta.registros[0] == 0x1200, "con CRC malo no se pisan los registros guardados");

    printf("5) Excepcion del esclavo (direccion ilegal)\n");
    readHoldingRegisters(1, 500, 2);
    uint8_t ex[8] = { 0x01, 0x83, 0x02 };
    llega(ex, con_crc(ex, 3));
    mb_procesar();
    CHECK(mb_estado() == MB_ERR_EXCEPCION && mbRespuesta.excepcion == 2, "excepcion 02 guardada");

    printf("6) Esclavo que no responde\n");
    readHoldingRegisters(7, 0, 1);
    int vueltas = 0;
    while (mb_ocupado() && vueltas < 10000) { mb_procesar(); vueltas++; }
    CHECK(mb_estado() == MB_ERR_TIMEOUT, "timeout");
    CHECK(vueltas >= 50 && vueltas <= 110, "timeout cerca de 100 ms");

    printf("7) Respuesta de otro esclavo\n");
    readHoldingRegisters(2, 0, 1);
    uint8_t otro[8] = { 0x03, 0x03, 0x02, 0x00, 0x05 };
    llega(otro, con_crc(otro, 5));
    mb_procesar();
    CHECK(mb_estado() == MB_ERR_TRAMA, "rechaza trama de otro esclavo");

    printf("8) Lectura de 10 coils\n");
    readCoils(1, 0, 10);
    uint8_t co[8] = { 0x01, 0x01, 0x02, 0x05, 0x02 };   /* bits 0,2 y 9 en 1 */
    llega(co, con_crc(co, 5));
    mb_procesar();
    CHECK(mb_estado() == MB_OK, "estado OK");
    CHECK(mb_bit(&mbRespuesta, 0) && !mb_bit(&mbRespuesta, 1) && mb_bit(&mbRespuesta, 2) &&
          mb_bit(&mbRespuesta, 9) && !mb_bit(&mbRespuesta, 8), "bits correctos");

    printf("9) Escritura de un coil (eco)\n");
    writeSingleCoil(1, 4, 1);
    CHECK(tx[4] == 0xFF && tx[5] == 0x00, "envia 0xFF00 para ON");
    uint8_t eco[8]; memcpy(eco, tx, 8);
    llega(eco, 8);
    mb_procesar();
    CHECK(mb_estado() == MB_OK, "eco aceptado");

    printf("10) Escritura de 3 registros\n");
    uint16_t vals[3] = { 1, 2, 3 };
    writeMultipleRegisters(1, 10, vals, 3);
    CHECK(tx_n == 9 + 6 && tx[6] == 6 && tx[12] == 3, "trama 0x10 armada");
    uint8_t ok10[8] = { 0x01, 0x10, 0x00, 0x0A, 0x00, 0x03 };
    llega(ok10, con_crc(ok10, 6));
    mb_procesar();
    CHECK(mb_estado() == MB_OK, "respuesta 0x10 aceptada");

    printf("11) Broadcast y parametros invalidos\n");
    CHECK(writeSingleRegister(0, 1, 99) == MB_OK && !mb_ocupado(), "broadcast no espera respuesta");
    CHECK(readHoldingRegisters(1, 0, 200) == MB_ERR_PARAM, "mas de 125 registros");
    CHECK(readCoils(0, 0, 1) == MB_ERR_PARAM, "lectura a broadcast");

    printf("\nEstadisticas: peticiones=%u ok=%u timeouts=%u crc=%u excepciones=%u trama=%u\n",
           (unsigned)mbEstadisticas.peticiones, (unsigned)mbEstadisticas.ok,
           (unsigned)mbEstadisticas.timeouts, (unsigned)mbEstadisticas.errores_crc,
           (unsigned)mbEstadisticas.excepciones, (unsigned)mbEstadisticas.errores_trama);
    printf("%d chequeos, %s\n", chequeos, fallas ? "HAY FALLAS" : "TODO OK");
    return fallas;
}
