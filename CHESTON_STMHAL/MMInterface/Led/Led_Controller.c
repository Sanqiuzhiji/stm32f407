#include "led_controller.h"
#include <string.h>

// =============================
// 基础 LED 函数实现
// =============================

static void led_base_on_impl(led_base_t* self)
{
    HAL_GPIO_WritePin(self->port, self->pin,
                      self->active_high ? GPIO_PIN_SET : GPIO_PIN_RESET);
    self->is_on = true;
}

static void led_base_off_impl(led_base_t* self)
{
    HAL_GPIO_WritePin(self->port, self->pin,
                      self->active_high ? GPIO_PIN_RESET : GPIO_PIN_SET);
    self->is_on = false;
}

static void led_base_toggle_impl(led_base_t* self)
{
    HAL_GPIO_TogglePin(self->port, self->pin);
    self->is_on = !self->is_on;
}

static bool led_base_is_on_impl(const led_base_t* self)
{
    return self->is_on;
}

void led_base_init(led_base_t* led, GPIO_TypeDef* port, uint16_t pin, bool active_high)
{
    led->port = port;
    led->pin = pin;
    led->active_high = active_high;
    led->is_on = false;

    // 绑定函数指针
    led->on = led_base_on_impl;
    led->off = led_base_off_impl;
    led->toggle = led_base_toggle_impl;
    led->is_on_func = led_base_is_on_impl;
}

// =============================
// LED 控制器函数实现
// =============================

// 私有控制器函数
static void led_controller_on_impl(led_controller_t* self)
{
    self->base.on(&self->base);
}

static void led_controller_off_impl(led_controller_t* self)
{
    self->base.off(&self->base);
}

static void led_controller_set_mode_impl(led_controller_t* self, led_mode_t mode)
{
    self->mode = mode;
    self->elapsed = 0;

    if (mode == LED_MODE_STATIC_ON)
    {
        led_controller_on_impl(self);
    }
    else if (mode == LED_MODE_STATIC_OFF)
    {
        led_controller_off_impl(self);
    }
}

static void led_controller_set_blink_impl(led_controller_t* self, uint32_t on_time_ms,
                                          uint32_t off_time_ms, bool start_on)
{
    self->mode = LED_MODE_BLINK;
    self->on_time = on_time_ms;
    self->off_time = off_time_ms;
    self->elapsed = 0;

    if (start_on)
    {
        led_controller_on_impl(self);
    }
    else
    {
        led_controller_off_impl(self);
    }
}

static void led_controller_set_breath_impl(led_controller_t* self, uint32_t period_ms)
{
    self->mode = LED_MODE_BREATH;
    self->period = period_ms;
    self->elapsed = 0;
}

static void led_controller_update_impl(led_controller_t* self, uint32_t delta_ms)
{
    self->elapsed += delta_ms;

    switch (self->mode)
    {
    case LED_MODE_STATIC_ON:
    case LED_MODE_STATIC_OFF:
        break;

    case LED_MODE_BLINK:
        if (self->base.is_on && self->elapsed >= self->on_time)
        {
            led_controller_off_impl(self);
            self->elapsed = 0;
        }
        else if (!self->base.is_on && self->elapsed >= self->off_time)
        {
            led_controller_on_impl(self);
            self->elapsed = 0;
        }
        break;

    case LED_MODE_BREATH:
        // TODO: 实现呼吸灯效果
        break;

    default:
        break;
    }
}

void led_controller_init(led_controller_t* led, GPIO_TypeDef* port, uint16_t pin, bool active_high)
{
    // 初始化基础LED
    led_base_init(&led->base, port, pin, active_high);

    // 初始化控制器参数
    led->mode = LED_MODE_STATIC_OFF;
    led->on_time = 0;
    led->off_time = 0;
    led->period = 0;
    led->elapsed = 0;

    // 绑定控制器函数指针
    led->set_mode = led_controller_set_mode_impl;
    led->set_blink = led_controller_set_blink_impl;
    led->set_breath = led_controller_set_breath_impl;
    led->update = led_controller_update_impl;
}

// =============================
// LED 管理器函数实现
// =============================

void led_manager_init(led_manager_t* manager)
{
    manager->count = 0;
    memset(manager->leds, 0, sizeof(manager->leds));
}

bool led_manager_add(led_manager_t* manager, led_controller_t* led)
{
    if (manager->count >= MAX_LEDS)
    {
        return false; // 已达到最大数量
    }

    manager->leds[manager->count] = led;
    manager->count++;
    return true;
}

void led_manager_update_all(led_manager_t* manager, uint32_t delta_ms)
{
    for (uint8_t i = 0; i < manager->count; i++)
    {
        if (manager->leds[i] != NULL)
        {
            manager->leds[i]->update(manager->leds[i], delta_ms);
        }
    }
}
