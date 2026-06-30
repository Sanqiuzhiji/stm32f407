#include "MMInterface/LCD/tft_lcd.h"

#include "main.h"

#include <stdio.h>

#define TFT_LCD_FSMC_NEX 4U
#define TFT_LCD_FSMC_RS_AX 6U
#define TFT_LCD_FSMC_BANK_BASE (0x60000000UL + (0x04000000UL * (TFT_LCD_FSMC_NEX - 1U)))
#define TFT_LCD_REG_ADDR TFT_LCD_FSMC_BANK_BASE
#define TFT_LCD_RAM_ADDR (TFT_LCD_FSMC_BANK_BASE + (1UL << (TFT_LCD_FSMC_RS_AX + 1U)))
#define TFT_LCD_REG (*((volatile uint16_t*)TFT_LCD_REG_ADDR))
#define TFT_LCD_RAM (*((volatile uint16_t*)TFT_LCD_RAM_ADDR))

static uint16_t lcd_id = 0U;
static bool lcd_initialized = false;

static void lcd_write_cmd(uint16_t cmd);
static void lcd_write_data(uint16_t data);
static uint16_t lcd_read_data(void);
static uint8_t lcd_read_u8(void);
static uint16_t lcd_read_ili9341_id(void);
static uint16_t lcd_detect_id(void);
static void lcd_print_id_probe(void);
static void lcd_ili9341_init(void);
static void lcd_st7789_init(void);
static void lcd_set_cursor(uint16_t x, uint16_t y);
static void lcd_prepare_write_ram(void);
static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
static void lcd_write_u8(uint8_t data);

bool TftLcd_Init(void)
{
    if (lcd_initialized)
    {
        return true;
    }

    TftLcd_Backlight(false);
    HAL_Delay(50U);

    lcd_write_cmd(0x01U);
    HAL_Delay(120U);

    lcd_print_id_probe();
    lcd_id = lcd_detect_id();
    // printf("LCD ID: 0x%04X\r\n", lcd_id);

    if (lcd_id == 0x7789U)
    {
        lcd_st7789_init();
    }
    else if (lcd_id == 0x9341U)
    {
        lcd_ili9341_init();
    }
    else
    {
        printf("LCD unsupported ID, init aborted\r\n");
        TftLcd_Backlight(false);
        return false;
    }

    TftLcd_Clear(TFT_LCD_COLOR_WHITE);
    TftLcd_Backlight(true);
    lcd_initialized = true;
    return true;
}

void TftLcd_Backlight(bool enabled)
{
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, enabled ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void TftLcd_Clear(uint16_t color)
{
    lcd_set_cursor(0U, 0U);
    lcd_prepare_write_ram();

    for (uint32_t i = 0U; i < ((uint32_t)TFT_LCD_WIDTH * (uint32_t)TFT_LCD_HEIGHT); i++)
    {
        lcd_write_data(color);
    }
}

void TftLcd_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= TFT_LCD_WIDTH || y >= TFT_LCD_HEIGHT)
    {
        return;
    }

    lcd_set_cursor(x, y);
    lcd_prepare_write_ram();
    lcd_write_data(color);
}

void TftLcd_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    int32_t dx = (x1 > x0) ? (int32_t)(x1 - x0) : (int32_t)(x0 - x1);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t dy = -((y1 > y0) ? (int32_t)(y1 - y0) : (int32_t)(y0 - y1));
    int32_t sy = (y0 < y1) ? 1 : -1;
    int32_t err = dx + dy;

    for (;;)
    {
        TftLcd_DrawPixel(x0, y0, color);

        if (x0 == x1 && y0 == y1)
        {
            break;
        }

        int32_t e2 = 2 * err;

        if (e2 >= dy)
        {
            err += dy;
            x0 = (uint16_t)((int32_t)x0 + sx);
        }

        if (e2 <= dx)
        {
            err += dx;
            y0 = (uint16_t)((int32_t)y0 + sy);
        }
    }
}

void TftLcd_DrawRgb565Line(uint16_t x, uint16_t y, const uint16_t* pixels, uint16_t count)
{
    if (pixels == NULL || count == 0U || x >= TFT_LCD_WIDTH || y >= TFT_LCD_HEIGHT)
    {
        return;
    }

    if ((uint32_t)x + count > TFT_LCD_WIDTH)
    {
        count = (uint16_t)(TFT_LCD_WIDTH - x);
    }

    lcd_set_window(x, y, (uint16_t)(x + count - 1U), y);

    for (uint16_t i = 0U; i < count; i++)
    {
        lcd_write_data(pixels[i]);
    }
}
uint16_t TftLcd_ReadId(void)
{
    return lcd_id;
}

uint16_t TftLcd_GetWidth(void)
{
    return TFT_LCD_WIDTH;
}

uint16_t TftLcd_GetHeight(void)
{
    return TFT_LCD_HEIGHT;
}

