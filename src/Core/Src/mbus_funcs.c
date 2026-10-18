/*
 * mbus_funcs.c - Maestro Modbus RTU por RS-485 (USART3).
 *
 * Cada peticion: arma la trama + CRC, la transmite con TX_EN en alto,
 * baja TX_EN apenas termina el ultimo bit y arranca la recepcion por
 * interrupcion hasta detectar la linea inactiva (fin de trama).
 * mb_procesar() valida y guarda la respuesta o detecta el timeout.
 */

#include "mbus_funcs.h"
#include "mbus_esclavo.h"
#include "modbus_crc.h"

uint8_t RxData[MB_MAX_TRAMA];
uint8_t TxData[MB_MAX_TRAMA];
uint8_t Data[MB_MAX_TRAMA];
volatile uint8_t uart_rx_flag = 0;

mb_respuesta_t    mbRespuesta;   /* arranca en cero: estado = MB_SIN_PETICION */
mb_estadisticas_t mbEstadisticas;
uint32_t          mb_timeout_ms = 100;
uint8_t           mb_reintentos = 2;

static mb_peticion_t     pet_actual;
static volatile bool     esperando = false;
static volatile bool     rx_listo = false;
static volatile bool     rx_error = false;
static volatile uint16_t rx_largo = 0;
static uint32_t          inicio_ms = 0;
static uint32_t          fin_ms = 0;
static mb_callback_t     callback = 0;
static uint16_t          largo_actual = 0;   /* trama actual con CRC, para reenviar */
static uint8_t           reintentos_restantes = 0;

/* ------------------------------------------------------------------------ */

bool sendData(uint8_t *data, uint16_t size)
{
    /* Tiempo de transmision: 10 bits por byte + margen */
    uint32_t baud = huart3.Init.BaudRate ? huart3.Init.BaudRate : 9600;
    uint32_t timeout = (uint32_t)size * 10U * 1000U / baud + 5U;

    HAL_GPIO_WritePin(TX_EN_GPIO_Port, TX_EN_Pin, GPIO_PIN_SET);
    /* HAL_UART_Transmit vuelve cuando se envio el ultimo bit (flag TC) */
    HAL_StatusTypeDef st = HAL_UART_Transmit(&huart3, data, size, timeout);
    HAL_GPIO_WritePin(TX_EN_GPIO_Port, TX_EN_Pin, GPIO_PIN_RESET);

    return st == HAL_OK;
}

static void finalizar(mb_estado_t estado)
{
    esperando = false;
    fin_ms = HAL_GetTick();

    if (estado != MB_OK && estado != MB_ERR_CRC && estado != MB_ERR_EXCEPCION &&
        estado != MB_ERR_TRAMA) {
        /* No hubo trama valida para interpretar: se completa a mano */
        mbRespuesta.esclavo     = pet_actual.esclavo;
        mbRespuesta.funcion     = pet_actual.funcion;
        mbRespuesta.direccion   = pet_actual.direccion;
        mbRespuesta.cantidad    = pet_actual.cantidad;
        mbRespuesta.excepcion   = 0;
        mbRespuesta.largo_trama = 0;
    }
    mbRespuesta.estado   = estado;
    mbRespuesta.tiempo_ms = fin_ms;

    /* timeouts, CRC y trama se cuentan en falla_comunicacion() (cada intento) */
    switch (estado) {
    case MB_OK:            mbEstadisticas.ok++;            break;
    case MB_ERR_EXCEPCION: mbEstadisticas.excepciones++;   break;
    case MB_ERR_TX:        mbEstadisticas.errores_tx++;    break;
    default: break;
    }

    if (callback)
        callback(&mbRespuesta);
}

/* Envia la trama que esta en TxData y deja la recepcion armada */
static mb_estado_t enviar_actual(void)
{
    /* Silencio entre tramas: al menos ~1 ms desde la respuesta anterior */
    while ((HAL_GetTick() - fin_ms) < 2U) {
    }

    HAL_UART_AbortReceive(&huart3);
    rx_listo = false;
    rx_error = false;
    rx_largo = 0;

    if (!sendData(TxData, largo_actual)) {
        finalizar(MB_ERR_TX);
        return MB_ERR_TX;
    }

    /* Broadcast (esclavo 0): no hay respuesta */
    if (pet_actual.esclavo == 0) {
        finalizar(MB_OK);
        return MB_OK;
    }

    esperando = true;
    inicio_ms = HAL_GetTick();
    mbRespuesta.estado = MB_PENDIENTE;

    if (HAL_UARTEx_ReceiveToIdle_IT(&huart3, RxData, sizeof(RxData)) != HAL_OK) {
        finalizar(MB_ERR_TX);
        return MB_ERR_TX;
    }
    return MB_PENDIENTE;
}

