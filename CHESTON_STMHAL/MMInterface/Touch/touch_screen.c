#include "touch_screen.h"

#include <string.h>

#define GT9XXX_CMD_WR 0x28U
#define GT9XXX_CMD_RD 0x29U
#define GT9XXX_CTRL_REG 0x8040U
#define GT9XXX_PID_REG 0x8140U
#define GT9XXX_GSTID_REG 0x814EU
#define GT9XXX_TP1_REG 0x8150U

#define FT5206_CMD_WR 0x70U
#define FT5206_CMD_RD 0x71U
#define FT5206_DEVICE_MODE 0x00U
#define FT5206_REG_NUM_FINGER 0x02U
#define FT5206_TP1_REG 0x03U
#define FT5206_ID_G_THGROUP 0x80U
#define FT5206_ID_G_PERIODACTIVE 0x88U
#define FT5206_ID_G_LIB_VERSION 0xA1U
#define FT5206_ID_G_MODE 0xA4U

#define XPT2046_CMD_READ_X 0xD0U
#define XPT2046_CMD_READ_Y 0x90U
#define XPT2046_READ_TIMES 5U
#define XPT2046_ERR_RANGE 50U

static touch_screen_config_t g_config;
static touch_screen_chip_t g_chip = TOUCH_SCREEN_CHIP_NONE;
static uint8_t g_gt_point_count = 5U;

static void touch_delay_us(uint32_t us);
static void ct_iic_init(void);
static void ct_iic_start(void);
static void ct_iic_stop(void);
static void ct_iic_send_byte(uint8_t data);
static uint8_t ct_iic_read_byte(bool ack);
static bool ct_iic_wait_ack(void);
static void ct_iic_ack(void);
static void ct_iic_nack(void);

static bool gt9xxx_init(void);
static bool gt9xxx_scan(touch_screen_state_t* state);
static bool gt9xxx_write_reg(uint16_t reg, const uint8_t* data, uint8_t len);
static bool gt9xxx_read_reg(uint16_t reg, uint8_t* data, uint8_t len);

static bool ft5206_init(void);
static bool ft5206_scan(touch_screen_state_t* state);
static bool ft5206_write_reg(uint8_t reg, const uint8_t* data, uint8_t len);
static bool ft5206_read_reg(uint8_t reg, uint8_t* data, uint8_t len);

static void xpt2046_init(void);
static bool xpt2046_scan(touch_screen_state_t* state);
static void xpt2046_write_byte(uint8_t data);
static uint16_t xpt2046_read_ad(uint8_t cmd);
static uint16_t xpt2046_read_filtered(uint8_t cmd);
static bool xpt2046_read_xy(uint16_t* x, uint16_t* y);

static void clear_state(touch_screen_state_t* state);
static void transform_point(uint16_t raw_x, uint16_t raw_y, uint16_t* x, uint16_t* y);
static uint16_t scale_resistive_axis(uint16_t value, uint16_t in_min, uint16_t in_max, uint16_t out_max);
static void write_pin(GPIO_TypeDef* port, uint16_t pin, bool level);
static GPIO_PinState read_pin(GPIO_TypeDef* port, uint16_t pin);

void TouchScreen_GetDefaultConfig(touch_screen_config_t* config)
{
    if (config == NULL)
    {
        return;
    }

    config->mode = TOUCH_SCREEN_MODE_RESISTIVE;
    config->width = 240U;
    config->height = 320U;
    config->swap_xy = false;
    config->invert_x = false;
    config->invert_y = false;
    config->resistive_x_min = 200U;
    config->resistive_x_max = 3900U;
    config->resistive_y_min = 200U;
    config->resistive_y_max = 3900U;
}

bool TouchScreen_Init(const touch_screen_config_t* config)
{
    TouchScreen_GetDefaultConfig(&g_config);

    if (config != NULL)
    {
        g_config = *config;
    }

    g_chip = TOUCH_SCREEN_CHIP_NONE;
    g_gt_point_count = 5U;

    if (g_config.mode == TOUCH_SCREEN_MODE_AUTO ||
        g_config.mode == TOUCH_SCREEN_MODE_CAPACITIVE)
    {
        if (gt9xxx_init())
        {
            g_chip = TOUCH_SCREEN_CHIP_GT9XXX;
            return true;
        }

        if (ft5206_init())
        {
            g_chip = TOUCH_SCREEN_CHIP_FT5206;
            return true;
        }

        if (g_config.mode == TOUCH_SCREEN_MODE_CAPACITIVE)
        {
            return false;
        }
    }

    xpt2046_init();
    g_chip = TOUCH_SCREEN_CHIP_XPT2046;
    return true;
}

