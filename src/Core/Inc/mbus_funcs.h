#ifndef MBUS_FUNCS_H
#define MBUS_FUNCS_H
#include "induSPI.h"
#include "string.h"
extern uint8_t RxData[256];
extern uint8_t TxData[256];
extern uint8_t bufCompleto[128];
extern volatile uint8_t uart_rx_flag;

extern uint8_t Data[256];
extern UART_HandleTypeDef huart3;
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

void sendData (uint8_t *data, uint16_t size);

void readCoils(uint16_t slAddress, uint16_t pos, uint16_t numCoils);

void writeSingleCoil(uint16_t slAddress, uint16_t pos, uint16_t value);

void writeMultipleCoils(uint16_t slAddress, uint16_t pos, uint8_t *values, uint16_t numCoils);

void readInputContacts(uint16_t slAddress, uint16_t pos, uint16_t numContacts);

void readHoldingRegisters(uint16_t slAddress, uint16_t pos, uint16_t numRegisters);

void writeSingleRegister(uint16_t slAddress, uint16_t pos, uint16_t value);

void writeMultipleRegisters(uint16_t slAddress, uint16_t pos, uint16_t *values, uint16_t numRegisters);

void readAnalogInputs(uint16_t slAddress, uint16_t pos, uint16_t numRegisters);

#endif /* MBUS_FUNCS_H_ */