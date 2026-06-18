#ifndef TFT_LCD_H
#define TFT_LCD_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#define TFT_LCD_WIDTH 240U
#define TFT_LCD_HEIGHT 320U

#define TFT_LCD_COLOR_WHITE 0xFFFFU
#define TFT_LCD_COLOR_BLACK 0x0000U
#define TFT_LCD_COLOR_RED 0xF800U
#define TFT_LCD_COLOR_GREEN 0x07E0U
#define TFT_LCD_COLOR_BLUE 0x001FU

bool TftLcd_Init(void);
void TftLcd_Backlight(bool enabled);
void TftLcd_Clear(uint16_t color);
void TftLcd_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void TftLcd_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
uint16_t TftLcd_ReadId(void);
uint16_t TftLcd_GetWidth(void);
uint16_t TftLcd_GetHeight(void);

#endif
