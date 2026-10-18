#ifndef MBUS_PARSER_H
#define MBUS_PARSER_H

/*
 * Interprete de respuestas Modbus RTU (maestro).
 * Codigo C puro, sin HAL: valida CRC, direccion, funcion y longitud,
 * y guarda los datos de la respuesta en una estructura.
 */

#include <stdint.h>
#include <stdbool.h>

#define MB_MAX_REGISTROS  125   /* maximo por lectura (funciones 03 y 04) */
#define MB_MAX_BITS       2000  /* maximo por lectura (funciones 01 y 02) */
#define MB_MAX_TRAMA      256

/* Codigos de funcion */
#define MB_FC_LEER_COILS              0x01
#define MB_FC_LEER_ENTRADAS           0x02
#define MB_FC_LEER_HOLDING            0x03
#define MB_FC_LEER_INPUT_REGS         0x04
#define MB_FC_ESCRIBIR_COIL           0x05
#define MB_FC_ESCRIBIR_REGISTRO       0x06
#define MB_FC_ESCRIBIR_COILS          0x0F
#define MB_FC_ESCRIBIR_REGISTROS      0x10

typedef enum {
    MB_SIN_PETICION = 0,/* todavia no se hizo ninguna peticion */
    MB_OK,              /* respuesta valida */
    MB_PENDIENTE,       /* esperando respuesta */
    MB_ERR_TIMEOUT,     /* el esclavo no respondio a tiempo */
    MB_ERR_CRC,         /* CRC de la respuesta incorrecto */
    MB_ERR_EXCEPCION,   /* el esclavo respondio con una excepcion (ver campo excepcion) */
    MB_ERR_TRAMA,       /* respuesta mal formada, de otro esclavo o de otra funcion */
    MB_ERR_OCUPADO,     /* ya hay una peticion en curso */
    MB_ERR_PARAM,       /* parametros invalidos (cantidad fuera de rango, etc.) */
    MB_ERR_TX,          /* fallo la transmision por la UART */
    MB_ERR_PUERTO       /* el RS-485 esta configurado como Modbus esclavo */
} mb_estado_t;

/* Lo que se le pidio al esclavo (se usa para validar la respuesta) */
typedef struct {
    uint8_t  esclavo;
    uint8_t  funcion;
    uint16_t direccion;
    uint16_t cantidad;  /* registros o bits; para 05/06 vale 1 */
    uint16_t valor;     /* valor escrito en 05 (0x0000/0xFF00) y 06 */
} mb_peticion_t;

/* Respuesta almacenada */
typedef struct {
    mb_estado_t estado;
    uint8_t  esclavo;
    uint8_t  funcion;
    uint8_t  excepcion;     /* codigo de excepcion si estado == MB_ERR_EXCEPCION */
    uint16_t direccion;     /* direccion inicial pedida */
    uint16_t cantidad;      /* cantidad pedida */
    uint16_t registros[MB_MAX_REGISTROS];   /* funciones 03 y 04 */
    uint8_t  bits[(MB_MAX_BITS + 7) / 8];   /* funciones 01 y 02, empaquetados (usar mb_bit) */
    uint8_t  trama[MB_MAX_TRAMA];           /* trama cruda recibida */
    uint16_t largo_trama;
    uint32_t tiempo_ms;     /* momento en que se completo (HAL_GetTick) */
} mb_respuesta_t;

#ifdef __cplusplus
extern "C" {
#endif

/* CRC-16 Modbus (definido en modbus_crc.c) */
uint16_t crc16(uint8_t *buffer, uint16_t buffer_length);

/* true si los dos ultimos bytes de la trama son su CRC correcto */
bool mb_crc_valido(const uint8_t *trama, uint16_t largo);

/* Valida la trama recibida contra la peticion y, si es correcta,
 * copia los datos en resp. Siempre completa resp->estado. */
mb_estado_t mb_interpretar(const mb_peticion_t *pet, const uint8_t *trama,
                           uint16_t largo, mb_respuesta_t *resp);

/* Valor del bit i (0..cantidad-1) de una respuesta 01/02 */
bool mb_bit(const mb_respuesta_t *resp, uint16_t i);

/* Texto corto del estado, util para depurar */
const char *mb_estado_texto(mb_estado_t e);

#ifdef __cplusplus
}
#endif

#endif /* MBUS_PARSER_H */
