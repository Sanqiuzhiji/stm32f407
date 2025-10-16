//
// Created by 27721 on 25-10-13.
//

#ifndef LEDCONTROLLER_H
#define LEDCONTROLLER_H

#include "main.h"
#include <cstdint>
#include <vector>
#include <cmath>
#include "LedBase.h"

// =============================
// 高级 LED 控制类（逻辑层）
// 支持非阻塞闪烁与呼吸灯效果
// =============================

#include "main.h"
#include <cmath>

class LedController
{
public:
    enum class Mode
    {
        STATIC_OFF,
        STATIC_ON,
        BLINK,
        BREATH
    };

    LedController(GPIO_TypeDef* port, uint16_t pin, bool activeHigh = true);

    void SetMode(Mode mode);
    void SetBlink(uint32_t onTimeMs, uint32_t offTimeMs, bool startOn = true);
    void SetBreath(uint32_t periodMs);
    void Update(uint32_t deltaMs); // <-- 支持任意时间间隔

private:
    void On();
    void Off();

private:
    GPIO_TypeDef* _port;
    uint16_t _pin;
    bool _activeHigh;
    bool _isOn;

    Mode _mode;
    uint32_t _onTime;
    uint32_t _offTime;
    uint32_t _period;

    uint32_t _elapsed; // 累积时间（毫秒）
};


// =============================
// 多灯管理类
// =============================

class LedManager
{
public:
    void Add(LedController* led);
    void UpdateAll(uint32_t deltaMs);

private:
    std::vector<LedController*> _ledList;
};


#endif //LEDCONTROLLER_H
