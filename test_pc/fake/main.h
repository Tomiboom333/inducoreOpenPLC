/* main.h falso para compilar en la PC (solo tests). */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { int dummy; } SPI_HandleTypeDef;
typedef struct { int dummy; } USART_TypeDef;
typedef struct { uint32_t BaudRate; } UART_InitTypeDef;
typedef struct { USART_TypeDef *Instance; UART_InitTypeDef Init; } UART_HandleTypeDef;
typedef struct { int dummy; } GPIO_TypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET } GPIO_PinState;

extern USART_TypeDef fake_usart3;
extern GPIO_TypeDef fake_gpioa;
#define USART3 (&fake_usart3)
#define TX_EN_GPIO_Port (&fake_gpioa)
#define TX_EN_Pin 0x0100

uint32_t HAL_GetTick(void);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState st);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, const uint8_t *d, uint16_t n, uint32_t t);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t n);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h);
