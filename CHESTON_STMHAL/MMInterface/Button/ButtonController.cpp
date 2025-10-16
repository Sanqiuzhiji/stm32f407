//
// Created by 27721 on 25-10-16.
//

#include "ButtonController.h"
#include <cstdio>
#include <algorithm>
#include <iostream>
#include <numeric>

void ButtonController::UpdateButtonState()
{
    std::rotate(sample_buffer, sample_buffer + 1, sample_buffer + SAMPLE_COUNT);
    sample_buffer[SAMPLE_COUNT - 1] = static_cast<uint8_t>(Read_level());

    // for (uint8_t i = 0; i < SAMPLE_COUNT; i++)
    //     printf("%d ", sample_buffer[i]);
    // printf("\n");

    bool front_sample_result =
        std::accumulate(sample_buffer, sample_buffer + SAMPLE_COUNT / 2, 0) >=
        ((SAMPLE_COUNT / 2) % 2 == 0 ? (SAMPLE_COUNT / 2) / 2 : (SAMPLE_COUNT / 2) / 2 + 1);
    bool back_sample_result =
        std::accumulate(sample_buffer + SAMPLE_COUNT / 2, sample_buffer + SAMPLE_COUNT, 0) >=
        ((SAMPLE_COUNT / 2) % 2 == 0 ? (SAMPLE_COUNT / 2) / 2 : (SAMPLE_COUNT / 2) / 2 + 1);
    if (front_sample_result == back_sample_result)
    {
        if (front_sample_result)
            button_state = BUTTON_STATE::HIGH_LEVEL;
        else
            button_state = BUTTON_STATE::LOW_LEVEL;
    }
    else
    {
        if (front_sample_result)
            button_state = BUTTON_STATE::FALLING_EDGE;
        else
            button_state = BUTTON_STATE::RISING_EDGE;
    }

    // printf("button_state:%d\r\n", button_state);
}

void ButtonController::UpdateButtonType(uint16_t period_ms)
{
    pre_button_state = button_state;
    UpdateButtonState();
    if (pre_button_state == BUTTON_STATE::RISING_EDGE &&
        button_state == BUTTON_STATE::HIGH_LEVEL)
        button_action = BUTTON_ACTION::PRESS_BUTTON;
    else if (pre_button_state == BUTTON_STATE::HIGH_LEVEL &&
        button_state == BUTTON_STATE::FALLING_EDGE)
        button_action = BUTTON_ACTION::RELEASE_BUTTON;
    if (button_action == BUTTON_ACTION::PRESS_BUTTON)
    {
        high_time_counter += period_ms; //自增计时器
        if (high_time_counter == long_press_high_ms)
            printf("long press time is arrived\r\n");
        if (is_continue_clicking)
        {
            continue_click_counter++;
            is_continue_clicking = false;
            high_time_counter = 0;
            low_time_counter = 0;
            // printf("continue clicking:%d\r\n", continue_click_counter);
        }
    }
    else if (button_action == BUTTON_ACTION::RELEASE_BUTTON)
    {
        if (high_time_counter > long_press_high_ms) //判断是否长按
        {
            button_type = BUTTON_TYPE::LONG_PRESS_BUTTON;
            button_action = BUTTON_ACTION::NONE;
            high_time_counter = 0;
            printf("long press button\r\n");
            return;
        }
        low_time_counter += period_ms;
        // printf("low_time_counter:%d\r\n", low_time_counter);
        if (low_time_counter > continue_click_low_ms && continue_click_counter == 0) //如果低电平持续时间过高，则判定为单次点击
        {
            button_type = BUTTON_TYPE::CLICK_BUTTON;
            button_action = BUTTON_ACTION::NONE;
            high_time_counter = 0;
            low_time_counter = 0;
            is_continue_clicking = false;
            printf("click button\r\n");
            return;
        }
        is_continue_clicking = true;
        if (low_time_counter > continue_click_low_ms)
        {
            button_type = BUTTON_TYPE::CONTINUE_CLICK_BUTTON;
            button_action = BUTTON_ACTION::NONE;
            high_time_counter = 0;
            low_time_counter = 0;
            continue_click_counter++; //因为最后一次点击已经被判定为单次点击，所以这里需要加一
            printf("continue click:%d\r\n", continue_click_counter);
            continue_click_counter = 0;
            is_continue_clicking = false;
        }
    }
}
