#ifndef LOOP_H
#define LOOP_H

#ifdef __cplusplus
extern "C" {
#endif
/*---------------------------- C Scope ---------------------------*/

#include <stdio.h>
#include "stdint-gcc.h"
#include "main.h"
#include "usart.h"
#include "retarget.h"
#include "FreeRTOS.h"
#include "task.h"

void Main_Setup();
void OnUartCmd(uint8_t* _data, uint16_t _len);

#ifdef __cplusplus
}

/*---------------------------- C++ Scope ---------------------------*/

#include "cstdio"
#include "cstring"
#include "cstdint"
#include "cmath"

#include "interface_uart.h"

#endif

#endif
