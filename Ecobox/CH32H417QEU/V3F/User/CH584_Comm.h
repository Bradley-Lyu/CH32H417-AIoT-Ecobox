#ifndef __CH584_COMM_H
#define __CH584_COMM_H

#include "debug.h"

typedef struct
{
    uint8_t group;
    uint8_t cmd;
    uint8_t value;
    uint8_t has_value;
} CH584_CommPacket;

void CH584_Comm_Init(uint32_t baudrate);
void CH584_Comm_FlushRx(void);
uint8_t CH584_Comm_ReadByte(uint8_t *data);
uint8_t CH584_Comm_ReadPacket(CH584_CommPacket *packet);
void CH584_Comm_SendByte(uint8_t data);
void CH584_Comm_SendBuffer(const uint8_t *data, uint16_t length);
void CH584_Comm_SendPacket(const CH584_CommPacket *packet);

#endif
