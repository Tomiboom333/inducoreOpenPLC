#pragma once
#include <stdint.h>
typedef struct { int dummy; } SPI_HandleTypeDef;
typedef struct { int dummy; } UART_HandleTypeDef;
uint32_t HAL_GetTick(void);
