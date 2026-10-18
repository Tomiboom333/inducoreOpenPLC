#ifndef MBUS_FUNCS_H
#define MBUS_FUNCS_H

/*
 * Maestro Modbus RTU por RS-485 (USART3, TX_EN en PA8).
 *
 * Funcionamiento NO bloqueante, pensado para el ciclo del PLC:
 *   1. Se llama a una funcion de peticion (readHoldingRegisters, writeSingleCoil, ...).
 *      Arma la trama, le agrega el CRC, la envia y queda esperando la respuesta.
 *   2. mb_procesar() (llamada seguido, por ejemplo en cada ciclo) detecta la
 *      respuesta, verifica el CRC y la longitud, y la guarda en mbRespuesta.
 *      Si el esclavo no contesta en mb_timeout_ms, la marca como MB_ERR_TIMEOUT.
 *   3. Si no hubo respuesta, llego con CRC incorrecto o mal formada, la
 *      peticion se reenvia hasta mb_reintentos veces (2 por defecto).
 *      Las excepciones del esclavo no se reintentan.
 *   4. Cuando termina (bien o mal) se llama al callback, si se registro uno.
 *
 * Para usarlo en forma bloqueante (pruebas, inicializacion): mb_esperar().
 */

#include "induSPI.h"
#include "mbus_parser.h"
#include <string.h>

/* Buffers de la UART (se mantienen por compatibilidad con el codigo anterior) */
extern uint8_t RxData[MB_MAX_TRAMA];
extern uint8_t TxData[MB_MAX_TRAMA];
extern uint8_t Data[MB_MAX_TRAMA];        /* copia de la ultima trama recibida */
extern volatile uint8_t uart_rx_flag;     /* 1 cuando hay una respuesta nueva; lo borra el usuario */
extern UART_HandleTypeDef huart3;

/* Ultima respuesta (valida o con error). Ver mbus_parser.h */
extern mb_respuesta_t mbRespuesta;

/* Contadores para diagnostico */
typedef struct {
    uint32_t peticiones;
    uint32_t ok;
    uint32_t timeouts;
    uint32_t errores_crc;
    uint32_t excepciones;
    uint32_t errores_trama;
    uint32_t errores_tx;
    uint32_t reintentos;
} mb_estadisticas_t;
extern mb_estadisticas_t mbEstadisticas;

/* Tiempo maximo de espera de la respuesta, en ms (por defecto 100) */
extern uint32_t mb_timeout_ms;
/* Reenvios automaticos ante timeout, CRC o trama invalida (por defecto 2) */
extern uint8_t mb_reintentos;

typedef void (*mb_callback_t)(const mb_respuesta_t *resp);

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Peticiones (devuelven MB_PENDIENTE si se enviaron, o un error) ---- */
mb_estado_t readCoils(uint16_t slAddress, uint16_t pos, uint16_t numCoils);                 /* 01 */
mb_estado_t readInputContacts(uint16_t slAddress, uint16_t pos, uint16_t numContacts);      /* 02 */
mb_estado_t readHoldingRegisters(uint16_t slAddress, uint16_t pos, uint16_t numRegisters);  /* 03 */
mb_estado_t readAnalogInputs(uint16_t slAddress, uint16_t pos, uint16_t numRegisters);      /* 04 */
mb_estado_t writeSingleCoil(uint16_t slAddress, uint16_t pos, uint16_t value);              /* 05 */
mb_estado_t writeSingleRegister(uint16_t slAddress, uint16_t pos, uint16_t value);          /* 06 */
mb_estado_t writeMultipleCoils(uint16_t slAddress, uint16_t pos, uint8_t *values, uint16_t numCoils);        /* 0F */
mb_estado_t writeMultipleRegisters(uint16_t slAddress, uint16_t pos, uint16_t *values, uint16_t numRegisters); /* 10 */

/* ---- Gestion ---- */
void        mb_procesar(void);              /* llamar seguido (en cada ciclo) */
bool        mb_ocupado(void);               /* true mientras se espera una respuesta */
mb_estado_t mb_estado(void);                /* estado de la ultima peticion */
mb_estado_t mb_esperar(void);               /* bloquea hasta respuesta o timeout */
void        mb_set_callback(mb_callback_t cb);

/* Envio crudo por RS-485 (maneja TX_EN). Devuelve true si se transmitio. */
bool sendData(uint8_t *data, uint16_t size);

/* Callbacks del HAL (implementados en mbus_funcs.c) */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* MBUS_FUNCS_H */