bool TouchScreen_Scan(touch_screen_state_t* state)
{
    if (state == NULL)
    {
        return false;
    }

    clear_state(state);
    state->chip = g_chip;

    switch (g_chip)
    {
    case TOUCH_SCREEN_CHIP_GT9XXX:
        return gt9xxx_scan(state);
    case TOUCH_SCREEN_CHIP_FT5206:
        return ft5206_scan(state);
    case TOUCH_SCREEN_CHIP_XPT2046:
        return xpt2046_scan(state);
    default:
        return false;
    }
}

touch_screen_chip_t TouchScreen_GetChip(void)
{
    return g_chip;
}

const char* TouchScreen_GetChipName(touch_screen_chip_t chip)
{
    switch (chip)
    {
    case TOUCH_SCREEN_CHIP_GT9XXX:
        return "GT9XXX";
    case TOUCH_SCREEN_CHIP_FT5206:
        return "FT5206/CST340";
    case TOUCH_SCREEN_CHIP_XPT2046:
        return "XPT2046";
    default:
        return "NONE";
    }
}

static void touch_delay_us(uint32_t us)
{
    uint32_t cycles = (SystemCoreClock / 1000000U / 8U) * us;

    if (cycles == 0U)
    {
        cycles = us;
    }

    while (cycles-- > 0U)
    {
        __NOP();
    }
}

static void ct_iic_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    gpio.Pin = TOUCH_CT_SCL_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(TOUCH_CT_SCL_GPIO_Port, &gpio);

    gpio.Pin = TOUCH_CT_SDA_Pin;
    HAL_GPIO_Init(TOUCH_CT_SDA_GPIO_Port, &gpio);

    ct_iic_stop();
}

static void ct_iic_start(void)
{
    write_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin, true);
    write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, true);
    touch_delay_us(2U);
    write_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin, false);
    touch_delay_us(2U);
    write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, false);
    touch_delay_us(2U);
}

static void ct_iic_stop(void)
{
    write_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin, false);
    touch_delay_us(2U);
    write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, true);
    touch_delay_us(2U);
    write_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin, true);
    touch_delay_us(2U);
}

static bool ct_iic_wait_ack(void)
{
    uint16_t wait = 0U;

    write_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin, true);
    touch_delay_us(2U);
    write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, true);
    touch_delay_us(2U);

    while (read_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin) == GPIO_PIN_SET)
    {
        if (++wait > 250U)
        {
            ct_iic_stop();
            return false;
        }

        touch_delay_us(2U);
    }

    write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, false);
    touch_delay_us(2U);
    return true;
}

static void ct_iic_ack(void)
{
    write_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin, false);
    touch_delay_us(2U);
    write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, true);
    touch_delay_us(2U);
    write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, false);
    touch_delay_us(2U);
    write_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin, true);
}

static void ct_iic_nack(void)
{
    write_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin, true);
    touch_delay_us(2U);
    write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, true);
    touch_delay_us(2U);
    write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, false);
    touch_delay_us(2U);
}

static void ct_iic_send_byte(uint8_t data)
{
    for (uint8_t i = 0U; i < 8U; i++)
    {
        write_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin, (data & 0x80U) != 0U);
        touch_delay_us(2U);
        write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, true);
        touch_delay_us(2U);
        write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, false);
        data <<= 1U;
    }

    write_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin, true);
}

static uint8_t ct_iic_read_byte(bool ack)
{
    uint8_t data = 0U;

    write_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin, true);

    for (uint8_t i = 0U; i < 8U; i++)
    {
        data <<= 1U;
        write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, true);
        touch_delay_us(2U);

        if (read_pin(TOUCH_CT_SDA_GPIO_Port, TOUCH_CT_SDA_Pin) == GPIO_PIN_SET)
        {
            data++;
        }

        write_pin(TOUCH_CT_SCL_GPIO_Port, TOUCH_CT_SCL_Pin, false);
        touch_delay_us(2U);
    }

    if (ack)
    {
        ct_iic_ack();
    }
    else
    {
        ct_iic_nack();
    }

    return data;
}

