/*
 * mbus_esclavo.c - Nucleo del Modbus RTU esclavo (sin HAL: se prueba en la PC).
 * Funciones soportadas: 01, 02, 03, 04, 05, 06, 0F, 10.
 */

#include "mbus_esclavo.h"
#include "mbus_parser.h"   /* crc16, mb_crc_valido */
#include <string.h>

mbs_estadisticas_t mbsEstadisticas;

static uint16_t leer16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }

static uint16_t cerrar(uint8_t *resp, uint16_t n)
{
    uint16_t crc = crc16(resp, n);
    resp[n]     = (uint8_t)(crc & 0xFF);
    resp[n + 1] = (uint8_t)(crc >> 8);
    mbsEstadisticas.respuestas++;
    return (uint16_t)(n + 2);
}

static uint16_t excepcion(uint8_t dir, uint8_t fc, uint8_t codigo, uint8_t *resp)
{
    resp[0] = dir;
    resp[1] = (uint8_t)(fc | 0x80);
    resp[2] = codigo;
    mbsEstadisticas.excepciones++;
    return cerrar(resp, 3);
}

/* true si todas las direcciones [ini, ini+n) existen en la tabla */
static bool rango_valido(mbs_tabla_t t, uint16_t ini, uint16_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        uint32_t d = (uint32_t)ini + i;
        if (d > 0xFFFF)
            return false;
        if (t == MBS_COILS || t == MBS_ENTRADAS) {
            bool b;
            if (!mbs_leer_bit(t, (uint16_t)d, &b)) return false;
        } else {
            uint16_t r;
            if (!mbs_leer_registro(t, (uint16_t)d, &r)) return false;
        }
    }
    return true;
}

