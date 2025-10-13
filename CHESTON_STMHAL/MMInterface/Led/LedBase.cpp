//
// Created by 27721 on 25-10-13.
//

#include "LedBase.h"

LedBase::LedBase(GPIO_TypeDef* port, uint16_t pin, bool activeHigh)
    : _port(port), _pin(pin), _activeHigh(activeHigh), _isOn(false)
{
}

void LedBase::On()
{
    HAL_GPIO_WritePin(_port, _pin, _activeHigh ? GPIO_PIN_SET : GPIO_PIN_RESET);
    _isOn = true;
}

void LedBase::Off()
{
    HAL_GPIO_WritePin(_port, _pin, _activeHigh ? GPIO_PIN_RESET : GPIO_PIN_SET);
    _isOn = false;
}

void LedBase::Toggle()
{
    HAL_GPIO_TogglePin(_port, _pin);
    _isOn = !_isOn;
}
