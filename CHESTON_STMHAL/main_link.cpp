//
// Created by 27721 on 25-7-7.
//
#include "main_link.h"


void Main_Setup() //—” ± π”√HAL_Delay
{
    UART_Interrupt_Init();
}

extern "C" void Start_Task_Main(void* argument)
{
    for (;;)
    {
        vTaskDelay(500);
    }
}