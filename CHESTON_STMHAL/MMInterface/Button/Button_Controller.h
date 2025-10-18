#ifndef BUTTON_CONTROLLER_H
#define BUTTON_CONTROLLER_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BUTTON_STATE_LOW_LEVEL,
    BUTTON_STATE_HIGH_LEVEL,
    BUTTON_STATE_RISING_EDGE,
    BUTTON_STATE_FALLING_EDGE
} button_state_e;

typedef enum
{
    BUTTON_ACTION_NONE,
    BUTTON_ACTION_RELEASE_BUTTON,
    BUTTON_ACTION_PRESS_BUTTON
} button_action_e;

typedef enum
{
    BUTTON_TYPE_NONE,
    BUTTON_TYPE_CLICK_BUTTON,
    BUTTON_TYPE_DOUBLE_CLICK_BUTTON,
    BUTTON_TYPE_CONTINUE_CLICK_BUTTON,
    BUTTON_TYPE_LONG_PRESS_BUTTON
} button_type_e;

typedef struct
{
    button_type_e button_type;
    uint8_t button_click_count;
} button_send_data_t;

// 前向声明
typedef struct button_controller_t button_controller_t;

// 函数指针类型定义
typedef bool (*read_level_func_t)(button_controller_t* self);
typedef void (*update_state_func_t)(button_controller_t* self);
typedef void (*update_type_func_t)(button_controller_t* self);
typedef button_type_e (*get_type_func_t)(button_controller_t* self);
typedef void (*reset_type_func_t)(button_controller_t* self);

struct button_controller_t
{
    // GPIO配置
    GPIO_TypeDef* port;
    uint16_t pin;
    bool active_level;

    // 状态变量
    uint8_t sample_buffer[10];
    button_state_e button_state;
    button_state_e pre_button_state;
    button_action_e button_action;
    button_type_e button_type;

    // 计数器
    uint8_t continue_click_counter;
    uint32_t high_time_counter;
    uint32_t low_time_counter;

    // 配置参数
    uint16_t long_press_high_ms;
    uint16_t continue_click_low_ms;
    bool is_continue_clicking;

    // 成员函数指针
    read_level_func_t read_level;
    update_state_func_t update_state;
    update_type_func_t update_type;
    get_type_func_t get_type;
    reset_type_func_t reset_type;
};

// 构造函数
void button_controller_init(button_controller_t* btn, GPIO_TypeDef* port, uint16_t pin, bool active_level);

// 静态内联函数（替代简单的getter/setter）
static button_type_e button_get_type_simple(button_controller_t* btn)
{
    return btn->button_type;
}

static void button_reset_type_simple(button_controller_t* btn)
{
    btn->button_type = BUTTON_TYPE_NONE;
}

#endif
