#ifndef __SHT30_IIC_H
#define __SHT30_IIC_H

#include "debug.h"

void SHT30_IIC_Init(void);
uint8_t SHT30_ReadTemperatureX10(int32_t *temp_x10);
uint8_t SHT30_GetLastAddr(void);
void SHT30_GetLastStatusText(char *text, uint32_t text_size);

#endif
