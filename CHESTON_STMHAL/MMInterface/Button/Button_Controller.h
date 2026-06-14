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
    BUTTON_ID_UP,
    BUTTON_ID_LEFT,
    BUTTON_ID_DOWN,
    BUTTON_ID_RIGHT,
    BUTTON_ID_UNKNOWN
} button_id_e;

typedef enum
{
    BUTTON_TYPE_NONE,
    BUTTON_TYPE_CLICK_BUTTON,
    BUTTON_TYPE_DOUBLE_CLICK_BUTTON,
    BUTTON_TYPE_MULTI_CLICK_BUTTON,
    BUTTON_TYPE_CONTINUE_CLICK_BUTTON = BUTTON_TYPE_MULTI_CLICK_BUTTON,
    BUTTON_TYPE_LONG_PRESS_BUTTON
} button_type_e;

typedef struct
{
    button_id_e button_id;
    button_type_e button_type;
    uint8_t button_click_count;
} button_send_data_t;

typedef struct button_controller_t button_controller_t;

typedef bool (*read_level_func_t)(button_controller_t* self);
typedef void (*update_state_func_t)(button_controller_t* self);
typedef bool (*update_type_func_t)(button_controller_t* self, button_send_data_t* event);
typedef button_type_e (*get_type_func_t)(button_controller_t* self);
typedef void (*reset_type_func_t)(button_controller_t* self);

struct button_controller_t
{
    button_id_e button_id;
    GPIO_TypeDef* port;
    uint16_t pin;
    bool active_level;

    uint8_t sample_buffer[10];
    button_state_e button_state;
    button_state_e pre_button_state;
    button_action_e button_action;
    button_type_e button_type;

    uint8_t continue_click_counter;
    uint32_t high_time_counter;
    uint32_t low_time_counter;

    uint16_t long_press_high_ms;
    uint16_t continue_click_low_ms;
    bool is_pressed;
    bool long_press_reported;
    bool waiting_click;

    read_level_func_t read_level;
    update_state_func_t update_state;
    update_type_func_t update_type;
    get_type_func_t get_type;
    reset_type_func_t reset_type;
};

void button_controller_init(button_controller_t* btn, button_id_e button_id,
                            GPIO_TypeDef* port, uint16_t pin, bool active_level);

static button_type_e button_get_type_simple(button_controller_t* btn)
{
    return btn->button_type;
}

static void button_reset_type_simple(button_controller_t* btn)
{
    btn->button_type = BUTTON_TYPE_NONE;
}

#endif
