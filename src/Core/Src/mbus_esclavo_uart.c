/*
 * mbus_esclavo_uart.c - Puerto serie del Modbus esclavo.
 *
 * Recepcion por interrupcion hasta linea inactiva (fin de trama). La trama
 * se procesa fuera de la interrupcion, en mbs_procesar(), y la respuesta se
 * envia por interrupcion para no frenar el ciclo del PLC.
 */

#include "mbus_esclavo.h"
#include "induSPI.h"

static UART_HandleTypeDef *uart = 0;
static uint8_t rx[256];
static uint8_t tx[256];
static volatile bool     rx_listo = false;
static volatile bool     rx_error = false;
static volatile uint16_t rx_largo = 0;
static volatile bool     tx_en_curso = false;

bool mbs_usa_rs485(void)
{
    return MBS_PUERTO == MBS_PUERTO_RS485;
}

static void armar_rx(void)
{
    rx_listo = false;
    rx_error = false;
    HAL_UARTEx_ReceiveToIdle_IT(uart, rx, sizeof(rx));
}

void mbs_init(void)
{
#if MBS_PUERTO == MBS_PUERTO_RS485
    uart = &huart3;   /* ya inicializado por induInit() */
    HAL_GPIO_WritePin(TX_EN_GPIO_Port, TX_EN_Pin, GPIO_PIN_RESET);
#else
    MX_USART1_UART_Init(MBS_BAUDIOS);
    uart = &huart1;
#endif
    armar_rx();
}

bool mbs_es_su_uart(const void *huart)
{
    return uart != 0 && huart == (const void *)uart;
}

void mbs_rx_evento(uint16_t largo)
{
    if (largo == 0) {
        HAL_UARTEx_ReceiveToIdle_IT(uart, rx, sizeof(rx));
        return;
    }
    rx_largo = largo;
    rx_listo = true;
}

void mbs_rx_error(void)
{
    rx_error = true;
}

void mbs_procesar(void)
{
    if (uart == 0 || tx_en_curso)
        return;

    if (rx_error) {
        HAL_UART_AbortReceive(uart);
        armar_rx();
        return;
    }
    if (!rx_listo)
        return;

    uint16_t n = mbs_procesar_trama(MBS_DIRECCION, rx, rx_largo, tx);
    rx_listo = false;
    if (n == 0) {
        armar_rx();   /* no hay que responder */
        return;
    }

#if MBS_PUERTO == MBS_PUERTO_RS485
    HAL_GPIO_WritePin(TX_EN_GPIO_Port, TX_EN_Pin, GPIO_PIN_SET);
#endif
    tx_en_curso = true;
    if (HAL_UART_Transmit_IT(uart, tx, n) != HAL_OK) {
        tx_en_curso = false;
#if MBS_PUERTO == MBS_PUERTO_RS485
        HAL_GPIO_WritePin(TX_EN_GPIO_Port, TX_EN_Pin, GPIO_PIN_RESET);
#endif
        armar_rx();
    }
}

/* Fin de la transmision (se envio el ultimo bit): liberar el bus y volver a escuchar */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != uart)
        return;
#if MBS_PUERTO == MBS_PUERTO_RS485
    HAL_GPIO_WritePin(TX_EN_GPIO_Port, TX_EN_Pin, GPIO_PIN_RESET);
#endif
    tx_en_curso = false;
    armar_rx();
}