static bool gt9xxx_write_reg(uint16_t reg, const uint8_t* data, uint8_t len)
{
    bool ok = true;

    ct_iic_start();
    ct_iic_send_byte(GT9XXX_CMD_WR);
    ok = ok && ct_iic_wait_ack();
    ct_iic_send_byte((uint8_t)(reg >> 8U));
    ok = ok && ct_iic_wait_ack();
    ct_iic_send_byte((uint8_t)(reg & 0xFFU));
    ok = ok && ct_iic_wait_ack();

    for (uint8_t i = 0U; i < len && ok; i++)
    {
        ct_iic_send_byte(data[i]);
        ok = ct_iic_wait_ack();
    }

    ct_iic_stop();
    return ok;
}

static bool gt9xxx_read_reg(uint16_t reg, uint8_t* data, uint8_t len)
{
    bool ok = true;

    ct_iic_start();
    ct_iic_send_byte(GT9XXX_CMD_WR);
    ok = ok && ct_iic_wait_ack();
    ct_iic_send_byte((uint8_t)(reg >> 8U));
    ok = ok && ct_iic_wait_ack();
    ct_iic_send_byte((uint8_t)(reg & 0xFFU));
    ok = ok && ct_iic_wait_ack();

    ct_iic_start();
    ct_iic_send_byte(GT9XXX_CMD_RD);
    ok = ok && ct_iic_wait_ack();

    if (ok)
    {
        for (uint8_t i = 0U; i < len; i++)
        {
            data[i] = ct_iic_read_byte(i != (len - 1U));
        }
    }

    ct_iic_stop();
    return ok;
}

static bool gt9xxx_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t id[5] = {0};
    uint8_t value = 0U;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin = TOUCH_CT_RST_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(TOUCH_CT_RST_GPIO_Port, &gpio);

    gpio.Pin = TOUCH_CT_INT_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TOUCH_CT_INT_GPIO_Port, &gpio);

    ct_iic_init();
    write_pin(TOUCH_CT_RST_GPIO_Port, TOUCH_CT_RST_Pin, false);
    HAL_Delay(10U);
    write_pin(TOUCH_CT_RST_GPIO_Port, TOUCH_CT_RST_Pin, true);
    HAL_Delay(100U);

    gpio.Pin = TOUCH_CT_INT_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(TOUCH_CT_INT_GPIO_Port, &gpio);

    if (!gt9xxx_read_reg(GT9XXX_PID_REG, id, 4U))
    {
        return false;
    }

    if (strcmp((const char*)id, "911") != 0 &&
        strcmp((const char*)id, "9147") != 0 &&
        strcmp((const char*)id, "1158") != 0 &&
        strcmp((const char*)id, "9271") != 0)
    {
        return false;
    }

    if (strcmp((const char*)id, "9271") == 0)
    {
        g_gt_point_count = 10U;
    }

    value = 0x02U;
    (void)gt9xxx_write_reg(GT9XXX_CTRL_REG, &value, 1U);
    HAL_Delay(10U);
    value = 0x00U;
    (void)gt9xxx_write_reg(GT9XXX_CTRL_REG, &value, 1U);
    return true;
}

static bool gt9xxx_scan(touch_screen_state_t* state)
{
    uint8_t status = 0U;
    uint8_t clear = 0U;
    uint8_t buf[4];

    if (!gt9xxx_read_reg(GT9XXX_GSTID_REG, &status, 1U))
    {
        return false;
    }

    if ((status & 0x80U) != 0U)
    {
        (void)gt9xxx_write_reg(GT9XXX_GSTID_REG, &clear, 1U);
    }

    uint8_t points = status & 0x0FU;
    if ((status & 0x80U) == 0U || points == 0U || points > g_gt_point_count)
    {
        return false;
    }

    if (points > TOUCH_SCREEN_MAX_POINTS)
    {
        points = TOUCH_SCREEN_MAX_POINTS;
    }

    state->pressed = true;
    state->point_count = points;

    for (uint8_t i = 0U; i < points; i++)
    {
        if (gt9xxx_read_reg((uint16_t)(GT9XXX_TP1_REG + (8U * i)), buf, 4U))
        {
            uint16_t raw_x = (uint16_t)(((uint16_t)buf[1] << 8U) | buf[0]);
            uint16_t raw_y = (uint16_t)(((uint16_t)buf[3] << 8U) | buf[2]);
            state->raw_x[i] = raw_x;
            state->raw_y[i] = raw_y;
            transform_point(raw_x, raw_y, &state->x[i], &state->y[i]);
        }
    }

    return true;
}

