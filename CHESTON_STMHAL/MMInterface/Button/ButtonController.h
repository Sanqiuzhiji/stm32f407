//
// Created by 27721 on 25-10-16.
//

#ifndef BUTTONCONTROLLER_H
#define BUTTONCONTROLLER_H

#include "ButtonBase.h"

class ButtonController : public ButtonBase
{
public:
    enum class BUTTON_STATE
    {
        LOW_LEVEL,
        HIGH_LEVEL,
        RISING_EDGE,
        FALLING_EDGE
    };

    enum class BUTTON_ACTION
    {
        NONE,
        RELEASE_BUTTON,
        PRESS_BUTTON
    };

    enum class BUTTON_TYPE
    {
        NONE,
        CLICK_BUTTON,
        DOUBLE_CLICK_BUTTON,
        CONTINUE_CLICK_BUTTON,
        LONG_PRESS_BUTTON
    };

    ButtonController(GPIO_TypeDef* port, uint16_t pin, bool active_level = true)
        : ButtonBase(port, pin, active_level)
    {
    }

    ~ButtonController() override = default;

    void UpdateButtonState();

    void UpdateButtonType();

    BUTTON_TYPE button_type = BUTTON_TYPE::NONE;
    uint8_t continue_click_counter = 0;


private:
    static constexpr uint8_t SAMPLE_COUNT = 10;
    uint8_t sample_buffer[SAMPLE_COUNT] = {false};
    uint8_t sample_index = 0;
    BUTTON_STATE button_state = BUTTON_STATE::LOW_LEVEL;
    BUTTON_STATE pre_button_state = BUTTON_STATE::LOW_LEVEL;
    BUTTON_ACTION button_action = BUTTON_ACTION::NONE;

    uint16_t long_press_high_ms = 1000; //continuous high-level time (ms)
    uint16_t continue_click_low_ms = 250; //continuous low-level time (ms)
    bool is_continue_clicking = false;
    uint32_t high_time_counter = 0; //按键高电平持续时间计数器
    uint32_t low_time_counter = 0; //按键低电平持续时间计数器
};


#endif //BUTTONCONTROLLER_H
