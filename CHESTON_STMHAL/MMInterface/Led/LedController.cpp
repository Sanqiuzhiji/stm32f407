//
// Created by 27721 on 25-10-13.
//

#include "LedController.h"

LedController::LedController(GPIO_TypeDef* port, uint16_t pin, bool activeHigh)
    : _port(port),
      _pin(pin),
      _activeHigh(activeHigh),
      _isOn(false),
      _mode(Mode::STATIC_OFF),
      _onTime(0), _offTime(0), _period(0), _elapsed(0)
{
}

void LedController::On()
{
    HAL_GPIO_WritePin(_port, _pin, _activeHigh ? GPIO_PIN_SET : GPIO_PIN_RESET);
    _isOn = true;
}

void LedController::Off()
{
    HAL_GPIO_WritePin(_port, _pin, _activeHigh ? GPIO_PIN_RESET : GPIO_PIN_SET);
    _isOn = false;
}

void LedController::SetMode(Mode mode)
{
    _mode = mode;
    _elapsed = 0;
    if (mode == Mode::STATIC_ON) On();
    else if (mode == Mode::STATIC_OFF) Off();
}

void LedController::SetBlink(uint32_t onTimeMs, uint32_t offTimeMs, bool startOn)
{
    _mode = Mode::BLINK;
    _onTime = onTimeMs;
    _offTime = offTimeMs;
    _elapsed = 0;
    if (startOn)
        On();
    else
        Off();
}

void LedController::SetBreath(uint32_t periodMs)
{
    _mode = Mode::BREATH;
    _period = periodMs;
    _elapsed = 0;
}

void LedController::Update(uint32_t deltaMs)
{
    _elapsed += deltaMs;

    switch (_mode)
    {
    case Mode::STATIC_ON:
    case Mode::STATIC_OFF:
        break;

    case Mode::BLINK:
        if (_isOn && _elapsed >= _onTime)
        {
            Off();
            _elapsed = 0;
        }
        else if (!_isOn && _elapsed >= _offTime)
        {
            On();
            _elapsed = 0;
        }
        break;

    case Mode::BREATH:
        // TODO: Implement breathing effect
        ;
    }
}

void LedManager::Add(LedController* led)
{
    _ledList.push_back(led);
}

void LedManager::UpdateAll(uint32_t deltaMs)
{
    for (auto* led : _ledList)
    {
        led->Update(deltaMs);
    }
}
