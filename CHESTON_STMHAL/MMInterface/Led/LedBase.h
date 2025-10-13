//
// Created by 27721 on 25-10-13.
//

#ifndef LEDBASE_H
#define LEDBASE_H

// =============================
// 基础 LED 控制类（GPIO级别）
// =============================

#include "main.h"
#include <cstdint>
#include <vector>
#include <cmath>


class LedBase
{
public:
    LedBase(GPIO_TypeDef* port, uint16_t pin, bool activeHigh = true);
    virtual ~LedBase() = default;

    virtual void On();
    virtual void Off();
    virtual void Toggle();
    [[nodiscard]] bool IsOn() const { return _isOn; }

protected:
    GPIO_TypeDef* _port;
    uint16_t _pin;
    bool _activeHigh;
    bool _isOn;
};


#endif //LEDBASE_H
