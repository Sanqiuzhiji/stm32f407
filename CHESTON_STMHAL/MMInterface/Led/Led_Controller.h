#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

// =============================
// 基础 LED 控制结构体
// =============================

typedef struct led_base_t led_base_t;

// 基础LED函数指针类型
typedef void (*led_on_func_t)(led_base_t* self);
typedef void (*led_off_func_t)(led_base_t* self);
typedef void (*led_toggle_func_t)(led_base_t* self);
typedef bool (*led_is_on_func_t)(const led_base_t* self);

struct led_base_t
{
    GPIO_TypeDef* port;
    uint16_t pin;
    bool active_high;
    bool is_on;

    // 函数指针
    led_on_func_t on;
    led_off_func_t off;
    led_toggle_func_t toggle;
    led_is_on_func_t is_on_func;
};

// 基础LED函数声明
void led_base_init(led_base_t* led, GPIO_TypeDef* port, uint16_t pin, bool active_high);

// =============================
// 高级 LED 控制器
// =============================

typedef struct led_controller_t led_controller_t;

// LED模式枚举
typedef enum
{
    LED_MODE_STATIC_OFF,
    LED_MODE_STATIC_ON,
    LED_MODE_BLINK,
    LED_MODE_BREATH
} led_mode_t;

// 控制器函数指针类型
typedef void (*led_set_mode_func_t)(led_controller_t* self, led_mode_t mode);
typedef void (*led_set_blink_func_t)(led_controller_t* self, uint32_t on_time_ms, uint32_t off_time_ms, bool start_on);
typedef void (*led_set_breath_func_t)(led_controller_t* self, uint32_t period_ms);
typedef void (*led_update_func_t)(led_controller_t* self, uint32_t delta_ms);

struct led_controller_t
{
    // 基础LED功能
    led_base_t base;

    // 控制参数
    led_mode_t mode;
    uint32_t on_time;
    uint32_t off_time;
    uint32_t period;
    uint32_t elapsed;

    // 控制器函数指针
    led_set_mode_func_t set_mode;
    led_set_blink_func_t set_blink;
    led_set_breath_func_t set_breath;
    led_update_func_t update;
};

// 控制器函数声明
void led_controller_init(led_controller_t* led, GPIO_TypeDef* port, uint16_t pin, bool active_high);

// =============================
// LED 管理器
// =============================

#define MAX_LEDS 10  // 最大LED数量

typedef struct
{
    led_controller_t* leds[MAX_LEDS];
    uint8_t count;
} led_manager_t;

// 管理器函数声明
void led_manager_init(led_manager_t* manager);
bool led_manager_add(led_manager_t* manager, led_controller_t* led);
void led_manager_update_all(led_manager_t* manager, uint32_t delta_ms);

#endif // LED_CONTROLLER_H
