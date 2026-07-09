#ifndef __FEEDER_H
#define __FEEDER_H

#include "debug.h"

#define FEEDER_PWM_PERIOD_US       20000U
#define FEEDER_PULSE_90_DEG_US      1500U
#define FEEDER_PULSE_180_DEG_US     2500U

void Feeder_Init(void);
void Feeder_FeedOnce(void);
void Feeder_SetPulseUs(uint16_t pulse_us);
void Feeder_Set90Deg(void);
void Feeder_Set180Deg(void);
void Feeder_Reset(void);
void Feeder_Service(uint32_t elapsed_ms);

#endif
