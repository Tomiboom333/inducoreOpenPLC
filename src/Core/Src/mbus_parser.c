/*
 * mbus_parser.c - Interprete de respuestas Modbus RTU (maestro).
 * Sin dependencias del HAL: se puede probar en la PC (test_pc/).
 */

#include "mbus_parser.h"
#include <string.h>

static uint16_t leer16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

bool mb_crc_valido(const uint8_t *trama, uint16_t largo)
{
    if (trama == 0 || largo < 4)
        return false;
    uint16_t calc = crc16((uint8_t *)trama, (uint16_t)(largo - 2));
    /* En Modbus RTU el CRC viaja con el byte bajo primero */
    uint16_t recibido = (uint16_t)(trama[largo - 2] | (trama[largo - 1] << 8));
    return calc == recibido;
}

mb_estado_t mb_interpretar(const mb_peticion_t *pet, const uint8_t *trama,
                           uint16_t largo, mb_respuesta_t *resp)
{
    resp->esclavo   = pet->esclavo;
    resp->funcion   = pet->funcion;
    resp->direccion = pet->direccion;
    resp->cantidad  = pet->cantidad;
    resp->excepcion = 0;

    if (largo > MB_MAX_TRAMA)
        largo = MB_MAX_TRAMA;
    memcpy(resp->trama, trama, largo);
    resp->largo_trama = largo;

    /* Minimo: direccion + funcion + 1 byte + CRC (2) */
    if (largo < 5)
        return resp->estado = MB_ERR_TRAMA;

    if (!mb_crc_valido(trama, largo))
        return resp->estado = MB_ERR_CRC;

    if (trama[0] != pet->esclavo)
        return resp->estado = MB_ERR_TRAMA;

    /* Excepcion: funcion con el bit 7 en 1, seguido del codigo */
    if (trama[1] == (uint8_t)(pet->funcion | 0x80)) {
        if (largo != 5)
            return resp->estado = MB_ERR_TRAMA;
        resp->excepcion = trama[2];
        return resp->estado = MB_ERR_EXCEPCION;
    }

    if (trama[1] != pet->funcion)
        return resp->estado = MB_ERR_TRAMA;

    switch (pet->funcion) {

    case MB_FC_LEER_COILS:
    case MB_FC_LEER_ENTRADAS: {
        uint16_t bytes = (uint16_t)((pet->cantidad + 7) / 8);
        if (pet->cantidad > MB_MAX_BITS || trama[2] != bytes || largo != (uint16_t)(5 + bytes))
            return resp->estado = MB_ERR_TRAMA;
        memset(resp->bits, 0, sizeof(resp->bits));
        memcpy(resp->bits, &trama[3], bytes);
        break;
    }

    case MB_FC_LEER_HOLDING:
    case MB_FC_LEER_INPUT_REGS: {
        uint16_t bytes = (uint16_t)(pet->cantidad * 2);
        if (pet->cantidad > MB_MAX_REGISTROS || trama[2] != bytes || largo != (uint16_t)(5 + bytes))
            return resp->estado = MB_ERR_TRAMA;
        for (uint16_t i = 0; i < pet->cantidad; i++)
            resp->registros[i] = leer16(&trama[3 + 2 * i]);
        break;
    }

    case MB_FC_ESCRIBIR_COIL:
    case MB_FC_ESCRIBIR_REGISTRO:
        /* El esclavo devuelve el eco de la peticion */
        if (largo != 8 || leer16(&trama[2]) != pet->direccion || leer16(&trama[4]) != pet->valor)
            return resp->estado = MB_ERR_TRAMA;
        if (pet->funcion == MB_FC_ESCRIBIR_REGISTRO)
            resp->registros[0] = pet->valor;
        break;

    case MB_FC_ESCRIBIR_COILS:
    case MB_FC_ESCRIBIR_REGISTROS:
        /* Respuesta: direccion inicial y cantidad escrita */
        if (largo != 8 || leer16(&trama[2]) != pet->direccion || leer16(&trama[4]) != pet->cantidad)
            return resp->estado = MB_ERR_TRAMA;
        break;

    default:
        return resp->estado = MB_ERR_TRAMA;
    }

    return resp->estado = MB_OK;
}

bool mb_bit(const mb_respuesta_t *resp, uint16_t i)
{
    if (i >= resp->cantidad || i >= MB_MAX_BITS)
        return false;
    return (resp->bits[i / 8] >> (i % 8)) & 0x01;
}

const char *mb_estado_texto(mb_estado_t e)
{
    switch (e) {
    case MB_OK:            return "OK";
    case MB_PENDIENTE:     return "PENDIENTE";
    case MB_ERR_TIMEOUT:   return "TIMEOUT";
    case MB_ERR_CRC:       return "ERROR CRC";
    case MB_ERR_EXCEPCION: return "EXCEPCION";
    case MB_ERR_TRAMA:     return "TRAMA INVALIDA";
    case MB_ERR_OCUPADO:   return "OCUPADO";
    case MB_ERR_PARAM:     return "PARAMETRO INVALIDO";
    case MB_ERR_TX:        return "ERROR TX";
    case MB_ERR_PUERTO:    return "PUERTO EN MODO ESCLAVO";
    case MB_SIN_PETICION:  return "SIN PETICION";
    }
    return "?";
}
