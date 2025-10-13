//
// Created by 27721 on 25-7-7.
//
#include "main_link.h"


LedController led0(LED0_GPIO_Port, LED0_Pin, false);
LedController led1(LED1_GPIO_Port, LED1_Pin, false);
LedManager ledManager;

extern "C" void Main_Setup() //延时使用HAL_Delay
{
    UART_Interrupt_Init();
    HAL_TIM_Base_Start_IT(&htim7);

    // 注册LED
    ledManager.Add(&led0);
    ledManager.Add(&led1);
    // 设置LED模式
    led0.SetBlink(800, 200);
    led1.SetBlink(200, 800, false);
}

extern "C" void Start_Task_Main(void* argument)
{
    for (;;)
    {
        vTaskDelay(500);
    }
}

extern "C" void TIM7_IQR_1MS_Handler()
{
    ledManager.UpdateAll(1);
}