uint16_t mbs_procesar_trama(uint8_t direccion, const uint8_t *req, uint16_t n, uint8_t *resp)
{
    if (n < 4 || !mb_crc_valido(req, n)) {
        mbsEstadisticas.errores_crc++;
        return 0;
    }
    uint8_t dir = req[0];
    bool broadcast = (dir == 0);
    if (!broadcast && dir != direccion) {
        mbsEstadisticas.otras_direcciones++;
        return 0;
    }
    mbsEstadisticas.tramas++;
    if (broadcast)
        mbsEstadisticas.broadcast++;

    uint8_t  fc  = req[1];
    uint16_t len = (uint16_t)(n - 2);   /* sin CRC */

    switch (fc) {

    case 0x01: case 0x02: {   /* leer coils / discrete inputs */
        if (broadcast) return 0;
        if (len != 6) return excepcion(dir, fc, MBS_EXC_VALOR, resp);
        uint16_t ini = leer16(&req[2]), cant = leer16(&req[4]);
        if (cant < 1 || cant > 2000) return excepcion(dir, fc, MBS_EXC_VALOR, resp);
        mbs_tabla_t t = (fc == 0x01) ? MBS_COILS : MBS_ENTRADAS;
        if (!rango_valido(t, ini, cant)) return excepcion(dir, fc, MBS_EXC_DIRECCION, resp);
        uint8_t bytes = (uint8_t)((cant + 7) / 8);
        resp[0] = dir; resp[1] = fc; resp[2] = bytes;
        memset(&resp[3], 0, bytes);
        for (uint16_t i = 0; i < cant; i++) {
            bool b = false;
            mbs_leer_bit(t, (uint16_t)(ini + i), &b);
            if (b) resp[3 + i / 8] |= (uint8_t)(1U << (i % 8));
        }
        return cerrar(resp, (uint16_t)(3 + bytes));
    }

    case 0x03: case 0x04: {   /* leer holding / input registers */
        if (broadcast) return 0;
        if (len != 6) return excepcion(dir, fc, MBS_EXC_VALOR, resp);
        uint16_t ini = leer16(&req[2]), cant = leer16(&req[4]);
        if (cant < 1 || cant > 125) return excepcion(dir, fc, MBS_EXC_VALOR, resp);
        mbs_tabla_t t = (fc == 0x03) ? MBS_HOLDING : MBS_INPUT_REGS;
        if (!rango_valido(t, ini, cant)) return excepcion(dir, fc, MBS_EXC_DIRECCION, resp);
        resp[0] = dir; resp[1] = fc; resp[2] = (uint8_t)(cant * 2);
        for (uint16_t i = 0; i < cant; i++) {
            uint16_t v = 0;
            mbs_leer_registro(t, (uint16_t)(ini + i), &v);
            resp[3 + 2 * i] = (uint8_t)(v >> 8);
            resp[4 + 2 * i] = (uint8_t)(v & 0xFF);
        }
        return cerrar(resp, (uint16_t)(3 + cant * 2));
    }

    case 0x05: {              /* escribir un coil */
        if (len != 6) return broadcast ? 0 : excepcion(dir, fc, MBS_EXC_VALOR, resp);
        uint16_t d = leer16(&req[2]), v = leer16(&req[4]);
        if (v != 0x0000 && v != 0xFF00) return broadcast ? 0 : excepcion(dir, fc, MBS_EXC_VALOR, resp);
        if (!mbs_escribir_bit(d, v == 0xFF00)) return broadcast ? 0 : excepcion(dir, fc, MBS_EXC_DIRECCION, resp);
        if (broadcast) return 0;
        memcpy(resp, req, 6);   /* eco */
        return cerrar(resp, 6);
    }

    case 0x06: {              /* escribir un registro */
        if (len != 6) return broadcast ? 0 : excepcion(dir, fc, MBS_EXC_VALOR, resp);
        uint16_t d = leer16(&req[2]), v = leer16(&req[4]);
        bool invalido = false;
        if (!mbs_escribir_registro(d, v, &invalido))
            return broadcast ? 0 : excepcion(dir, fc, invalido ? MBS_EXC_VALOR : MBS_EXC_DIRECCION, resp);
        if (broadcast) return 0;
        memcpy(resp, req, 6);
        return cerrar(resp, 6);
    }

    case 0x0F: {              /* escribir varios coils */
        if (len < 7) return broadcast ? 0 : excepcion(dir, fc, MBS_EXC_VALOR, resp);
        uint16_t ini = leer16(&req[2]), cant = leer16(&req[4]);
        uint8_t bytes = req[6];
        if (cant < 1 || cant > 1968 || bytes != (cant + 7) / 8 || len != (uint16_t)(7 + bytes))
            return broadcast ? 0 : excepcion(dir, fc, MBS_EXC_VALOR, resp);
        if (!rango_valido(MBS_COILS, ini, cant))
            return broadcast ? 0 : excepcion(dir, fc, MBS_EXC_DIRECCION, resp);
        for (uint16_t i = 0; i < cant; i++)
            mbs_escribir_bit((uint16_t)(ini + i), (req[7 + i / 8] >> (i % 8)) & 1);
        if (broadcast) return 0;
        memcpy(resp, req, 6);
        return cerrar(resp, 6);
    }

    case 0x10: {              /* escribir varios registros */
        if (len < 7) return broadcast ? 0 : excepcion(dir, fc, MBS_EXC_VALOR, resp);
        uint16_t ini = leer16(&req[2]), cant = leer16(&req[4]);
        uint8_t bytes = req[6];
        if (cant < 1 || cant > 123 || bytes != cant * 2 || len != (uint16_t)(7 + bytes))
            return broadcast ? 0 : excepcion(dir, fc, MBS_EXC_VALOR, resp);
        if (!rango_valido(MBS_HOLDING, ini, cant))
            return broadcast ? 0 : excepcion(dir, fc, MBS_EXC_DIRECCION, resp);
        /* Se escriben todos o ninguno: primero se validan los valores */
        for (uint16_t i = 0; i < cant; i++) {
            uint16_t v = leer16(&req[7 + 2 * i]);
            uint16_t d = (uint16_t)(ini + i);
            if (d < 1024 && v > 4095)   /* salidas analogicas de 12 bits */
                return broadcast ? 0 : excepcion(dir, fc, MBS_EXC_VALOR, resp);
        }
        for (uint16_t i = 0; i < cant; i++) {
            bool inv = false;
            mbs_escribir_registro((uint16_t)(ini + i), leer16(&req[7 + 2 * i]), &inv);
        }
        if (broadcast) return 0;
        memcpy(resp, req, 6);
        return cerrar(resp, 6);
    }

    default:
        if (broadcast) return 0;
        return excepcion(dir, fc, MBS_EXC_FUNCION, resp);
    }
}
