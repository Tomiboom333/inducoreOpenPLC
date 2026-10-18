/*
 * Test en PC del protocolo del backplane (induproto.h): CRC-8, armado y
 * lectura de tramas, rechazo de tramas corruptas o de un modulo ausente.
 */
#include <stdio.h>
#include <string.h>
#include "induproto.h"

static int fallas = 0, chequeos = 0;
#define CHECK(c, msg) do { chequeos++; if (!(c)) { printf("  FALLA: %s\n", msg); fallas++; } } while (0)

int main(void)
{
    printf("1) CRC-8 (polinomio 0x07) con el vector de referencia\n");
    /* CRC-8/SMBUS de "123456789" = 0xF4 */
    const uint8_t ref[] = "23456789";
    CHECK(proto_crc8('1', ref, 8) == 0xF4, "CRC-8 de \"123456789\" = 0xF4");

    printf("2) Deteccion\n");
    uint8_t det[3];
    proto_armar_deteccion(PROTO_ID_ENTRADAS, det);
    CHECK(proto_leer_deteccion(det) == PROTO_ID_ENTRADAS, "ID de entradas");
    proto_armar_deteccion(PROTO_ID_SALIDAS, det);
    CHECK(proto_leer_deteccion(det) == PROTO_ID_SALIDAS, "ID de salidas");
    det[0] ^= 0x10;
    CHECK(proto_leer_deteccion(det) == 0, "ID corrupto rechazado");
    uint8_t vacio00[3] = { 0, 0, 0 }, vacioFF[3] = { 0xFF, 0xFF, 0xFF };
    CHECK(proto_leer_deteccion(vacio00) == 0 && proto_leer_deteccion(vacioFF) == 0,
          "slot vacio (MISO en 0x00 o 0xFF) no se detecta como modulo");
    uint8_t viejo[3] = { PROTO_ID_ENTRADAS, 0, 0 };
    CHECK(proto_leer_deteccion(viejo) == 0, "firmware viejo (sin CRC/version) no se acepta");

    printf("3) Tramas de datos\n");
    proto_datos_t d = { 0xA5, { 4095, 1234 } }, r;
    uint8_t buf[6];
    proto_armar_datos(PROTO_CMD_LEER, &d, buf);
    CHECK(buf[1] == 0x0F && buf[2] == 0xFF && buf[3] == 0x04 && buf[4] == 0xD2, "12 bits, byte alto primero");
    CHECK(proto_leer_datos(PROTO_CMD_LEER, buf, &r) && r.dig == 0xA5 && r.ana[0] == 4095 && r.ana[1] == 1234,
          "ida y vuelta sin perdida");
    CHECK(!proto_leer_datos(PROTO_CMD_ESCRIBIR, buf, &r), "el CRC incluye el comando");

    int detectados = 0, total = 0;
    for (int byte = 0; byte < 6; byte++)
        for (int bit = 0; bit < 8; bit++) {
            uint8_t c[6]; memcpy(c, buf, 6); c[byte] ^= (uint8_t)(1 << bit);
            total++;
            if (!proto_leer_datos(PROTO_CMD_LEER, c, &r)) detectados++;
        }
    CHECK(detectados == total, "detecta todos los errores de 1 bit (48/48)");

    detectados = 0; total = 0;
    for (int a = 0; a < 48; a++)
        for (int b = a + 1; b < 48; b++) {
            uint8_t c[6]; memcpy(c, buf, 6);
            c[a / 8] ^= (uint8_t)(1 << (a % 8)); c[b / 8] ^= (uint8_t)(1 << (b % 8));
            total++;
            if (!proto_leer_datos(PROTO_CMD_LEER, c, &r)) detectados++;
        }
    CHECK(detectados == total, "detecta todos los errores de 2 bits");

    uint8_t z[6] = { 0 }, f[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
    CHECK(!proto_leer_datos(PROTO_CMD_LEER, z, &r) && !proto_leer_datos(PROTO_CMD_LEER, f, &r),
          "MISO flotante (todo 0x00 o 0xFF) se rechaza");

    proto_datos_t grande = { 0, { 9999, 0 } };
    proto_armar_datos(PROTO_CMD_ESCRIBIR, &grande, buf);
    CHECK(proto_leer_datos(PROTO_CMD_ESCRIBIR, buf, &r) && r.ana[0] == 4095, "valores > 4095 se recortan al armar");

    printf("%d chequeos, %s\n", chequeos, fallas ? "HAY FALLAS" : "TODO OK");
    return fallas;
}
