//
// Created by 27721 on 25-10-16.
//

#ifndef BUTTONBASE_H
#define BUTTONBASE_H

#include "main.h"

class ButtonBase
{
public:
    ButtonBase(GPIO_TypeDef* port, uint16_t pin, bool active_level = true);
    virtual ~ButtonBase() = default;
    virtual bool Read_level();

private:
    GPIO_TypeDef* port;
    uint16_t pin;
    bool active_level;
};


#endif //BUTTONBASE_H
