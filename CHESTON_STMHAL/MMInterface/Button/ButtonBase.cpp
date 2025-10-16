//
// Created by 27721 on 25-10-16.
//

#include "ButtonBase.h"

ButtonBase::ButtonBase(GPIO_TypeDef* port, uint16_t pin, bool active_level)
{
    this->port = port;
    this->pin = pin;
    this->active_level = active_level;
}


bool ButtonBase::Read_level()
{
    return HAL_GPIO_ReadPin(port, pin) == active_level;
}