static bool ft5206_write_reg(uint8_t reg, const uint8_t* data, uint8_t len)
{
    bool ok = true;

    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_WR);
    ok = ok && ct_iic_wait_ack();
    ct_iic_send_byte(reg);
    ok = ok && ct_iic_wait_ack();

    for (uint8_t i = 0U; i < len && ok; i++)
    {
        ct_iic_send_byte(data[i]);
        ok = ct_iic_wait_ack();
    }

    ct_iic_stop();
    return ok;
}

static bool ft5206_read_reg(uint8_t reg, uint8_t* data, uint8_t len)
{
    bool ok = true;

    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_WR);
    ok = ok && ct_iic_wait_ack();
    ct_iic_send_byte(reg);
    ok = ok && ct_iic_wait_ack();
    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_RD);
    ok = ok && ct_iic_wait_ack();

    if (ok)
    {
        for (uint8_t i = 0U; i < len; i++)
        {
            data[i] = ct_iic_read_byte(i != (len - 1U));
        }
    }

    ct_iic_stop();
    return ok;
}

static bool ft5206_init(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint8_t temp[2] = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    gpio.Pin = TOUCH_CT_RST_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(TOUCH_CT_RST_GPIO_Port, &gpio);

    gpio.Pin = TOUCH_CT_INT_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(TOUCH_CT_INT_GPIO_Port, &gpio);

    ct_iic_init();
    write_pin(TOUCH_CT_RST_GPIO_Port, TOUCH_CT_RST_Pin, false);
    HAL_Delay(20U);
    write_pin(TOUCH_CT_RST_GPIO_Port, TOUCH_CT_RST_Pin, true);
    HAL_Delay(50U);

    temp[0] = 0U;
    (void)ft5206_write_reg(FT5206_DEVICE_MODE, temp, 1U);
    (void)ft5206_write_reg(FT5206_ID_G_MODE, temp, 1U);
    temp[0] = 22U;
    (void)ft5206_write_reg(FT5206_ID_G_THGROUP, temp, 1U);
    temp[0] = 12U;
    (void)ft5206_write_reg(FT5206_ID_G_PERIODACTIVE, temp, 1U);

    if (!ft5206_read_reg(FT5206_ID_G_LIB_VERSION, temp, 2U))
    {
        return false;
    }

    return ((temp[0] == 0x30U && temp[1] == 0x03U) ||
            temp[1] == 0x01U ||
            temp[1] == 0x02U ||
            (temp[0] == 0x00U && temp[1] == 0x00U));
}

static bool ft5206_scan(touch_screen_state_t* state)
{
    uint8_t status = 0U;
    uint8_t buf[4];

    if (!ft5206_read_reg(FT5206_REG_NUM_FINGER, &status, 1U))
    {
        return false;
    }

    uint8_t points = status & 0x0FU;
    if (points == 0U || points > 5U)
    {
        return false;
    }

    state->pressed = true;
    state->point_count = points;

    for (uint8_t i = 0U; i < points; i++)
    {
        if (ft5206_read_reg((uint8_t)(FT5206_TP1_REG + (6U * i)), buf, 4U))
        {
            if ((buf[0] & 0xF0U) != 0x80U)
            {
                continue;
            }

            uint16_t raw_x = (uint16_t)(((uint16_t)(buf[0] & 0x0FU) << 8U) | buf[1]);
            uint16_t raw_y = (uint16_t)(((uint16_t)(buf[2] & 0x0FU) << 8U) | buf[3]);
            state->raw_x[i] = raw_x;
            state->raw_y[i] = raw_y;
            transform_point(raw_x, raw_y, &state->x[i], &state->y[i]);
        }
    }

    return true;
}