/* Agrega el CRC y envia */
static mb_estado_t iniciar_peticion(const mb_peticion_t *pet, uint16_t largo)
{
    if (esperando)
        return MB_ERR_OCUPADO;
    if (mbs_usa_rs485())
        return MB_ERR_PUERTO;

    uint16_t crc = crc16(TxData, largo);
    TxData[largo]     = (uint8_t)(crc & 0xFF);         /* CRC bajo */
    TxData[largo + 1] = (uint8_t)((crc >> 8) & 0xFF);  /* CRC alto */
    largo_actual = (uint16_t)(largo + 2);

    pet_actual = *pet;
    reintentos_restantes = mb_reintentos;
    mbEstadisticas.peticiones++;

    return enviar_actual();
}

/* Ante una falla de comunicacion (timeout, CRC o trama invalida) la cuenta
 * y reenvia la peticion si quedan reintentos; si no, la da por terminada. */
static void falla_comunicacion(mb_estado_t e)
{
    switch (e) {
    case MB_ERR_TIMEOUT: mbEstadisticas.timeouts++;      break;
    case MB_ERR_CRC:     mbEstadisticas.errores_crc++;   break;
    default:             mbEstadisticas.errores_trama++; break;
    }
    if (reintentos_restantes > 0) {
        reintentos_restantes--;
        mbEstadisticas.reintentos++;
        esperando = false;
        fin_ms = HAL_GetTick();
        enviar_actual();
    } else {
        finalizar(e);
    }
}

static void cabecera(uint8_t esclavo, uint8_t funcion, uint16_t pos, uint16_t n)
{
    TxData[0] = esclavo;
    TxData[1] = funcion;
    TxData[2] = (uint8_t)(pos >> 8);
    TxData[3] = (uint8_t)(pos & 0xFF);
    TxData[4] = (uint8_t)(n >> 8);
    TxData[5] = (uint8_t)(n & 0xFF);
}

static mb_estado_t lectura(uint16_t esclavo, uint8_t funcion, uint16_t pos,
                           uint16_t cantidad, uint16_t maximo)
{
    if (esclavo == 0 || esclavo > 247 || cantidad == 0 || cantidad > maximo)
        return MB_ERR_PARAM;
    if (esperando)
        return MB_ERR_OCUPADO;
    mb_peticion_t p = { (uint8_t)esclavo, funcion, pos, cantidad, 0 };
    cabecera((uint8_t)esclavo, funcion, pos, cantidad);
    return iniciar_peticion(&p, 6);
}

/* ---- Peticiones --------------------------------------------------------- */

mb_estado_t readCoils(uint16_t slAddress, uint16_t pos, uint16_t numCoils)
{
    return lectura(slAddress, MB_FC_LEER_COILS, pos, numCoils, MB_MAX_BITS);
}

mb_estado_t readInputContacts(uint16_t slAddress, uint16_t pos, uint16_t numContacts)
{
    return lectura(slAddress, MB_FC_LEER_ENTRADAS, pos, numContacts, MB_MAX_BITS);
}

mb_estado_t readHoldingRegisters(uint16_t slAddress, uint16_t pos, uint16_t numRegisters)
{
    return lectura(slAddress, MB_FC_LEER_HOLDING, pos, numRegisters, MB_MAX_REGISTROS);
}

mb_estado_t readAnalogInputs(uint16_t slAddress, uint16_t pos, uint16_t numRegisters)
{
    return lectura(slAddress, MB_FC_LEER_INPUT_REGS, pos, numRegisters, MB_MAX_REGISTROS);
}

mb_estado_t writeSingleCoil(uint16_t slAddress, uint16_t pos, uint16_t value)
{
    if (slAddress > 247)
        return MB_ERR_PARAM;
    uint16_t v = value ? 0xFF00 : 0x0000;   /* valores que define el estandar */
    mb_peticion_t p = { (uint8_t)slAddress, MB_FC_ESCRIBIR_COIL, pos, 1, v };
    cabecera((uint8_t)slAddress, MB_FC_ESCRIBIR_COIL, pos, v);
    return iniciar_peticion(&p, 6);
}

