#ifndef TOUCH_SCREEN_H
#define TOUCH_SCREEN_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#define TOUCH_SCREEN_MAX_POINTS 10U

#define TOUCH_CT_SCL_GPIO_Port GPIOB
#define TOUCH_CT_SCL_Pin GPIO_PIN_0
#define TOUCH_CT_SDA_GPIO_Port GPIOF
#define TOUCH_CT_SDA_Pin GPIO_PIN_11
#define TOUCH_CT_INT_GPIO_Port GPIOB
#define TOUCH_CT_INT_Pin GPIO_PIN_1
#define TOUCH_CT_RST_GPIO_Port GPIOC
#define TOUCH_CT_RST_Pin GPIO_PIN_13

#define TOUCH_RES_PEN_GPIO_Port GPIOB
#define TOUCH_RES_PEN_Pin GPIO_PIN_1
#define TOUCH_RES_CS_GPIO_Port GPIOC
#define TOUCH_RES_CS_Pin GPIO_PIN_13
#define TOUCH_RES_MISO_GPIO_Port GPIOB
#define TOUCH_RES_MISO_Pin GPIO_PIN_2
#define TOUCH_RES_MOSI_GPIO_Port GPIOF
#define TOUCH_RES_MOSI_Pin GPIO_PIN_11
#define TOUCH_RES_CLK_GPIO_Port GPIOB
#define TOUCH_RES_CLK_Pin GPIO_PIN_0

typedef enum
{
    TOUCH_SCREEN_MODE_AUTO = 0,
    TOUCH_SCREEN_MODE_CAPACITIVE,
    TOUCH_SCREEN_MODE_RESISTIVE,
} touch_screen_mode_t;

typedef enum
{
    TOUCH_SCREEN_CHIP_NONE = 0,
    TOUCH_SCREEN_CHIP_GT9XXX,
    TOUCH_SCREEN_CHIP_FT5206,
    TOUCH_SCREEN_CHIP_XPT2046,
} touch_screen_chip_t;

typedef struct
{
    touch_screen_mode_t mode;
    uint16_t width;
    uint16_t height;
    bool swap_xy;
    bool invert_x;
    bool invert_y;

    uint16_t resistive_x_min;
    uint16_t resistive_x_max;
    uint16_t resistive_y_min;
    uint16_t resistive_y_max;
} touch_screen_config_t;

typedef struct
{
    touch_screen_chip_t chip;
    bool pressed;
    uint8_t point_count;
    uint16_t x[TOUCH_SCREEN_MAX_POINTS];
    uint16_t y[TOUCH_SCREEN_MAX_POINTS];
    uint16_t raw_x[TOUCH_SCREEN_MAX_POINTS];
    uint16_t raw_y[TOUCH_SCREEN_MAX_POINTS];
} touch_screen_state_t;

void TouchScreen_GetDefaultConfig(touch_screen_config_t* config);
bool TouchScreen_Init(const touch_screen_config_t* config);
bool TouchScreen_Scan(touch_screen_state_t* state);
touch_screen_chip_t TouchScreen_GetChip(void);
const char* TouchScreen_GetChipName(touch_screen_chip_t chip);

#endif
