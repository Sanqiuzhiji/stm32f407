//
// Created by 27721 on 25-7-7.
//
#include "main_link.h"

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

#include "handle.h"
#include "App/PlotTest/plot_test.h"
#include "interface_uart.h"
#include "USB/interface_usb.h"
#include "Button_Controller.h"
#include "Led_Controller.h"
#include "LCD/tft_lcd.h"
#include "Touch/touch_screen.h"
#include "Touch/touch_calibration.h"

#define BUTTON_COUNT 4

static const char* Button_GetName(button_id_e button_id);
static void Button_SendEventFromISR(const button_send_data_t* event);

button_send_data_t common_receive_data = {BUTTON_ID_UNKNOWN, BUTTON_TYPE_NONE, 0};
button_controller_t buttons[BUTTON_COUNT];

led_controller_t led0;
led_controller_t led1;
led_manager_t led_manager;

static bool lcd_ready = false;

void Main_Setup()
{
    plot_test_config_t plot_config = {
        .send = InterfaceUsb_CdcSend,
        .user = NULL,
        .channel_count = PLOT_TEST_DEFAULT_CHANNELS,
        .interval_ms = PLOT_TEST_DEFAULT_INTERVAL_MS,
        .auto_start = true,
    };

    InterfaceUsb_Init();
    InterfaceUsb_SetRxCpltCallBack(OnUartCmd);
    UART_Interrupt_Init();
    PlotTest_Init(&plot_config);

    lcd_ready = TftLcd_Init();
    if (lcd_ready)
    {
        touch_screen_config_t touch_config;
        TouchScreen_GetDefaultConfig(&touch_config);
        touch_config.mode = TOUCH_SCREEN_MODE_RESISTIVE;
        touch_config.width = TftLcd_GetWidth();
        touch_config.height = TftLcd_GetHeight();
        (void)TouchScreen_Init(&touch_config);
        (void)TouchCalibration_RunBlocking();
    }
    else
    {
        printf("LCD init failed, skip touch calibration\r\n");
    }

    button_controller_init(&buttons[0], BUTTON_ID_UP, KEY_UP_GPIO_Port, KEY_UP_Pin, true);
    button_controller_init(&buttons[1], BUTTON_ID_LEFT, KEY_LEFT_GPIO_Port, KEY_LEFT_Pin, false);
    button_controller_init(&buttons[2], BUTTON_ID_DOWN, KEY_DOWN_GPIO_Port, KEY_DOWN_Pin, false);
    button_controller_init(&buttons[3], BUTTON_ID_RIGHT, KEY_RIGHT_GPIO_Port, KEY_RIGHT_Pin, false);

    led_controller_init(&led0, LED0_GPIO_Port, LED0_Pin, false);
    led_controller_init(&led1, LED1_GPIO_Port, LED1_Pin, false);

    led0.set_blink(&led0, 500, 500, true);
    led1.set_blink(&led1, 500, 500, false);

    led_manager_init(&led_manager);
    led_manager_add(&led_manager, &led0);
    led_manager_add(&led_manager, &led1);

    HAL_TIM_Base_Start_IT(&htim7);
}

void Start_Task_Main(void* argument)
{
    touch_screen_state_t touch_state;

    for (;;)
    {
        InterfaceUsb_TaskTick();
        PlotTest_TaskTick(HAL_GetTick());

        if (lcd_ready && TouchScreen_Scan(&touch_state) && touch_state.pressed)
        {
            TftLcd_DrawPixel(touch_state.x[0], touch_state.y[0], TFT_LCD_COLOR_RED);
            TftLcd_DrawPixel((uint16_t)(touch_state.x[0] + 1U), touch_state.y[0], TFT_LCD_COLOR_RED);
            TftLcd_DrawPixel(touch_state.x[0], (uint16_t)(touch_state.y[0] + 1U), TFT_LCD_COLOR_RED);
            TftLcd_DrawPixel((uint16_t)(touch_state.x[0] + 1U), (uint16_t)(touch_state.y[0] + 1U), TFT_LCD_COLOR_RED);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void Start_Task_TimerIRQ(void* argument)
{
    for (;;)
    {
        if (xQueueReceive(Queue_ButtonHandle, &common_receive_data, 5) == pdTRUE)
        {
            const char* button_name = Button_GetName(common_receive_data.button_id);

            if (common_receive_data.button_type == BUTTON_TYPE_CLICK_BUTTON)
            {
                printf("%s click button\r\n", button_name);
                HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
            }
            else if (common_receive_data.button_type == BUTTON_TYPE_LONG_PRESS_BUTTON)
            {
                printf("%s long button\r\n", button_name);
            }
            else if (common_receive_data.button_type == BUTTON_TYPE_DOUBLE_CLICK_BUTTON)
            {
                printf("%s double button\r\n", button_name);
            }
            else if (common_receive_data.button_type == BUTTON_TYPE_MULTI_CLICK_BUTTON)
            {
                printf("%s multi click button: %d\r\n", button_name, common_receive_data.button_click_count);
            }
        }
        vTaskDelay(10);
    }
}

void TIM7_IQR_1MS_Handler()
{
    button_send_data_t event = {BUTTON_ID_UNKNOWN, BUTTON_TYPE_NONE, 0};

    for (uint8_t i = 0; i < BUTTON_COUNT; i++)
    {
        if (buttons[i].update_type(&buttons[i], &event))
        {
            Button_SendEventFromISR(&event);
        }
    }
    // led_manager_update_all(&led_manager, 1);
}

static const char* Button_GetName(button_id_e button_id)
{
    switch (button_id)
    {
    case BUTTON_ID_UP:
        return "UP";
    case BUTTON_ID_LEFT:
        return "LEFT";
    case BUTTON_ID_DOWN:
        return "DOWN";
    case BUTTON_ID_RIGHT:
        return "RIGHT";
    default:
        return "UNKNOWN";
    }
}

static void Button_SendEventFromISR(const button_send_data_t* event)
{
    if (Queue_ButtonHandle == NULL || event == NULL || event->button_type == BUTTON_TYPE_NONE)
    {
        return;
    }

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendToBackFromISR(Queue_ButtonHandle, event, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
