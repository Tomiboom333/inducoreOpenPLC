/* main.h falso para compilar en la PC (solo tests). */
#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { HAL_SPI_STATE_RESET = 0, HAL_SPI_STATE_READY } HAL_SPI_StateTypeDef;
typedef struct { int dummy; } SPI_HandleTypeDef;
typedef struct { int dummy; } USART_TypeDef;
typedef struct { uint32_t BaudRate; } UART_InitTypeDef;
typedef struct { USART_TypeDef *Instance; UART_InitTypeDef Init; } UART_HandleTypeDef;
typedef struct { int id; } GPIO_TypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET } GPIO_PinState;

extern USART_TypeDef fake_usart1, fake_usart3;
extern GPIO_TypeDef fake_gpioa;
#define USART1 (&fake_usart1)
#define USART3 (&fake_usart3)
#define GPIOA  (&fake_gpioa)

#define TX_EN_GPIO_Port GPIOA
#define TX_EN_Pin 0x0100
#define SPI1_SS4_GPIO_Port GPIOA
#define SPI1_SS4_Pin 0x1
#define SPI1_SS3_GPIO_Port GPIOA
#define SPI1_SS3_Pin 0x2
#define SPI1_SS2_GPIO_Port GPIOA
#define SPI1_SS2_Pin 0x4
#define SPI1_SS1_GPIO_Port GPIOA
#define SPI1_SS1_Pin 0x8
#define SPI1_SS0_GPIO_Port GPIOA
#define SPI1_SS0_Pin 0x10

#define __HAL_RCC_AFIO_CLK_ENABLE()   do {} while (0)
#define __HAL_AFIO_REMAP_SWJ_NOJTAG() do {} while (0)

uint32_t HAL_GetTick(void);
void HAL_Init(void);
void HAL_Delay(uint32_t ms);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState st);
HAL_SPI_StateTypeDef HAL_SPI_GetState(SPI_HandleTypeDef *h);
HAL_StatusTypeDef HAL_SPI_Transmit(SPI_HandleTypeDef *h, uint8_t *d, uint16_t n, uint32_t t);
HAL_StatusTypeDef HAL_SPI_Receive(SPI_HandleTypeDef *h, uint8_t *d, uint16_t n, uint32_t t);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, const uint8_t *d, uint16_t n, uint32_t t);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_IT(UART_HandleTypeDef *h, uint8_t *d, uint16_t n);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h);
