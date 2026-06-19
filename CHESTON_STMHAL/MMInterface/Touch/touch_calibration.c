#include "touch_calibration.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "LCD/tft_lcd.h"
#include "Touch/touch_screen.h"

#define CAL_POINT_COUNT 5U
#define CAL_MARGIN 20U
#define CAL_TOUCH_TIMEOUT_MS 30000U
#define CAL_MIN_RAW_DELTA 100

typedef struct
{
    uint16_t x;
    uint16_t y;
} cal_point_t;

static void draw_cross(uint16_t x, uint16_t y, uint16_t color);
static bool wait_raw_touch(uint16_t* raw_x, uint16_t* raw_y);
static void wait_release(void);
static bool build_config(const cal_point_t raw[CAL_POINT_COUNT],
                         touch_screen_config_t* config);
static int32_t abs_i32(int32_t value);

bool TouchCalibration_RunBlocking(void)
{
    const cal_point_t target[CAL_POINT_COUNT] = {
        {CAL_MARGIN, CAL_MARGIN},
        {TFT_LCD_WIDTH - CAL_MARGIN, CAL_MARGIN},
        {CAL_MARGIN, TFT_LCD_HEIGHT - CAL_MARGIN},
        {TFT_LCD_WIDTH - CAL_MARGIN, TFT_LCD_HEIGHT - CAL_MARGIN},
        {TFT_LCD_WIDTH / 2U, TFT_LCD_HEIGHT / 2U},
    };
    cal_point_t raw[CAL_POINT_COUNT] = {0};
    touch_screen_config_t config;

    TftLcd_Clear(TFT_LCD_COLOR_WHITE);

    for (uint8_t i = 0U; i < CAL_POINT_COUNT; i++)
    {
        draw_cross(target[i].x, target[i].y, TFT_LCD_COLOR_RED);

        if (!wait_raw_touch(&raw[i].x, &raw[i].y))
        {
            // printf("Touch calibration timeout at point %u\r\n", (unsigned)(i + 1U));
            TftLcd_Clear(TFT_LCD_COLOR_WHITE);
            return false;
        }

        // printf("Touch cal p%u: raw=(%u,%u) target=(%u,%u)\r\n",
        //        (unsigned)(i + 1U),
        //        raw[i].x,
        //        raw[i].y,
        //        target[i].x,
        //        target[i].y);

        draw_cross(target[i].x, target[i].y, TFT_LCD_COLOR_GREEN);
        wait_release();
        HAL_Delay(150U);
        draw_cross(target[i].x, target[i].y, TFT_LCD_COLOR_WHITE);
    }

    TouchScreen_GetDefaultConfig(&config);
    config.mode = TOUCH_SCREEN_MODE_RESISTIVE;
    config.width = TftLcd_GetWidth();
    config.height = TftLcd_GetHeight();

    if (!build_config(raw, &config))
    {
        // printf("Touch calibration failed: invalid raw point geometry\r\n");
        TftLcd_Clear(TFT_LCD_COLOR_WHITE);
        draw_cross(TFT_LCD_WIDTH / 2U, TFT_LCD_HEIGHT / 2U, TFT_LCD_COLOR_RED);
        return false;
    }

    (void)TouchScreen_Init(&config);
    // printf("Touch calibration OK: swap=%u invx=%u invy=%u xmin=%u xmax=%u ymin=%u ymax=%u\r\n",
    //        config.swap_xy ? 1U : 0U,
    //        config.invert_x ? 1U : 0U,
    //        config.invert_y ? 1U : 0U,
    //        config.resistive_x_min,
    //        config.resistive_x_max,
    //        config.resistive_y_min,
    //        config.resistive_y_max);

    TftLcd_Clear(TFT_LCD_COLOR_WHITE);
    return true;
}

