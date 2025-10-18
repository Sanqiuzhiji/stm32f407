#ifndef LOOP_H
#define LOOP_H

#include "stm32f4xx_hal.h"

void Main_Setup();
void OnUartCmd(uint8_t* _data, uint16_t _len);
void Start_Task_Main(void* argument);
void Start_Task_TimerIRQ(void *argument);
void TIM7_IQR_1MS_Handler();

#endif
