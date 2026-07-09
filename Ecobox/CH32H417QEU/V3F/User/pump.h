#ifndef __PUMP_H
#define __PUMP_H

#include "debug.h"

void Pump_Init(void);
void Pump_On(void);
void Pump_Off(void);
void Pump_Set(uint8_t on);
uint8_t Pump_GetState(void);

#endif
