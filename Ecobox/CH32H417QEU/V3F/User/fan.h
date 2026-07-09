#ifndef __FAN_H
#define __FAN_H

#include "debug.h"

void Fan_Init(void);
void Fan_On(void);
void Fan_Off(void);
void Fan_Set(uint8_t on);
uint8_t Fan_GetState(void);

#endif
