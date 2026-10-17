#include "stdio.h"


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "modbus_crc.h"
#include "mbus_funcs.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
uint8_t RxData[256];
uint8_t TxData[256];
uint8_t bufCompleto[128];
volatile uint8_t uart_rx_flag;
uint8_t Data[256];


/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
//void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
      memcpy(Data, RxData, 256);
      HAL_UART_Receive_IT(&huart3, RxData, 9);
    }
   
}
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3)
    {
        memcpy(Data, RxData, Size);
    }
}



void sendData (uint8_t *data, uint16_t size)
{
	HAL_GPIO_WritePin(TX_EN_GPIO_Port, TX_EN_Pin, GPIO_PIN_SET);
  
  //timeout de 5ms
	HAL_UART_Transmit(&huart3, data, size, 10);

	HAL_GPIO_WritePin(TX_EN_GPIO_Port,TX_EN_Pin , GPIO_PIN_RESET);

  HAL_Delay(5);
}


void readCoils(uint16_t slAddress, uint16_t pos, uint16_t numCoils)
{
  TxData[0] = slAddress;  
  TxData[1] = 0x01;  // Function code for Read Coils

  TxData[2] = (uint8_t)(pos>>8&0xFF);
  TxData[3] = (uint8_t)(pos&0xFF);

  TxData[4] = (uint8_t)(numCoils>>8&0xFF);  // Number of coils to read HIGH
  TxData[5] = (uint8_t)(numCoils&0xFF);  // Number of coils to read LOW

  uint16_t crc = crc16(TxData, 6);
  TxData[6] = (uint8_t)(crc&0xFF);   // CRC LOW
  TxData[7] = (uint8_t)((crc>>8)&0xFF);  // CRC HIGH

  sendData(TxData, 8);

  HAL_UARTEx_ReceiveToIdle_IT(&huart3, RxData, 5+(numCoils+7)/8);
}

void writeSingleCoil(uint16_t slAddress, uint16_t pos, uint16_t value)
{
  TxData[0] = slAddress;  
  TxData[1] = 0x05;  // Function code for Write Single Coil

  TxData[2] = (uint8_t)(pos>>8&0xFF);
  TxData[3] = (uint8_t)(pos&0xFF);

  TxData[4] = (value == 0) ? 0x00 : 0xFF;  // Coil value LOW
  TxData[5] = 0x00;  // Coil value HIGH

  uint16_t crc = crc16(TxData, 6);
  TxData[6] = (uint8_t)(crc&0xFF);   // CRC LOW
  TxData[7] = (uint8_t)((crc>>8)&0xFF);  // CRC HIGH

  sendData(TxData, 8);
}

void writeMultipleCoils(uint16_t slAddress, uint16_t pos, uint8_t *values, uint16_t numCoils)
{
  TxData[0] = slAddress;  
  TxData[1] = 0x0F;  // Function code for Write Multiple Coils

  TxData[2] = (uint8_t)(pos>>8&0xFF);
  TxData[3] = (uint8_t)(pos&0xFF);

  TxData[4] = (uint8_t)(numCoils>>8&0xFF);  // Number of coils to write HIGH
  TxData[5] = (uint8_t)(numCoils&0xFF);  // Number of coils to write LOW

  TxData[6] = (numCoils + 7) / 8;  // Number of bytes needed to represent the coils

  uint8_t byteCount = (numCoils + 7) / 8; // Number of bytes needed to represent the coils

  memset(&TxData[7], 0, byteCount); // Clear the coil data bytes

  for (int i = 0; i < numCoils; i++) {
    TxData[7 + i/8] |= (values[i] ? (1 << (i % 8)) : 0);  // Set the appropriate bit for each coil
  }

  uint16_t crc = crc16(TxData, 7 + (numCoils + 7) / 8);
  TxData[7 + (numCoils + 7) / 8] = (uint8_t)(crc&0xFF);   // CRC LOW
  TxData[8 + (numCoils + 7) / 8] = (uint8_t)((crc>>8)&0xFF);  // CRC HIGH

  sendData(TxData, 9 + (numCoils + 7) / 8);
}