static void xpt2046_init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    gpio.Pin = TOUCH_RES_PEN_Pin;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(TOUCH_RES_PEN_GPIO_Port, &gpio);

    gpio.Pin = TOUCH_RES_MISO_Pin;
    HAL_GPIO_Init(TOUCH_RES_MISO_GPIO_Port, &gpio);

    gpio.Pin = TOUCH_RES_MOSI_Pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(TOUCH_RES_MOSI_GPIO_Port, &gpio);

    gpio.Pin = TOUCH_RES_CLK_Pin;
    HAL_GPIO_Init(TOUCH_RES_CLK_GPIO_Port, &gpio);

    gpio.Pin = TOUCH_RES_CS_Pin;
    HAL_GPIO_Init(TOUCH_RES_CS_GPIO_Port, &gpio);

    write_pin(TOUCH_RES_CS_GPIO_Port, TOUCH_RES_CS_Pin, true);
    write_pin(TOUCH_RES_CLK_GPIO_Port, TOUCH_RES_CLK_Pin, true);
}

static bool xpt2046_scan(touch_screen_state_t* state)
{
    uint16_t raw_x = 0U;
    uint16_t raw_y = 0U;

    if (read_pin(TOUCH_RES_PEN_GPIO_Port, TOUCH_RES_PEN_Pin) != GPIO_PIN_RESET)
    {
        return false;
    }

    if (!xpt2046_read_xy(&raw_x, &raw_y))
    {
        return false;
    }

    state->pressed = true;
    state->point_count = 1U;
    state->raw_x[0] = raw_x;
    state->raw_y[0] = raw_y;

    uint16_t x = scale_resistive_axis(raw_x,
                                      g_config.resistive_x_min,
                                      g_config.resistive_x_max,
                                      g_config.width);
    uint16_t y = scale_resistive_axis(raw_y,
                                      g_config.resistive_y_min,
                                      g_config.resistive_y_max,
                                      g_config.height);
    transform_point(x, y, &state->x[0], &state->y[0]);
    return true;
}

static void xpt2046_write_byte(uint8_t data)
{
    for (uint8_t i = 0U; i < 8U; i++)
    {
        write_pin(TOUCH_RES_MOSI_GPIO_Port, TOUCH_RES_MOSI_Pin, (data & 0x80U) != 0U);
        data <<= 1U;
        write_pin(TOUCH_RES_CLK_GPIO_Port, TOUCH_RES_CLK_Pin, false);
        touch_delay_us(1U);
        write_pin(TOUCH_RES_CLK_GPIO_Port, TOUCH_RES_CLK_Pin, true);
    }
}

static uint16_t xpt2046_read_ad(uint8_t cmd)
{
    uint16_t value = 0U;

    write_pin(TOUCH_RES_CLK_GPIO_Port, TOUCH_RES_CLK_Pin, false);
    write_pin(TOUCH_RES_MOSI_GPIO_Port, TOUCH_RES_MOSI_Pin, false);
    write_pin(TOUCH_RES_CS_GPIO_Port, TOUCH_RES_CS_Pin, false);
    xpt2046_write_byte(cmd);
    touch_delay_us(6U);
    write_pin(TOUCH_RES_CLK_GPIO_Port, TOUCH_RES_CLK_Pin, false);
    touch_delay_us(1U);
    write_pin(TOUCH_RES_CLK_GPIO_Port, TOUCH_RES_CLK_Pin, true);
    touch_delay_us(1U);
    write_pin(TOUCH_RES_CLK_GPIO_Port, TOUCH_RES_CLK_Pin, false);

    for (uint8_t i = 0U; i < 16U; i++)
    {
        value <<= 1U;
        write_pin(TOUCH_RES_CLK_GPIO_Port, TOUCH_RES_CLK_Pin, false);
        touch_delay_us(1U);
        write_pin(TOUCH_RES_CLK_GPIO_Port, TOUCH_RES_CLK_Pin, true);

        if (read_pin(TOUCH_RES_MISO_GPIO_Port, TOUCH_RES_MISO_Pin) == GPIO_PIN_SET)
        {
            value++;
        }
    }

    value >>= 4U;
    write_pin(TOUCH_RES_CS_GPIO_Port, TOUCH_RES_CS_Pin, true);
    return value;
}

