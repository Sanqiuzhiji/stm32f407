#ifndef _RETARGET_H__
#define _RETARGET_H__

#include "stm32f4xx_hal.h"
#include <sys/stat.h>
#include <stdio.h>

void RetargetInit(UART_HandleTypeDef *huart);
void Serial_SendByte(uint8_t Byte);
void Serial_SendArray(const uint8_t *Array,uint8_t Array_length);
void printf_DMA(const char *format,...);

int _isatty(int fd);

int _write(int fd, const char *ptr, int len);

int _close(int fd);

int _lseek(int fd, int ptr, int dir);

int _read(int fd, char *ptr, int len);

int _fstat(int fd, struct stat *st);

#endif //#ifndef _RETARGET_H__