mb_estado_t writeSingleRegister(uint16_t slAddress, uint16_t pos, uint16_t value)
{
    if (slAddress > 247)
        return MB_ERR_PARAM;
    mb_peticion_t p = { (uint8_t)slAddress, MB_FC_ESCRIBIR_REGISTRO, pos, 1, value };
    cabecera((uint8_t)slAddress, MB_FC_ESCRIBIR_REGISTRO, pos, value);
    return iniciar_peticion(&p, 6);
}

mb_estado_t writeMultipleCoils(uint16_t slAddress, uint16_t pos, uint8_t *values, uint16_t numCoils)
{
    if (slAddress > 247 || values == 0 || numCoils == 0 || numCoils > 1968)
        return MB_ERR_PARAM;
    if (esperando)
        return MB_ERR_OCUPADO;

    uint8_t bytes = (uint8_t)((numCoils + 7) / 8);
    cabecera((uint8_t)slAddress, MB_FC_ESCRIBIR_COILS, pos, numCoils);
    TxData[6] = bytes;
    memset(&TxData[7], 0, bytes);
    for (uint16_t i = 0; i < numCoils; i++) {
        if (values[i])
            TxData[7 + i / 8] |= (uint8_t)(1U << (i % 8));
    }
    mb_peticion_t p = { (uint8_t)slAddress, MB_FC_ESCRIBIR_COILS, pos, numCoils, 0 };
    return iniciar_peticion(&p, (uint16_t)(7 + bytes));
}

mb_estado_t writeMultipleRegisters(uint16_t slAddress, uint16_t pos, uint16_t *values, uint16_t numRegisters)
{
    if (slAddress > 247 || values == 0 || numRegisters == 0 || numRegisters > 123)
        return MB_ERR_PARAM;
    if (esperando)
        return MB_ERR_OCUPADO;

    cabecera((uint8_t)slAddress, MB_FC_ESCRIBIR_REGISTROS, pos, numRegisters);
    TxData[6] = (uint8_t)(numRegisters * 2);
    for (uint16_t i = 0; i < numRegisters; i++) {
        TxData[7 + i * 2] = (uint8_t)(values[i] >> 8);
        TxData[8 + i * 2] = (uint8_t)(values[i] & 0xFF);
    }
    mb_peticion_t p = { (uint8_t)slAddress, MB_FC_ESCRIBIR_REGISTROS, pos, numRegisters, 0 };
    return iniciar_peticion(&p, (uint16_t)(7 + numRegisters * 2));
}

/* ---- Gestion ------------------------------------------------------------ */

void mb_procesar(void)
{
    if (!esperando)
        return;

    if (rx_listo) {
        uint16_t largo = rx_largo;
        mb_estado_t e = mb_interpretar(&pet_actual, RxData, largo, &mbRespuesta);
        memcpy(Data, RxData, largo);
        uart_rx_flag = 1;
        if (e == MB_ERR_CRC || e == MB_ERR_TRAMA)
            falla_comunicacion(e);
        else
            finalizar(e);
    } else if (rx_error) {
        /* ruido, error de trama o desborde en la UART */
        HAL_UART_AbortReceive(&huart3);
        mbRespuesta.largo_trama = 0;
        falla_comunicacion(MB_ERR_TRAMA);
    } else if ((HAL_GetTick() - inicio_ms) >= mb_timeout_ms) {
        HAL_UART_AbortReceive(&huart3);
        falla_comunicacion(MB_ERR_TIMEOUT);
    }
}

bool mb_ocupado(void)
{
    return esperando;
}

mb_estado_t mb_estado(void)
{
    return mbRespuesta.estado;
}

mb_estado_t mb_esperar(void)
{
    while (esperando)
        mb_procesar();
    return mbRespuesta.estado;
}

void mb_set_callback(mb_callback_t cb)
{
    callback = cb;
}

/* ---- Callbacks del HAL (contexto de interrupcion: solo marcar flags) ---- */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (mbs_es_su_uart(huart)) {
        mbs_rx_evento(Size);
        return;
    }
    if (huart->Instance != USART3 || !esperando)
        return;
    if (Size == 0) {
        /* Linea inactiva sin datos: se vuelve a escuchar */
        HAL_UARTEx_ReceiveToIdle_IT(&huart3, RxData, sizeof(RxData));
        return;
    }
    rx_largo = Size;
    rx_listo = true;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (mbs_es_su_uart(huart)) {
        mbs_rx_error();
        return;
    }
    if (huart->Instance == USART3 && esperando)
        rx_error = true;   /* ruido, error de trama o desborde */
}
