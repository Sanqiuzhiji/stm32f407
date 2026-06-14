#include "main_link.h"
#include "interface_uart.h"
#include "main.h"
#include "retarget.h"
#include "stdio.h"
#include "usart.h"
#include "App/PlotTest/plot_test.h"

uint8_t uart1_rx_data = 0;
uint8_t result_u8 = 0;

/*
 * 功能描述: 串口命令处理函数
 float result = 0.0f;
    if (sscanf((char*)_data, "v %f", &result) < 1)
        printf("[v] Command format error\r\n");
    else
        功能代码;
 */

void OnUartCmd(uint8_t* _data, uint16_t _len)
{
    if (PlotTest_HandleCommand(_data, _len))
    {
        return;
    }

    printf_DMA("%.*s\r\n", _len, (const char*)(_data));
    if (sscanf((const char*)(_data), "led %d", &result_u8) < 1)
        printf("Command format error\r\n");
    else
    {
        HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, (GPIO_PinState)(result_u8));
    }
}

void UART_Interrupt_Init()
{
    HAL_UART_Receive_IT(&huart1, &uart1_rx_data, 1);
}