static uint16_t xpt2046_read_filtered(uint8_t cmd)
{
    uint16_t buf[XPT2046_READ_TIMES];
    uint32_t sum = 0U;

    for (uint8_t i = 0U; i < XPT2046_READ_TIMES; i++)
    {
        buf[i] = xpt2046_read_ad(cmd);
    }

    for (uint8_t i = 0U; i < XPT2046_READ_TIMES - 1U; i++)
    {
        for (uint8_t j = i + 1U; j < XPT2046_READ_TIMES; j++)
        {
            if (buf[i] > buf[j])
            {
                uint16_t tmp = buf[i];
                buf[i] = buf[j];
                buf[j] = tmp;
            }
        }
    }

    for (uint8_t i = 1U; i < XPT2046_READ_TIMES - 1U; i++)
    {
        sum += buf[i];
    }

    return (uint16_t)(sum / (XPT2046_READ_TIMES - 2U));
}

static bool xpt2046_read_xy(uint16_t* x, uint16_t* y)
{
    uint16_t x1 = xpt2046_read_filtered(XPT2046_CMD_READ_X);
    uint16_t y1 = xpt2046_read_filtered(XPT2046_CMD_READ_Y);
    uint16_t x2 = xpt2046_read_filtered(XPT2046_CMD_READ_X);
    uint16_t y2 = xpt2046_read_filtered(XPT2046_CMD_READ_Y);

    uint16_t dx = (x1 > x2) ? (uint16_t)(x1 - x2) : (uint16_t)(x2 - x1);
    uint16_t dy = (y1 > y2) ? (uint16_t)(y1 - y2) : (uint16_t)(y2 - y1);

    if (dx >= XPT2046_ERR_RANGE || dy >= XPT2046_ERR_RANGE)
    {
        return false;
    }

    *x = (uint16_t)((x1 + x2) / 2U);
    *y = (uint16_t)((y1 + y2) / 2U);
    return true;
}

static void clear_state(touch_screen_state_t* state)
{
    touch_screen_chip_t chip = state->chip;
    memset(state, 0, sizeof(*state));
    state->chip = chip;

    for (uint8_t i = 0U; i < TOUCH_SCREEN_MAX_POINTS; i++)
    {
        state->x[i] = 0xFFFFU;
        state->y[i] = 0xFFFFU;
        state->raw_x[i] = 0xFFFFU;
        state->raw_y[i] = 0xFFFFU;
    }
}

static void transform_point(uint16_t raw_x, uint16_t raw_y, uint16_t* x, uint16_t* y)
{
    uint16_t tx = raw_x;
    uint16_t ty = raw_y;

    if (g_config.swap_xy)
    {
        uint16_t tmp = tx;
        tx = ty;
        ty = tmp;
    }

    if (g_config.invert_x && g_config.width > 0U)
    {
        tx = (tx >= g_config.width) ? 0U : (uint16_t)(g_config.width - 1U - tx);
    }

    if (g_config.invert_y && g_config.height > 0U)
    {
        ty = (ty >= g_config.height) ? 0U : (uint16_t)(g_config.height - 1U - ty);
    }

    *x = tx;
    *y = ty;
}

static uint16_t scale_resistive_axis(uint16_t value, uint16_t in_min, uint16_t in_max, uint16_t out_max)
{
    if (out_max == 0U || in_min == in_max)
    {
        return 0U;
    }

    if (value <= in_min)
    {
        return 0U;
    }

    if (value >= in_max)
    {
        return (uint16_t)(out_max - 1U);
    }

    return (uint16_t)(((uint32_t)(value - in_min) * (uint32_t)(out_max - 1U)) /
                      (uint32_t)(in_max - in_min));
}

static void write_pin(GPIO_TypeDef* port, uint16_t pin, bool level)
{
    HAL_GPIO_WritePin(port, pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static GPIO_PinState read_pin(GPIO_TypeDef* port, uint16_t pin)
{
    return HAL_GPIO_ReadPin(port, pin);
}