static void draw_cross(uint16_t x, uint16_t y, uint16_t color)
{
    TftLcd_DrawLine((uint16_t)(x - 12U), y, (uint16_t)(x + 12U), y, color);
    TftLcd_DrawLine(x, (uint16_t)(y - 12U), x, (uint16_t)(y + 12U), color);

    TftLcd_DrawPixel((uint16_t)(x + 1U), (uint16_t)(y + 1U), color);
    TftLcd_DrawPixel((uint16_t)(x - 1U), (uint16_t)(y + 1U), color);
    TftLcd_DrawPixel((uint16_t)(x + 1U), (uint16_t)(y - 1U), color);
    TftLcd_DrawPixel((uint16_t)(x - 1U), (uint16_t)(y - 1U), color);
}

static bool wait_raw_touch(uint16_t* raw_x, uint16_t* raw_y)
{
    uint32_t start = HAL_GetTick();
    touch_screen_state_t state;

    while ((HAL_GetTick() - start) < CAL_TOUCH_TIMEOUT_MS)
    {
        if (TouchScreen_Scan(&state) && state.pressed && state.point_count > 0U)
        {
            *raw_x = state.raw_x[0];
            *raw_y = state.raw_y[0];
            return true;
        }

        HAL_Delay(10U);
    }

    return false;
}

static void wait_release(void)
{
    touch_screen_state_t state;

    do
    {
        HAL_Delay(10U);
    } while (TouchScreen_Scan(&state) && state.pressed);
}

static bool build_config(const cal_point_t raw[CAL_POINT_COUNT],
                         touch_screen_config_t* config)
{
    int32_t raw_x_h = (int32_t)(raw[1].x - raw[0].x) + (int32_t)(raw[3].x - raw[2].x);
    int32_t raw_x_v = (int32_t)(raw[2].x - raw[0].x) + (int32_t)(raw[3].x - raw[1].x);
    int32_t raw_y_h = (int32_t)(raw[1].y - raw[0].y) + (int32_t)(raw[3].y - raw[2].y);
    int32_t raw_y_v = (int32_t)(raw[2].y - raw[0].y) + (int32_t)(raw[3].y - raw[1].y);

    config->swap_xy = (abs_i32(raw_x_v) + abs_i32(raw_y_h)) >
                      (abs_i32(raw_x_h) + abs_i32(raw_y_v));

    uint16_t ax0 = config->swap_xy ? raw[0].y : raw[0].x;
    uint16_t ax1 = config->swap_xy ? raw[1].y : raw[1].x;
    uint16_t ax2 = config->swap_xy ? raw[2].y : raw[2].x;
    uint16_t ax3 = config->swap_xy ? raw[3].y : raw[3].x;
    uint16_t ay0 = config->swap_xy ? raw[0].x : raw[0].y;
    uint16_t ay1 = config->swap_xy ? raw[1].x : raw[1].y;
    uint16_t ay2 = config->swap_xy ? raw[2].x : raw[2].y;
    uint16_t ay3 = config->swap_xy ? raw[3].x : raw[3].y;

    uint16_t left = (uint16_t)(((uint32_t)ax0 + ax2) / 2U);
    uint16_t right = (uint16_t)(((uint32_t)ax1 + ax3) / 2U);
    uint16_t top = (uint16_t)(((uint32_t)ay0 + ay1) / 2U);
    uint16_t bottom = (uint16_t)(((uint32_t)ay2 + ay3) / 2U);

    if (abs_i32((int32_t)right - left) < CAL_MIN_RAW_DELTA ||
        abs_i32((int32_t)bottom - top) < CAL_MIN_RAW_DELTA)
    {
        return false;
    }

    if (left < right)
    {
        config->resistive_x_min = left;
        config->resistive_x_max = right;
        config->invert_x = false;
    }
    else
    {
        config->resistive_x_min = right;
        config->resistive_x_max = left;
        config->invert_x = true;
    }

    if (top < bottom)
    {
        config->resistive_y_min = top;
        config->resistive_y_max = bottom;
        config->invert_y = false;
    }
    else
    {
        config->resistive_y_min = bottom;
        config->resistive_y_max = top;
        config->invert_y = true;
    }

    return true;
}

static int32_t abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}
