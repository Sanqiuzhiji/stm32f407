#include "App/RemoteTest/remote_test.h"

#include "MMInterface/LCD/tft_lcd.h"
#include "MMInterface/Remote/remote.h"

#include <stdio.h>

#define REMOTE_TEST_COLOR_BG 0x0000U
#define REMOTE_TEST_COLOR_TEXT 0xFFFFU
#define REMOTE_TEST_COLOR_TITLE 0x07E0U
#define REMOTE_TEST_COLOR_LABEL 0xFFE0U
#define REMOTE_TEST_COLOR_VALUE 0x07FFU
#define REMOTE_TEST_COLOR_FRAME 0x39E7U

static bool remote_test_active = false;
static bool remote_test_lcd_enabled = false;

static void remote_test_draw_static(void);
static void remote_test_draw_event(const remote_key_event_t* event);
static void draw_text(uint16_t x, uint16_t y, const char* text, uint16_t color, uint8_t scale);
static void draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint8_t scale);
static void draw_scaled_pixel(uint16_t x, uint16_t y, uint16_t color, uint8_t scale);
static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
static void draw_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
static void format_hex8(char* dst, uint32_t value);
static void format_hex2(char* dst, uint8_t value);
static void format_dec3(char* dst, uint8_t value);
static void remote_test_print_event(const remote_key_event_t* event);
static const uint8_t* glyph_for(char c);

void RemoteTest_Init(bool lcd_enabled)
{
    remote_test_active = true;
    remote_test_lcd_enabled = lcd_enabled;

    if (remote_test_lcd_enabled)
    {
        remote_test_draw_static();
    }

    printf("Remote test ready: PA8 TIM1_CH1 NEC decoder\r\n");
}

void RemoteTest_TaskTick(void)
{
    if (!remote_test_active)
    {
        return;
    }

    remote_key_event_t event;
    if (!Remote_PollEvent(&event))
    {
        return;
    }

    remote_test_print_event(&event);

    if (remote_test_lcd_enabled)
    {
        remote_test_draw_event(&event);
    }
}

bool RemoteTest_IsActive(void)
{
    return remote_test_active;
}

static void remote_test_draw_static(void)
{
    TftLcd_Clear(REMOTE_TEST_COLOR_BG);
    draw_box(4U, 4U, 232U, 312U, REMOTE_TEST_COLOR_FRAME);
    draw_text(18U, 18U, "REMOTE TEST", REMOTE_TEST_COLOR_TITLE, 2U);
    draw_text(18U, 50U, "PA8 TIM1 CH1", REMOTE_TEST_COLOR_TEXT, 1U);
    draw_text(18U, 74U, "WAIT SIGNAL", REMOTE_TEST_COLOR_LABEL, 1U);

    draw_text(18U, 112U, "KEY", REMOTE_TEST_COLOR_LABEL, 2U);
    draw_text(18U, 176U, "COUNT", REMOTE_TEST_COLOR_LABEL, 2U);
    draw_text(18U, 236U, "RAW", REMOTE_TEST_COLOR_LABEL, 2U);

    draw_text(100U, 108U, "0X00", REMOTE_TEST_COLOR_VALUE, 3U);
    draw_text(126U, 172U, "000", REMOTE_TEST_COLOR_VALUE, 3U);
    draw_text(78U, 236U, "00000000", REMOTE_TEST_COLOR_VALUE, 2U);
}

static void remote_test_draw_event(const remote_key_event_t* event)
{
    char key_text[] = "0X00";
    char count_text[] = "000";
    char raw_text[] = "00000000";

    format_hex2(&key_text[2], event->key);
    format_dec3(count_text, event->repeat_count);
    format_hex8(raw_text, event->raw_data);

    fill_rect(18U, 74U, 120U, 8U, REMOTE_TEST_COLOR_BG);
    draw_text(18U, 74U, "RECEIVED", REMOTE_TEST_COLOR_TITLE, 1U);

    fill_rect(100U, 108U, 116U, 24U, REMOTE_TEST_COLOR_BG);
    draw_text(100U, 108U, key_text, REMOTE_TEST_COLOR_VALUE, 3U);

    fill_rect(126U, 172U, 90U, 24U, REMOTE_TEST_COLOR_BG);
    draw_text(126U, 172U, count_text, REMOTE_TEST_COLOR_VALUE, 3U);

    fill_rect(78U, 236U, 132U, 16U, REMOTE_TEST_COLOR_BG);
    draw_text(78U, 236U, raw_text, REMOTE_TEST_COLOR_VALUE, 2U);
}

static void draw_text(uint16_t x, uint16_t y, const char* text, uint16_t color, uint8_t scale)
{
    uint16_t cursor_x = x;

    while (*text != '\0')
    {
        draw_char(cursor_x, y, *text, color, scale);
        cursor_x = (uint16_t)(cursor_x + (6U * scale));
        text++;
    }
}

