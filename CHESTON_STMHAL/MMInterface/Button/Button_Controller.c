#include "button_controller.h"
#include <string.h>
#include <stdio.h>
#include "handle.h"
#include "FreeRTOS.h"
#include "queue.h"

#define SAMPLE_COUNT 10

// 私有函数声明
static bool button_read_level_impl(button_controller_t* self);
static void button_update_state_impl(button_controller_t* self);
static void button_update_type_impl(button_controller_t* self);
static button_type_e button_get_type_impl(button_controller_t* self);
static void button_reset_type_impl(button_controller_t* self);

static void rotate_buffer(uint8_t* buffer, uint8_t size);
static uint8_t accumulate_buffer(const uint8_t* buffer, uint8_t start, uint8_t end);
static bool get_majority_result(const uint8_t* buffer, uint8_t start, uint8_t end);

button_send_data_t common_send_data = {BUTTON_TYPE_NONE, 0};

void button_controller_init(button_controller_t* btn, GPIO_TypeDef* port, uint16_t pin, bool active_level)
{
    // 初始化数据成员
    btn->port = port;
    btn->pin = pin;
    btn->active_level = active_level;

    // 初始化状态
    memset(btn->sample_buffer, 0, sizeof(btn->sample_buffer));
    btn->button_state = BUTTON_STATE_LOW_LEVEL;
    btn->pre_button_state = BUTTON_STATE_LOW_LEVEL;
    btn->button_action = BUTTON_ACTION_NONE;
    btn->button_type = BUTTON_TYPE_NONE;

    // 初始化计数器
    btn->continue_click_counter = 0;
    btn->high_time_counter = 0;
    btn->low_time_counter = 0;

    // 初始化配置参数
    btn->long_press_high_ms = 1000;
    btn->continue_click_low_ms = 250;
    btn->is_continue_clicking = false;

    // 绑定成员函数
    btn->read_level = button_read_level_impl;
    btn->update_state = button_update_state_impl;
    btn->update_type = button_update_type_impl;
    btn->get_type = button_get_type_impl;
    btn->reset_type = button_reset_type_impl;
}

// 成员函数实现
static bool button_read_level_impl(button_controller_t* self)
{
    return (HAL_GPIO_ReadPin(self->port, self->pin) == self->active_level);
}

static void button_update_state_impl(button_controller_t* self)
{
    // 旋转缓冲区并添加新样本
    rotate_buffer(self->sample_buffer, SAMPLE_COUNT);
    self->sample_buffer[SAMPLE_COUNT - 1] = (uint8_t)self->read_level(self);

    bool front_sample_result = get_majority_result(self->sample_buffer, 0, 4);
    bool back_sample_result = get_majority_result(self->sample_buffer, 5, 9);

    if (front_sample_result == back_sample_result)
    {
        self->button_state = front_sample_result ? BUTTON_STATE_HIGH_LEVEL : BUTTON_STATE_LOW_LEVEL;
    }
    else
    {
        self->button_state = front_sample_result ? BUTTON_STATE_FALLING_EDGE : BUTTON_STATE_RISING_EDGE;
    }
}

static void button_update_type_impl(button_controller_t* self)
{
    self->pre_button_state = self->button_state;
    self->update_state(self);

    if (self->pre_button_state == BUTTON_STATE_RISING_EDGE &&
        self->button_state == BUTTON_STATE_HIGH_LEVEL)
    {
        self->button_action = BUTTON_ACTION_PRESS_BUTTON;
    }
    else if (self->pre_button_state == BUTTON_STATE_HIGH_LEVEL &&
        self->button_state == BUTTON_STATE_FALLING_EDGE)
    {
        self->button_action = BUTTON_ACTION_RELEASE_BUTTON;
    }

    if (self->button_action == BUTTON_ACTION_PRESS_BUTTON)
    {
        self->high_time_counter++;

        if (self->high_time_counter == self->long_press_high_ms)
        {
            // printf("long press time is arrived\r\n");
        }

        if (self->is_continue_clicking)
        {
            self->continue_click_counter++;
            self->is_continue_clicking = false;
            self->high_time_counter = 0;
            self->low_time_counter = 0;
        }
    }
    else if (self->button_action == BUTTON_ACTION_RELEASE_BUTTON)
    {
        if (self->high_time_counter > self->long_press_high_ms)
        {
            self->button_type = BUTTON_TYPE_LONG_PRESS_BUTTON;
            self->button_action = BUTTON_ACTION_NONE;
            self->high_time_counter = 0;
            common_send_data.button_type = self->button_type;
            xQueueSendToBackFromISR(Queue_ButtonHandle, &common_send_data, NULL);
            // printf("long press button\r\n");
            return;
        }

        self->low_time_counter++;

        if (self->low_time_counter > self->continue_click_low_ms && self->continue_click_counter == 0)
        {
            self->button_type = BUTTON_TYPE_CLICK_BUTTON;
            self->button_action = BUTTON_ACTION_NONE;
            self->high_time_counter = 0;
            self->low_time_counter = 0;
            self->is_continue_clicking = false;
            common_send_data.button_type = self->button_type;
            xQueueSendToBackFromISR(Queue_ButtonHandle, &common_send_data, NULL);
            // printf("click button\r\n");
            return;
        }

        self->is_continue_clicking = true;

        if (self->low_time_counter > self->continue_click_low_ms)
        {
            self->button_action = BUTTON_ACTION_NONE;
            self->high_time_counter = 0;
            self->low_time_counter = 0;
            self->continue_click_counter++;
            if (self->continue_click_counter == 2)
                self->button_type = BUTTON_TYPE_DOUBLE_CLICK_BUTTON;
            else
                self->button_type = BUTTON_TYPE_CONTINUE_CLICK_BUTTON;
            common_send_data.button_type = self->button_type;
            common_send_data.button_click_count = self->continue_click_counter;
            xQueueSendToBackFromISR(Queue_ButtonHandle, &common_send_data, NULL);
            // printf("continue click:%d\r\n", self->continue_click_counter);
            self->continue_click_counter = 0;
            self->is_continue_clicking = false;
        }
    }
}

static button_type_e button_get_type_impl(button_controller_t* self)
{
    return self->button_type;
}

static void button_reset_type_impl(button_controller_t* self)
{
    self->button_type = BUTTON_TYPE_NONE;
}

// 私有辅助函数
static void rotate_buffer(uint8_t* buffer, uint8_t size)
{
    for (uint8_t i = 0; i < size - 1; i++)
    {
        buffer[i] = buffer[i + 1];
    }
}

static uint8_t accumulate_buffer(const uint8_t* buffer, uint8_t start, uint8_t end)
{
    uint8_t sum = 0;
    for (uint8_t i = start; i <= end; i++)
    {
        sum += buffer[i];
    }
    return sum;
}

static bool get_majority_result(const uint8_t* buffer, uint8_t start, uint8_t end)
{
    uint8_t count = end - start + 1;
    uint8_t sum = accumulate_buffer(buffer, start, end);
    uint8_t threshold = (count % 2 == 0) ? (count / 2) : (count / 2 + 1);
    return (sum >= threshold);
}
