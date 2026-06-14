#ifndef PLOT_TEST_H
#define PLOT_TEST_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32f4xx_hal.h"

#define PLOT_TEST_DEFAULT_CHANNELS 4U
#define PLOT_TEST_DEFAULT_INTERVAL_MS 5U

typedef bool (*plot_test_send_func_t)(const uint8_t* data, uint16_t len, void* user);

typedef struct
{
    plot_test_send_func_t send;
    void* user;
    uint8_t channel_count;
    uint16_t interval_ms;
    bool auto_start;
} plot_test_config_t;

void PlotTest_Init(const plot_test_config_t* config);
void PlotTest_TaskTick(uint32_t now_ms);
bool PlotTest_HandleCommand(const uint8_t* data, uint16_t len);

bool PlotTest_UartDmaSend(const uint8_t* data, uint16_t len, void* user);

#endif
