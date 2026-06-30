#include "MMInterface/Button/Button_Controller.h"
#include <string.h>

#define SAMPLE_COUNT 10
#define SAMPLE_FRONT_START 0
#define SAMPLE_FRONT_END 4
#define SAMPLE_BACK_START 5
#define SAMPLE_BACK_END 9

static bool button_read_level_impl(button_controller_t* self);
static void button_update_state_impl(button_controller_t* self);
static bool button_update_type_impl(button_controller_t* self, button_send_data_t* event);
static button_type_e button_get_type_impl(button_controller_t* self);
static void button_reset_type_impl(button_controller_t* self);

static void rotate_buffer(uint8_t* buffer, uint8_t size);
static uint8_t accumulate_buffer(const uint8_t* buffer, uint8_t start, uint8_t end);
static bool get_majority_result(const uint8_t* buffer, uint8_t start, uint8_t end);
static bool is_active_state(button_state_e state);
static bool emit_event(button_controller_t* self, button_send_data_t* event,
                       button_type_e type, uint8_t click_count);
static void reset_click_sequence(button_controller_t* self);

void button_controller_init(button_controller_t* btn, button_id_e button_id,
                            GPIO_TypeDef* port, uint16_t pin, bool active_level)
{
    btn->button_id = button_id;
    btn->port = port;
    btn->pin = pin;
    btn->active_level = active_level;

    memset(btn->sample_buffer, 0, sizeof(btn->sample_buffer));
    btn->button_state = BUTTON_STATE_LOW_LEVEL;
    btn->pre_button_state = BUTTON_STATE_LOW_LEVEL;
    btn->button_action = BUTTON_ACTION_NONE;
    btn->button_type = BUTTON_TYPE_NONE;

    btn->continue_click_counter = 0;
    btn->high_time_counter = 0;
    btn->low_time_counter = 0;

    btn->long_press_high_ms = 1000;
    btn->continue_click_low_ms = 250;
    btn->is_pressed = false;
    btn->long_press_reported = false;
    btn->waiting_click = false;

    btn->read_level = button_read_level_impl;
    btn->update_state = button_update_state_impl;
    btn->update_type = button_update_type_impl;
    btn->get_type = button_get_type_impl;
    btn->reset_type = button_reset_type_impl;
}

static bool button_read_level_impl(button_controller_t* self)
{
    return (HAL_GPIO_ReadPin(self->port, self->pin) == self->active_level);
}

static void button_update_state_impl(button_controller_t* self)
{
    rotate_buffer(self->sample_buffer, SAMPLE_COUNT);
    self->sample_buffer[SAMPLE_COUNT - 1] = (uint8_t)self->read_level(self);

    bool front_sample_result = get_majority_result(self->sample_buffer, SAMPLE_FRONT_START, SAMPLE_FRONT_END);
    bool back_sample_result = get_majority_result(self->sample_buffer, SAMPLE_BACK_START, SAMPLE_BACK_END);

    if (front_sample_result == back_sample_result)
    {
        self->button_state = front_sample_result ? BUTTON_STATE_HIGH_LEVEL : BUTTON_STATE_LOW_LEVEL;
    }
    else
    {
        self->button_state = front_sample_result ? BUTTON_STATE_FALLING_EDGE : BUTTON_STATE_RISING_EDGE;
    }
}

static bool button_update_type_impl(button_controller_t* self, button_send_data_t* event)
{
    if (event != NULL)
    {
        event->button_id = self->button_id;
        event->button_type = BUTTON_TYPE_NONE;
        event->button_click_count = 0;
    }
    self->button_type = BUTTON_TYPE_NONE;

    self->pre_button_state = self->button_state;
    self->update_state(self);

    bool was_active = is_active_state(self->pre_button_state);
    bool is_active = is_active_state(self->button_state);

    if (!was_active && is_active)
    {
        self->button_action = BUTTON_ACTION_PRESS_BUTTON;
        self->is_pressed = true;
        self->high_time_counter = 0;
        self->low_time_counter = 0;
        self->long_press_reported = false;
        self->waiting_click = false;
    }
    else if (was_active && !is_active && self->is_pressed)
    {
        self->button_action = BUTTON_ACTION_RELEASE_BUTTON;
        self->is_pressed = false;

        if (self->long_press_reported)
        {
            reset_click_sequence(self);
            return false;
        }

        self->continue_click_counter++;
        self->high_time_counter = 0;
        self->low_time_counter = 0;
        self->waiting_click = true;
    }
    else
    {
        self->button_action = BUTTON_ACTION_NONE;
    }

    if (self->is_pressed)
    {
        self->high_time_counter++;

        if (!self->long_press_reported &&
            self->high_time_counter >= self->long_press_high_ms)
        {
            self->long_press_reported = true;
            self->continue_click_counter = 0;
            self->low_time_counter = 0;
            self->waiting_click = false;
            return emit_event(self, event, BUTTON_TYPE_LONG_PRESS_BUTTON, 0);
        }
    }
    else if (self->waiting_click)
    {
        self->low_time_counter++;

        if (self->low_time_counter >= self->continue_click_low_ms)
        {
            uint8_t click_count = self->continue_click_counter;
            button_type_e type = BUTTON_TYPE_CLICK_BUTTON;

            if (click_count == 2)
            {
                type = BUTTON_TYPE_DOUBLE_CLICK_BUTTON;
            }
            else if (click_count > 2)
            {
                type = BUTTON_TYPE_MULTI_CLICK_BUTTON;
            }

            reset_click_sequence(self);
            return emit_event(self, event, type, click_count);
        }
    }

    return false;
}

static button_type_e button_get_type_impl(button_controller_t* self)
{
    return self->button_type;
}

static void button_reset_type_impl(button_controller_t* self)
{
    self->button_type = BUTTON_TYPE_NONE;
}

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

static bool is_active_state(button_state_e state)
{
    return (state == BUTTON_STATE_HIGH_LEVEL || state == BUTTON_STATE_RISING_EDGE);
}

static bool emit_event(button_controller_t* self, button_send_data_t* event,
                       button_type_e type, uint8_t click_count)
{
    self->button_type = type;

    if (event == NULL)
    {
        return false;
    }

    event->button_id = self->button_id;
    event->button_type = type;
    event->button_click_count = click_count;
    return true;
}

static void reset_click_sequence(button_controller_t* self)
{
    self->button_action = BUTTON_ACTION_NONE;
    self->continue_click_counter = 0;
    self->high_time_counter = 0;
    self->low_time_counter = 0;
    self->long_press_reported = false;
    self->waiting_click = false;
}
