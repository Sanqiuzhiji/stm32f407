#ifndef REMOTE_TEST_H
#define REMOTE_TEST_H

#include <stdbool.h>

void RemoteTest_Init(bool lcd_enabled);
void RemoteTest_TaskTick(void);
bool RemoteTest_IsActive(void);

#endif
