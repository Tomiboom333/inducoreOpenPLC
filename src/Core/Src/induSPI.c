#include "induSPI.h"
#include "stm32f103xb.h"
#include "mbus_funcs.h"



SPI_HandleTypeDef hspi1;
estAct_t estAct;
UART_HandleTypeDef huart3;

static uint8_t spiTxBuffer[4];

static uint8_t InTxBuffer;
static uint8_t InRxBuffer[3];

GPIO_TypeDef *modulo[5] = {
  SPI1_NSS1_GPIO_Port,
  SPI1_NSS2_GPIO_Port,
  SPI1_NSS3_GPIO_Port,
  SPI1_NSS4_GPIO_Port,
  SPI1_NSS5_GPIO_Port
};

uint16_t pinCs[5] = {
  SPI1_NSS1_Pin,
  SPI1_NSS2_Pin,
  SPI1_NSS3_Pin,
  SPI1_NSS4_Pin,
  SPI1_NSS5_Pin
};



int contModulosIn = 0, contModulosOut = 0;
GPIO_TypeDef *modulosIn[5], *modulosOut[5];

uint16_t pinIn[5], pinOut[5];

static void spi_deassert_all_cs(void)
{
    for(int i=0; i<5; i++){
      HAL_GPIO_WritePin(modulo[i], pinCs[i], GPIO_PIN_SET);
    }
}

static void spi_wait_ready(void)
{
    while (HAL_SPI_GetState(&hspi1) != HAL_SPI_STATE_READY) {
    }
}

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART3_UART_Init(void);

void induInit(void){
    HAL_Init();
    SystemClock_Config();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_USART3_UART_Init();
    HAL_Delay(1000);
    moduleDet();
}

void moduleDet(void){
  contModulosIn=0; contModulosOut=0;
  uint8_t infoMod;
  uint8_t tx = 0x48;
  for(int i = 0; i<5; i++){
    infoMod=0x00;
    spi_deassert_all_cs();
    spi_wait_ready();
    HAL_GPIO_WritePin(modulo[i], pinCs[i], GPIO_PIN_RESET);
    if(HAL_SPI_Transmit(&hspi1, &tx, 1, 10 ) != HAL_OK){
      spi_deassert_all_cs();
      continue;
    }
    if(HAL_SPI_Receive(&hspi1, &infoMod, 1, 10) != HAL_OK){
      spi_deassert_all_cs();
      continue;
    }
    spi_deassert_all_cs();
    switch(infoMod){
      case 0x12:
        modulosIn[contModulosIn]=modulo[i];
        pinIn[contModulosIn]=pinCs[i];
        contModulosIn++;
        break;
      case 0x02:
        modulosOut[contModulosOut]=modulo[i];
        pinOut[contModulosOut]=pinCs[i];
        contModulosOut++;
        break;
      default: 
        break;
    }
    
    
  }

}

void digWrite(int modulo, int salida, bool estado){
  
  if (modulo < 0 || modulo >= 5)
  return;
  
  if (salida < 0 || salida >= 8)
  return;

  estAct.modOd[modulo][salida] = estado; // guardo el estado deseado de la salida elegida.
}

void anWrite(int modulo, int salida, uint8_t valor){
  if (modulo < 0 || modulo >= 5)
  return;
  
  if (salida < 0 || salida >= 2)
  return;
    estAct.modOa[modulo][salida] = valor; // guardo el valor deseado (0 a 255) de la salida elegida.
}

bool digRead(int modulo, int entrada){
  if (modulo < 0 || modulo >= 5)
  return false;

  if (entrada < 0 || entrada >= 8)
  return false;
  
  return estAct.modId[modulo][entrada];
}

uint8_t anRead(int modulo, int entrada){
  if (modulo < 0 || modulo >= 5)
  return 0;

  if (entrada < 0 || entrada >= 2)
  return false;

  return estAct.modIa[modulo][entrada];
}


void plc_store_spi_inputs(int j){
    for (int i = 0; i < 8; i++) {
        estAct.modId[j][i] = ((InRxBuffer[0] >> i) & 0x1);
    }

    for (int i = 0; i < 2; i++) {
        estAct.modIa[j][i] = InRxBuffer[i + 1];
    }
}


void plc_read_inputs(void){

    InTxBuffer = 0x01;

    /* Una lectura SPI necesita transmitir para generar el reloj. */
    for(int j = 0; j<contModulosIn; j++){
      for (int i = 0; i < 3; i++) {
        InRxBuffer[i] = 0x00;
      }

      spi_deassert_all_cs(); 
      spi_wait_ready();


      HAL_GPIO_WritePin(modulosIn[j], pinIn[j], GPIO_PIN_RESET);
      HAL_SPI_Transmit(&hspi1, &InTxBuffer, 1, 5);
      HAL_SPI_Receive(&hspi1, (uint8_t*)InRxBuffer, 3, 5);

      spi_deassert_all_cs();
      plc_store_spi_inputs(j);
    }
}

void plc_write_outputs(void){

    spiTxBuffer[0] = 0x02;
    spiTxBuffer[1] = 0x00;
    spiTxBuffer[2] = 0x00;
    spiTxBuffer[3] = 0x00;
    for(int j=0; j<contModulosOut; j++){
      spiTxBuffer[1] = 0x00;
      for (int i = 0; i < 8; i++) {
        if (estAct.modOd[j][i]) {
          spiTxBuffer[1] |= (uint8_t)(0x01 << i);
        }
      }

      /* estAct.modOa has two channels indexed 0 and 1 */
      for (int i = 0; i < 2; i++) {
          spiTxBuffer[i + 2] = estAct.modOa[j][i];
      }

      spi_deassert_all_cs();
      spi_wait_ready();

      HAL_GPIO_WritePin(modulosOut[j], pinOut[j], GPIO_PIN_RESET);

      if (HAL_SPI_Transmit(&hspi1, spiTxBuffer, 4, 10) != HAL_OK) {
          spi_deassert_all_cs();
          return;
      }

      spi_deassert_all_cs();
  }
}




void plc_run_cycle(void (*fuser)(void)){
    plc_read_inputs();
  /* La logica espera al callback; nunca se ejecuta dentro de la ISR. */
    fuser();
    plc_write_outputs();
    /* El tiempo de ciclo ahora lo fija plc_openplc_wait_cycle() (openplc_glue.c)
       segun el INTERVAL de la TASK del programa IEC. */
}
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  /* Los dos CS se manejan manualmente con GPIO. */
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */
  HAL_NVIC_SetPriority(SPI1_IRQn, 0, 0);//faltaba esto
  HAL_NVIC_EnableIRQ(SPI1_IRQn);//faltaba esto
  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, SPI1_NSS1_Pin|SPI1_NSS2_Pin|SPI1_NSS3_Pin|SPI1_NSS4_Pin
                          |SPI1_NSS5_Pin, GPIO_PIN_SET);//cambiado a set
  HAL_GPIO_WritePin(GPIOA, TX_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pins : SPI1_NSS1_Pin SPI1_NSS2_Pin SPI1_NSS3_Pin SPI1_NSS4_Pin
                           SPI1_NSS5_Pin TX_EN_Pin */
  GPIO_InitStruct.Pin = SPI1_NSS1_Pin|SPI1_NSS2_Pin|SPI1_NSS3_Pin|SPI1_NSS4_Pin
                          |SPI1_NSS5_Pin|TX_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB12 PB13 PB14 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB3 PB4 PB5 PB6 */
  GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
