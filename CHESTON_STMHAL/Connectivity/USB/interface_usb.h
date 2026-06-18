#ifndef INTERFACE_USB_H
#define INTERFACE_USB_H

#include <stdbool.h>
#include <stdint.h>

void InterfaceUsb_Init(void);
void InterfaceUsb_SetRxCpltCallBack(void (*callback)(uint8_t* data, uint16_t len));
void InterfaceUsb_OnReceive(const uint8_t* data, uint32_t len);
void InterfaceUsb_TaskTick(void);

bool InterfaceUsb_CdcSend(const uint8_t* data, uint16_t len, void* user);
void InterfaceUsb_SetConfigured(bool configured);
bool InterfaceUsb_IsConfigured(void);

uint32_t InterfaceUsb_GetRxOverflowCount(void);
uint32_t InterfaceUsb_GetTxBusyCount(void);

#endif