/**
    lcd_write_cmd(0xD3U)等价于：
    *(volatile uint16_t*)0x6C000000 = 0x00D3;
    CPU 对 0x6C000000 这个地址写数据时，FSMC 硬件会自动产生 8080 并口时序：
    - PG12 / FSMC_NE4 拉低，选中 LCD
    - PD5 / FSMC_NWE 产生写脉冲
    - D0~D15 输出 0x00D3
    - PF12 / FSMC_A6 = 0，表示这是命令
*/
static void lcd_write_cmd(uint16_t cmd)
{
    TFT_LCD_REG = cmd;
}

/**
    lcd_write_data(data)等价于：
    *(volatile uint16_t*)0x6C000080 = data;
    此时 PF12 / FSMC_A6 = 1，LCD 就把它当作数据/参数/GRAM 写入。
 */
static void lcd_write_data(uint16_t data)
{
    TFT_LCD_RAM = data;
}

static uint16_t lcd_read_data(void)
{
    return TFT_LCD_RAM;
}

static uint8_t lcd_read_u8(void)
{
    return (uint8_t)(lcd_read_data() & 0x00FFU);
}

static uint16_t lcd_read_ili9341_id(void)
{
    uint16_t id;

    lcd_write_cmd(0xD3U);
    (void)lcd_read_data();
    (void)lcd_read_data();
    id = (uint16_t)((uint16_t)lcd_read_u8() << 8U);
    id |= lcd_read_u8();
    return id;
}

static uint16_t lcd_detect_id(void)
{
    uint16_t id = lcd_read_ili9341_id();

    if (id == 0x9341U)
    {
        return id;
    }

    lcd_write_cmd(0x04U);
    (void)lcd_read_data();
    (void)lcd_read_data();
    id = (uint16_t)((uint16_t)lcd_read_u8() << 8U);
    id |= lcd_read_u8();

    if (id == 0x8552U)
    {
        return 0x7789U;
    }

    return id;
}

static void lcd_print_id_probe(void)
{
    uint16_t d3[4];
    uint16_t r04[4];

    lcd_write_cmd(0xD3U);
    for (uint8_t i = 0U; i < 4U; i++)
    {
        d3[i] = lcd_read_data();
    }

    lcd_write_cmd(0x04U);
    for (uint8_t i = 0U; i < 4U; i++)
    {
        r04[i] = lcd_read_data();
    }

    // printf("LCD RDDID D3: %04X %04X %04X %04X\r\n", d3[0], d3[1], d3[2], d3[3]);
    // printf("LCD RDDID 04: %04X %04X %04X %04X\r\n", r04[0], r04[1], r04[2], r04[3]);
}

static void lcd_ili9341_init(void)
{
    lcd_write_cmd(0xCFU);
    lcd_write_u8(0x00U);
    lcd_write_u8(0xC1U);
    lcd_write_u8(0x30U);
    lcd_write_cmd(0xEDU);
    lcd_write_u8(0x64U);
    lcd_write_u8(0x03U);
    lcd_write_u8(0x12U);
    lcd_write_u8(0x81U);
    lcd_write_cmd(0xE8U);
    lcd_write_u8(0x85U);
    lcd_write_u8(0x10U);
    lcd_write_u8(0x7AU);
    lcd_write_cmd(0xCBU);
    lcd_write_u8(0x39U);
    lcd_write_u8(0x2CU);
    lcd_write_u8(0x00U);
    lcd_write_u8(0x34U);
    lcd_write_u8(0x02U);
    lcd_write_cmd(0xF7U);
    lcd_write_u8(0x20U);
    lcd_write_cmd(0xEAU);
    lcd_write_u8(0x00U);
    lcd_write_u8(0x00U);
    lcd_write_cmd(0xC0U);
    lcd_write_u8(0x1BU);
    lcd_write_cmd(0xC1U);
    lcd_write_u8(0x01U);
    lcd_write_cmd(0xC5U);
    lcd_write_u8(0x30U);
    lcd_write_u8(0x30U);
    lcd_write_cmd(0xC7U);
    lcd_write_u8(0xB7U);
    lcd_write_cmd(0x36U);
    lcd_write_u8(0x08U);
    lcd_write_cmd(0x3AU);
    lcd_write_u8(0x55U);
    lcd_write_cmd(0xB1U);
    lcd_write_u8(0x00U);
    lcd_write_u8(0x1AU);
    lcd_write_cmd(0xB6U);
    lcd_write_u8(0x0AU);
    lcd_write_u8(0xA2U);
    lcd_write_cmd(0xF2U);
    lcd_write_u8(0x00U);
    lcd_write_cmd(0x26U);
    lcd_write_u8(0x01U);

    lcd_write_cmd(0xE0U);
    lcd_write_u8(0x0FU);
    lcd_write_u8(0x2AU);
    lcd_write_u8(0x28U);
    lcd_write_u8(0x08U);
    lcd_write_u8(0x0EU);
    lcd_write_u8(0x08U);
    lcd_write_u8(0x54U);
    lcd_write_u8(0xA9U);
    lcd_write_u8(0x43U);
    lcd_write_u8(0x0AU);
    lcd_write_u8(0x0FU);
    lcd_write_u8(0x00U);
    lcd_write_u8(0x00U);
    lcd_write_u8(0x00U);
    lcd_write_u8(0x00U);

    lcd_write_cmd(0xE1U);
    lcd_write_u8(0x00U);
    lcd_write_u8(0x15U);
    lcd_write_u8(0x17U);
    lcd_write_u8(0x07U);
    lcd_write_u8(0x11U);
    lcd_write_u8(0x06U);
    lcd_write_u8(0x2BU);
    lcd_write_u8(0x56U);
    lcd_write_u8(0x3CU);
    lcd_write_u8(0x05U);
    lcd_write_u8(0x10U);
    lcd_write_u8(0x0FU);
    lcd_write_u8(0x3FU);
    lcd_write_u8(0x3FU);
    lcd_write_u8(0x0FU);

    lcd_set_window(0U, 0U, TFT_LCD_WIDTH - 1U, TFT_LCD_HEIGHT - 1U);
    lcd_write_cmd(0x11U);
    HAL_Delay(120U);
    lcd_write_cmd(0x29U);
}

