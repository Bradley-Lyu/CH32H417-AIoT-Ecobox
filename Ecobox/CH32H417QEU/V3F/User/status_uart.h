#ifndef __STATUS_UART_H
#define __STATUS_UART_H

#include "debug.h"

void StatusUart_Init(uint32_t baudrate);
uint8_t StatusUart_ReadByte(uint8_t *data);
uint8_t StatusUart_ReadPacket(uint8_t *group, uint8_t *cmd, uint8_t *value, uint8_t *has_value);
void StatusUart_SendByte(uint8_t data);
void StatusUart_SendString(const char *text);
void StatusUart_Printf(const char *fmt, ...);
void StatusUart_LogScreenPacket(uint8_t group, uint8_t cmd, uint8_t value, uint8_t has_value);

#endif