static void draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint8_t scale)
{
    const uint8_t* glyph = glyph_for(c);

    for (uint8_t col = 0U; col < 5U; col++)
    {
        uint8_t bits = glyph[col];
        for (uint8_t row = 0U; row < 7U; row++)
        {
            if ((bits & (1U << row)) != 0U)
            {
                draw_scaled_pixel((uint16_t)(x + (col * scale)),
                                  (uint16_t)(y + (row * scale)),
                                  color,
                                  scale);
            }
        }
    }
}

static void draw_scaled_pixel(uint16_t x, uint16_t y, uint16_t color, uint8_t scale)
{
    for (uint8_t dy = 0U; dy < scale; dy++)
    {
        for (uint8_t dx = 0U; dx < scale; dx++)
        {
            TftLcd_DrawPixel((uint16_t)(x + dx), (uint16_t)(y + dy), color);
        }
    }
}

static void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    static uint16_t line[TFT_LCD_WIDTH];

    if (x >= TFT_LCD_WIDTH || y >= TFT_LCD_HEIGHT || w == 0U || h == 0U)
    {
        return;
    }

    if ((uint32_t)x + w > TFT_LCD_WIDTH)
    {
        w = (uint16_t)(TFT_LCD_WIDTH - x);
    }

    if ((uint32_t)y + h > TFT_LCD_HEIGHT)
    {
        h = (uint16_t)(TFT_LCD_HEIGHT - y);
    }

    for (uint16_t i = 0U; i < w; i++)
    {
        line[i] = color;
    }

    for (uint16_t row = 0U; row < h; row++)
    {
        TftLcd_DrawRgb565Line(x, (uint16_t)(y + row), line, w);
    }
}

static void draw_box(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    TftLcd_DrawLine(x, y, (uint16_t)(x + w - 1U), y, color);
    TftLcd_DrawLine(x, (uint16_t)(y + h - 1U), (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U), color);
    TftLcd_DrawLine(x, y, x, (uint16_t)(y + h - 1U), color);
    TftLcd_DrawLine((uint16_t)(x + w - 1U), y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U), color);
}

static void format_hex8(char* dst, uint32_t value)
{
    static const char hex[] = "0123456789ABCDEF";

    for (uint8_t i = 0U; i < 8U; i++)
    {
        uint8_t shift = (uint8_t)(28U - (i * 4U));
        dst[i] = hex[(value >> shift) & 0x0FU];
    }
}

static void format_hex2(char* dst, uint8_t value)
{
    static const char hex[] = "0123456789ABCDEF";

    dst[0] = hex[(value >> 4U) & 0x0FU];
    dst[1] = hex[value & 0x0FU];
}

static void format_dec3(char* dst, uint8_t value)
{
    dst[0] = (char)('0' + (value / 100U));
    dst[1] = (char)('0' + ((value / 10U) % 10U));
    dst[2] = (char)('0' + (value % 10U));
}

static void remote_test_print_event(const remote_key_event_t* event)
{
    char line[] = "Remote key=0x00 repeat=000 raw=0x00000000\r\n";

    format_hex2(&line[13], event->key);
    format_dec3(&line[23], event->repeat_count);
    format_hex8(&line[33], event->raw_data);

    fputs(line, stdout);
}

static const uint8_t* glyph_for(char c)
{
    static const uint8_t blank[5] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
    static const uint8_t glyphs[][5] = {
        {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU}, /* 0 */
        {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U}, /* 1 */
        {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}, /* 2 */
        {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U}, /* 3 */
        {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U}, /* 4 */
        {0x27U, 0x45U, 0x45U, 0x45U, 0x39U}, /* 5 */
        {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U}, /* 6 */
        {0x01U, 0x71U, 0x09U, 0x05U, 0x03U}, /* 7 */
        {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}, /* 8 */
        {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}, /* 9 */
        {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU}, /* A */
        {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U}, /* B */
        {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U}, /* C */
        {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU}, /* D */
        {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U}, /* E */
        {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U}, /* F */
        {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU}, /* G */
        {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU}, /* H */
        {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U}, /* I */
        {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U}, /* J */
        {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U}, /* K */
        {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U}, /* L */
        {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU}, /* M */
        {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU}, /* N */
        {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU}, /* O */
        {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U}, /* P */
        {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU}, /* Q */
        {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U}, /* R */
        {0x46U, 0x49U, 0x49U, 0x49U, 0x31U}, /* S */
        {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U}, /* T */
        {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU}, /* U */
        {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU}, /* V */
        {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU}, /* W */
        {0x63U, 0x14U, 0x08U, 0x14U, 0x63U}, /* X */
        {0x07U, 0x08U, 0x70U, 0x08U, 0x07U}, /* Y */
        {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}, /* Z */
    };

    if (c >= '0' && c <= '9')
    {
        return glyphs[c - '0'];
    }

    if (c >= 'a' && c <= 'z')
    {
        c = (char)(c - ('a' - 'A'));
    }

    if (c >= 'A' && c <= 'Z')
    {
        return glyphs[10 + (c - 'A')];
    }

    return blank;
}
