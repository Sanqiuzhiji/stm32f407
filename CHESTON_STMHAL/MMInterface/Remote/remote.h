#ifndef REMOTE_H
#define REMOTE_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

typedef struct
{
    uint8_t key;
    uint8_t repeat_count;
    uint32_t raw_data;
} remote_key_event_t;

bool Remote_Init(TIM_HandleTypeDef* htim, uint32_t channel);
bool Remote_PollEvent(remote_key_event_t* event);
void Remote_OnTimerPeriodElapsed(TIM_HandleTypeDef* htim);
void Remote_OnInputCapture(TIM_HandleTypeDef* htim);

#endif
