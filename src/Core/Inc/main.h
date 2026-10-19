/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
/* Chip select de cada slot, con el nombre de la linea del riel a la que llegan.
 * El conector esta espejado: el pin PA4 llega a SS0 (slot M1, pegado a la CPU)
 * y el PA0 a SS4 (ultimo slot). Ver docs/PROTOCOLO.md. */
#define SPI1_SS0_Pin GPIO_PIN_4        /* slot M1  */
#define SPI1_SS0_GPIO_Port GPIOA
#define SPI1_SS1_Pin GPIO_PIN_3        /* slot M2  */
#define SPI1_SS1_GPIO_Port GPIOA
#define SPI1_SS2_Pin GPIO_PIN_2        /* slot ME1 */
#define SPI1_SS2_GPIO_Port GPIOA
#define SPI1_SS3_Pin GPIO_PIN_1        /* slot ME2 */
#define SPI1_SS3_GPIO_Port GPIOA
#define SPI1_SS4_Pin GPIO_PIN_0        /* slot ME3 */
#define SPI1_SS4_GPIO_Port GPIOA
#define TX_EN_Pin GPIO_PIN_8
#define TX_EN_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