void readInputContacts(uint16_t slAddress, uint16_t pos, uint16_t numContacts)
{
  TxData[0] = slAddress;  
  TxData[1] = 0x02;  // Function code for Read Input Contacts

  TxData[2] = (uint8_t)(pos>>8&0xFF);
  TxData[3] = (uint8_t)(pos&0xFF);

  TxData[4] = (uint8_t)(numContacts>>8&0xFF);  // Number of contacts to read HIGH
  TxData[5] = (uint8_t)(numContacts&0xFF);  // Number of contacts to read LOW

  uint16_t crc = crc16(TxData, 6);
  TxData[6] = (uint8_t)(crc&0xFF);   // CRC LOW
  TxData[7] = (uint8_t)((crc>>8)&0xFF);  // CRC HIGH

  sendData(TxData, 8);
  HAL_UARTEx_ReceiveToIdle_IT(&huart3, RxData, 5+(numContacts+7)/8);
}

void readHoldingRegisters(uint16_t slAddress, uint16_t pos, uint16_t numRegisters)
{
  TxData[0] = slAddress;  
  TxData[1] = 0x03;  // Function code for Read Holding Registers

  TxData[2] = (uint8_t)(pos>>8&0xFF);
  TxData[3] = (uint8_t)(pos&0xFF);

  TxData[4] = (uint8_t)(numRegisters>>8&0xFF);  // Number of registers to read HIGH
  TxData[5] = (uint8_t)(numRegisters&0xFF);  // Number of registers to read LOW

  uint16_t crc = crc16(TxData, 6);
  TxData[6] = (uint8_t)(crc&0xFF);   // CRC LOW
  TxData[7] = (uint8_t)((crc>>8)&0xFF);  // CRC HIGH

  sendData(TxData, 8);
  // HAL_UART_Receive_IT(&huart3, RxData, 5 + numRegisters * 2);
  HAL_StatusTypeDef status = HAL_UART_Receive_IT(&huart3, RxData, 5 + numRegisters * 2);
  if (status != HAL_OK) {
    printf("Error al iniciar la recepción: %d\r\n", status);
  }
}

void writeSingleRegister(uint16_t slAddress, uint16_t pos, uint16_t value)
{
  TxData[0] = slAddress;  
  TxData[1] = 0x06;  // Function code for Write Single Register

  TxData[2] = (uint8_t)(pos>>8&0xFF);
  TxData[3] = (uint8_t)(pos&0xFF);

  TxData[4] = (uint8_t)(value>>8&0xFF);
  TxData[5] = (uint8_t)(value&0xFF);

  uint16_t crc = crc16(TxData, 6);
  TxData[6] = (uint8_t)(crc&0xFF);   // CRC LOW
  TxData[7] = (uint8_t)((crc>>8)&0xFF);  // CRC HIGH

  sendData(TxData, 8);
}

void writeMultipleRegisters(uint16_t slAddress, uint16_t pos, uint16_t *values, uint16_t numRegisters)
{
  TxData[0] = slAddress;  
  TxData[1] = 0x10;  // Function code for Write Multiple Registers

  TxData[2] = (uint8_t)(pos>>8&0xFF);
  TxData[3] = (uint8_t)(pos&0xFF);

  TxData[4] = (uint8_t)((numRegisters>>8)&0xFF);
  TxData[5] = (uint8_t)(numRegisters&0xFF);

  TxData[6] = numRegisters * 2;

  for (int i = 0; i < numRegisters; i++) {
    TxData[7 + i*2] = (uint8_t)((values[i]>>8)&0xFF);   // Register value HIGH
    TxData[8 + i*2] = (uint8_t)(values[i]&0xFF);   // Register value LOW
  }

  uint16_t crc = crc16(TxData, 7 + numRegisters*2);
  TxData[7 + numRegisters*2] = (uint8_t)(crc&0xFF);   // CRC LOW
  TxData[8 + numRegisters*2] = (uint8_t)((crc>>8)&0xFF);  // CRC HIGH

  sendData(TxData, 9 + numRegisters*2);
}

void readAnalogInputs(uint16_t slAddress, uint16_t pos, uint16_t numRegisters)
{
  TxData[0] = slAddress;  
  TxData[1] = 0x04;  // Function code for Read Analog Inputs

  TxData[2] = (uint8_t)(pos>>8&0xFF);
  TxData[3] = (uint8_t)(pos&0xFF);

  TxData[4] = (uint8_t)(numRegisters>>8&0xFF);  // Number of registers to read HIGH
  TxData[5] = (uint8_t)(numRegisters&0xFF);  // Number of registers to read LOW

  uint16_t crc = crc16(TxData, 6);
  TxData[6] = (uint8_t)(crc&0xFF);   // CRC LOW
  TxData[7] = (uint8_t)((crc>>8)&0xFF);  // CRC HIGH

  sendData(TxData, 8);
  HAL_UARTEx_ReceiveToIdle_IT(&huart3, RxData, 5 + numRegisters * 2);
}