#include "App/PlotTest/plot_test.h"

#include "Protocol/justfloat_protocol.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    plot_test_send_func_t send;
    void* user;
    uint8_t channel_count;
    uint16_t interval_ms;
    bool enabled;
    uint32_t next_send_ms;
    uint32_t sample_index;
    justfloat_frame_t frame;
    float values[JUSTFLOAT_MAX_CHANNELS];
} plot_test_t;

static plot_test_t g_plot_test;

static uint8_t clamp_channel_count(uint8_t channel_count);
static uint16_t clamp_interval_ms(uint16_t interval_ms);
static void generate_values(plot_test_t* test);
static float triangle_wave(uint32_t phase, uint32_t period);
static bool command_equals(const uint8_t* data, uint16_t len, const char* command);
static bool command_scan_u16(const uint8_t* data, uint16_t len, const char* format, uint16_t* value);

void PlotTest_Init(const plot_test_config_t* config)
{
    memset(&g_plot_test, 0, sizeof(g_plot_test));

    g_plot_test.channel_count = PLOT_TEST_DEFAULT_CHANNELS;
    g_plot_test.interval_ms = PLOT_TEST_DEFAULT_INTERVAL_MS;

    if (config != NULL)
    {
        g_plot_test.send = config->send;
        g_plot_test.user = config->user;
        g_plot_test.channel_count = clamp_channel_count(config->channel_count);
        g_plot_test.interval_ms = clamp_interval_ms(config->interval_ms);
        g_plot_test.enabled = config->auto_start;
    }
}

void PlotTest_TaskTick(uint32_t now_ms)
{
    if (!g_plot_test.enabled || g_plot_test.send == NULL)
    {
        return;
    }

    if ((int32_t)(now_ms - g_plot_test.next_send_ms) < 0)
    {
        return;
    }

    generate_values(&g_plot_test);
    uint16_t frame_len = JustFloat_BuildFrame(&g_plot_test.frame,
                                              g_plot_test.values,
                                              g_plot_test.channel_count);
    if (frame_len == 0U)
    {
        return;
    }

    if (g_plot_test.send((const uint8_t*)&g_plot_test.frame, frame_len, g_plot_test.user))
    {
        g_plot_test.sample_index++;
        g_plot_test.next_send_ms = now_ms + g_plot_test.interval_ms;
    }
}

bool PlotTest_HandleCommand(const uint8_t* data, uint16_t len)
{
    uint16_t value = 0U;

    if (command_equals(data, len, "plot start"))
    {
        g_plot_test.enabled = true;
        return true;
    }

    if (command_equals(data, len, "plot stop"))
    {
        g_plot_test.enabled = false;
        return true;
    }

    if (command_scan_u16(data, len, "plot rate %hu", &value))
    {
        g_plot_test.interval_ms = clamp_interval_ms(value);
        return true;
    }

    if (command_scan_u16(data, len, "plot ch %hu", &value))
    {
        g_plot_test.channel_count = clamp_channel_count((uint8_t)value);
        return true;
    }

    if (command_equals(data, len, "plot status"))
    {
        return true;
    }

    return false;
}

bool PlotTest_UartDmaSend(const uint8_t* data, uint16_t len, void* user)
{
    UART_HandleTypeDef* huart = (UART_HandleTypeDef*)user;

    if (huart == NULL || data == NULL || len == 0U || huart->hdmatx == NULL)
    {
        return false;
    }

    if (huart->gState != HAL_UART_STATE_READY ||
        huart->hdmatx->State != HAL_DMA_STATE_READY)
    {
        return false;
    }

    return (HAL_UART_Transmit_DMA(huart, (uint8_t*)data, len) == HAL_OK);
}

static uint8_t clamp_channel_count(uint8_t channel_count)
{
    if (channel_count == 0U)
    {
        return PLOT_TEST_DEFAULT_CHANNELS;
    }

    if (channel_count > JUSTFLOAT_MAX_CHANNELS)
    {
        return JUSTFLOAT_MAX_CHANNELS;
    }

    return channel_count;
}

static uint16_t clamp_interval_ms(uint16_t interval_ms)
{
    if (interval_ms == 0U)
    {
        return 1U;
    }

    return interval_ms;
}

static void generate_values(plot_test_t* test)
{
    for (uint8_t i = 0U; i < test->channel_count; i++)
    {
        uint32_t period = 80U + ((uint32_t)i * 24U);
        uint32_t phase = test->sample_index + ((uint32_t)i * period / 4U);
        float wave = triangle_wave(phase, period);

        if (i == 1U)
        {
            wave = (phase % period) < (period / 2U) ? 1.0f : -1.0f;
        }
        else if (i == 2U)
        {
            wave = ((float)(phase % period) / (float)period) * 2.0f - 1.0f;
        }

        test->values[i] = wave + ((float)i * 1.5f);
    }
}

static float triangle_wave(uint32_t phase, uint32_t period)
{
    uint32_t x = phase % period;
    uint32_t half = period / 2U;

    if (x < half)
    {
        return ((float)x / (float)half) * 2.0f - 1.0f;
    }

    return 1.0f - (((float)(x - half) / (float)half) * 2.0f);
}

static bool command_equals(const uint8_t* data, uint16_t len, const char* command)
{
    uint16_t command_len = (uint16_t)strlen(command);

    while (len > 0U && (data[len - 1U] == '\r' || data[len - 1U] == '\n' || data[len - 1U] == ' '))
    {
        len--;
    }

    return (len == command_len && memcmp(data, command, command_len) == 0);
}

static bool command_scan_u16(const uint8_t* data, uint16_t len, const char* format, uint16_t* value)
{
    char buffer[32];

    if (len >= sizeof(buffer))
    {
        len = sizeof(buffer) - 1U;
    }

    memcpy(buffer, data, len);
    buffer[len] = '\0';

    return (sscanf(buffer, format, value) == 1);
}
