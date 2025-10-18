//
// Created by 27721 on 25-7-7.
//
#include "main_link.h"

//标准头文件-stm32头文件
#include "stdio.h"
#include "stdint-gcc.h"
#include "main.h"
#include "usart.h"
#include "retarget.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "tim.h"
#include "rtc.h"

//用户头文件
#include "handle.h"
#include "interface_uart.h"
#include "Button_Controller.h"

// RTC_DateTypeDef GetData;    //获取日期结构体
// RTC_TimeTypeDef GetTime;    //获取时间结构体
// HAL_RTC_GetTime(&hrtc, &GetTime, RTC_FORMAT_BIN);
// HAL_RTC_GetDate(&hrtc, &GetData, RTC_FORMAT_BIN);
//
// printf("%02d/%02d/%02d\r\n",2000 + GetData.Year, GetData.Month, GetData.Date);
// printf("%02d:%02d:%02d\r\n",GetTime.Hours, GetTime.Minutes, GetTime.Seconds);

button_send_data_t common_receive_data = {BUTTON_TYPE_NONE, 0};
button_controller_t button_up;

void Main_Setup() //延时使用HAL_Delay
{
    UART_Interrupt_Init();
    HAL_TIM_Base_Start_IT(&htim7);
    button_controller_init(&button_up, KEY_UP_GPIO_Port, KEY_UP_Pin, true);
}

void Start_Task_Main(void* argument)
{
    for (;;)
    {
        vTaskDelay(10);
    }
}

void Start_Task_TimerIRQ(void* argument)
{
    for (;;)
    {
        if (xQueueReceive(Queue_ButtonHandle, &common_receive_data, 5) == pdTRUE)
        {
            if (common_receive_data.button_type == BUTTON_TYPE_CLICK_BUTTON)
            {
                printf("click button\r\n");
                HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);
            }
            else if (common_receive_data.button_type == BUTTON_TYPE_LONG_PRESS_BUTTON)
            {
                printf("long button\r\n");
            }
            else if (common_receive_data.button_type == BUTTON_TYPE_DOUBLE_CLICK_BUTTON)
            {
                printf("double button\r\n");
            }
            else if (common_receive_data.button_type == BUTTON_TYPE_CONTINUE_CLICK_BUTTON)
            {
                printf("continue button: %d\r\n", common_receive_data.button_click_count);
            }
        }
        vTaskDelay(10);
    }
}

void TIM7_IQR_1MS_Handler()
{
    button_up.update_type(&button_up);
}
