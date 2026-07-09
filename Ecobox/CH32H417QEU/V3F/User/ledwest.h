#ifndef __LEDWEST_H
#define __LEDWEST_H

#include "debug.h"

void LedWest_Init(void);
void LedWest_SetBrightness(uint8_t percent);
uint8_t LedWest_GetBrightness(void);

#endif