static void lcd_st7789_init(void)
{
    lcd_write_cmd(0x11U);
    HAL_Delay(120U);

    lcd_write_cmd(0x36U);
    lcd_write_u8(0x08U);

    lcd_write_cmd(0x3AU);
    lcd_write_u8(0x05U);

    lcd_write_cmd(0xB2U);
    lcd_write_u8(0x0CU);
    lcd_write_u8(0x0CU);
    lcd_write_u8(0x00U);
    lcd_write_u8(0x33U);
    lcd_write_u8(0x33U);

    lcd_write_cmd(0xB7U);
    lcd_write_u8(0x35U);

    lcd_write_cmd(0xBBU);
    lcd_write_u8(0x32U);

    lcd_write_cmd(0xC0U);
    lcd_write_u8(0x0CU);

    lcd_write_cmd(0xC2U);
    lcd_write_u8(0x01U);

    lcd_write_cmd(0xC3U);
    lcd_write_u8(0x10U);

    lcd_write_cmd(0xC4U);
    lcd_write_u8(0x20U);

    lcd_write_cmd(0xC6U);
    lcd_write_u8(0x0FU);

    lcd_write_cmd(0xD0U);
    lcd_write_u8(0xA4U);
    lcd_write_u8(0xA1U);

    lcd_write_cmd(0xE0U);
    lcd_write_u8(0xD0U);
    lcd_write_u8(0x00U);
    lcd_write_u8(0x02U);
    lcd_write_u8(0x07U);
    lcd_write_u8(0x0AU);
    lcd_write_u8(0x28U);
    lcd_write_u8(0x32U);
    lcd_write_u8(0x44U);
    lcd_write_u8(0x42U);
    lcd_write_u8(0x06U);
    lcd_write_u8(0x0EU);
    lcd_write_u8(0x12U);
    lcd_write_u8(0x14U);
    lcd_write_u8(0x17U);

    lcd_write_cmd(0xE1U);
    lcd_write_u8(0xD0U);
    lcd_write_u8(0x00U);
    lcd_write_u8(0x02U);
    lcd_write_u8(0x07U);
    lcd_write_u8(0x0AU);
    lcd_write_u8(0x28U);
    lcd_write_u8(0x31U);
    lcd_write_u8(0x54U);
    lcd_write_u8(0x47U);
    lcd_write_u8(0x0EU);
    lcd_write_u8(0x1CU);
    lcd_write_u8(0x17U);
    lcd_write_u8(0x1BU);
    lcd_write_u8(0x1EU);

    lcd_set_window(0U, 0U, TFT_LCD_WIDTH - 1U, TFT_LCD_HEIGHT - 1U);
    lcd_write_cmd(0x29U);
}

static void lcd_set_cursor(uint16_t x, uint16_t y)
{
    lcd_write_cmd(0x2AU);
    lcd_write_u8((uint8_t)(x >> 8U));
    lcd_write_u8((uint8_t)x);

    lcd_write_cmd(0x2BU);
    lcd_write_u8((uint8_t)(y >> 8U));
    lcd_write_u8((uint8_t)y);
}

static void lcd_prepare_write_ram(void)
{
    lcd_write_cmd(0x2CU);
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    lcd_write_cmd(0x2AU);
    lcd_write_u8((uint8_t)(x0 >> 8U));
    lcd_write_u8((uint8_t)x0);
    lcd_write_u8((uint8_t)(x1 >> 8U));
    lcd_write_u8((uint8_t)x1);

    lcd_write_cmd(0x2BU);
    lcd_write_u8((uint8_t)(y0 >> 8U));
    lcd_write_u8((uint8_t)y0);
    lcd_write_u8((uint8_t)(y1 >> 8U));
    lcd_write_u8((uint8_t)y1);

    lcd_write_cmd(0x2CU);
}

static void lcd_write_u8(uint8_t data)
{
    lcd_write_data(data);
}
