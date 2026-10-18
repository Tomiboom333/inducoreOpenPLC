#ifndef INDUPROTO_H
#define INDUPROTO_H

/*
 * induproto.h - Protocolo del backplane SPI de InduCore.
 *
 * ESTE ARCHIVO ES COMUN A LOS TRES FIRMWARES (CPU, entradas y salidas).
 * Tiene que ser identico en Modulo_IN, Modulo_OUT e inducoreOpenPLC:
 * si se cambia en uno, copiarlo a los otros dos y subir PROTO_VERSION.
 *
 * Descripcion completa en docs/PROTOCOLO.md.
 *
 * Todas las transacciones son iniciadas por el CPU (maestro):
 *
 *   NSS bajo -> comando (1 byte) -> pausa ~100 us -> datos -> NSS alto
 *
 * La pausa le da tiempo al esclavo (que atiende el SPI por polling) a
 * procesar el comando y cargar la respuesta.
 *
 *  Deteccion   CPU: [0x48]            Esclavo: [ID][VERSION][CRC]
 *  Leer        CPU: [0x01]            Esclavo: [DIG][AN1 H][AN1 L][AN2 H][AN2 L][CRC]
 *  Escribir    CPU: [0x02][DIG][AO1 H][AO1 L][AO2 H][AO2 L][CRC]  (pausa)  Esclavo: [ACK/NAK]
 *
 * El CRC es CRC-8 (polinomio 0x07, valor inicial 0x00) calculado sobre el
 * comando y todos los bytes de datos de la trama.
 * Los valores analogicos son de 12 bits (0..4095), byte alto primero.
 */

#include <stdint.h>
#include <stdbool.h>

#define PROTO_VERSION        2

/* Comandos (CPU -> esclavo) */
#define PROTO_CMD_DETECCION  0x48
#define PROTO_CMD_LEER       0x01
#define PROTO_CMD_ESCRIBIR   0x02

/* Identificadores de modulo (respuesta a la deteccion) */
#define PROTO_ID_ENTRADAS    0x12
#define PROTO_ID_SALIDAS     0x02

/* Respuesta del modulo de salidas a una escritura */
#define PROTO_ACK            0x06   /* trama correcta, salidas aplicadas */
#define PROTO_NAK            0x15   /* CRC incorrecto, salidas sin cambios */

/* Pausa entre el comando y los datos (lado CPU) */
#define PROTO_PAUSA_US       100

/* Tamanios (sin contar el byte de comando) */
#define PROTO_LARGO_DETECCION  3   /* ID, VERSION, CRC */
#define PROTO_LARGO_ENTRADAS   6   /* DIG, AN1 H, AN1 L, AN2 H, AN2 L, CRC */
#define PROTO_LARGO_SALIDAS    6   /* DIG, AO1 H, AO1 L, AO2 H, AO2 L, CRC */

#define PROTO_ANA_MAX        4095  /* 12 bits */
#define PROTO_N_DIG          8
#define PROTO_N_ANA          2

/* Datos de un modulo (entradas o salidas) ya decodificados */
typedef struct {
    uint8_t  dig;                  /* bit i = canal digital i (0..7) */
    uint16_t ana[PROTO_N_ANA];     /* 0..4095 */
} proto_datos_t;

/* ---- CRC-8, polinomio 0x07 ------------------------------------------------ */

static inline uint8_t proto_crc8_byte(uint8_t crc, uint8_t b)
{
    crc ^= b;
    for (int i = 0; i < 8; i++)
        crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
    return crc;
}

/* CRC de [cmd][datos...] */
static inline uint8_t proto_crc8(uint8_t cmd, const uint8_t *datos, uint8_t n)
{
    uint8_t crc = proto_crc8_byte(0x00, cmd);
    for (uint8_t i = 0; i < n; i++)
        crc = proto_crc8_byte(crc, datos[i]);
    return crc;
}

/* ---- Armado y lectura de tramas ------------------------------------------- */

/* Respuesta a la deteccion: buf[3] */
static inline void proto_armar_deteccion(uint8_t id, uint8_t *buf)
{
    buf[0] = id;
    buf[1] = PROTO_VERSION;
    buf[2] = proto_crc8(PROTO_CMD_DETECCION, buf, 2);
}

/* Devuelve el ID del modulo, o 0 si la respuesta no es valida
 * (CRC incorrecto, version distinta o ID desconocido). */
static inline uint8_t proto_leer_deteccion(const uint8_t *buf)
{
    if (proto_crc8(PROTO_CMD_DETECCION, buf, 2) != buf[2])
        return 0;
    if (buf[1] != PROTO_VERSION)
        return 0;
    if (buf[0] != PROTO_ID_ENTRADAS && buf[0] != PROTO_ID_SALIDAS)
        return 0;
    return buf[0];
}

/* Trama de datos (entradas o salidas): buf[6]. cmd = PROTO_CMD_LEER o PROTO_CMD_ESCRIBIR */
static inline void proto_armar_datos(uint8_t cmd, const proto_datos_t *d, uint8_t *buf)
{
    buf[0] = d->dig;
    for (int i = 0; i < PROTO_N_ANA; i++) {
        uint16_t v = d->ana[i] > PROTO_ANA_MAX ? PROTO_ANA_MAX : d->ana[i];
        buf[1 + 2 * i] = (uint8_t)(v >> 8);
        buf[2 + 2 * i] = (uint8_t)(v & 0xFF);
    }
    buf[5] = proto_crc8(cmd, buf, 5);
}

/* Devuelve true si la trama es valida y la copia en d */
static inline bool proto_leer_datos(uint8_t cmd, const uint8_t *buf, proto_datos_t *d)
{
    if (proto_crc8(cmd, buf, 5) != buf[5])
        return false;
    for (int i = 0; i < PROTO_N_ANA; i++) {
        uint16_t v = (uint16_t)((buf[1 + 2 * i] << 8) | buf[2 + 2 * i]);
        if (v > PROTO_ANA_MAX)
            return false;
    }
    d->dig = buf[0];
    for (int i = 0; i < PROTO_N_ANA; i++)
        d->ana[i] = (uint16_t)((buf[1 + 2 * i] << 8) | buf[2 + 2 * i]);
    return true;
}

#endif /* INDUPROTO_H */
