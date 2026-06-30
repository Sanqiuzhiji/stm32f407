#include "MMInterface/Remote/remote.h"

#include "main.h"

#define REMOTE_ID 0U

#define REMOTE_STATE_LEADER_RECEIVED 0x80U
#define REMOTE_STATE_FRAME_READY 0x40U
#define REMOTE_STATE_RISING_CAPTURED 0x10U
#define REMOTE_STATE_OVERFLOW_MASK 0x0FU

static TIM_HandleTypeDef* remote_timer = NULL;
static uint32_t remote_channel = TIM_CHANNEL_1;

static volatile uint8_t remote_state = 0U;
static volatile uint32_t remote_data = 0U;
static volatile uint8_t remote_repeat_count = 0U;
static volatile uint8_t remote_last_key = 0U;
static volatile bool remote_event_pending = false;
static volatile remote_key_event_t remote_pending_event = {0U, 0U, 0U};

static GPIO_PinState remote_read_pin(void);
static void remote_push_event_from_isr(uint8_t key, uint32_t raw_data);
static bool remote_try_decode_frame(uint8_t* key, uint32_t* raw_data);
static bool remote_is_own_timer(const TIM_HandleTypeDef* htim);
static uint32_t remote_enter_critical(void);
static void remote_exit_critical(uint32_t primask);

bool Remote_Init(TIM_HandleTypeDef* htim, uint32_t channel)
{
    if (htim == NULL)
    {
        return false;
    }

    remote_timer = htim;
    remote_channel = channel;
    remote_state = 0U;
    remote_data = 0U;
    remote_repeat_count = 0U;
    remote_last_key = 0U;
    remote_event_pending = false;

    __HAL_TIM_SET_COUNTER(remote_timer, 0U);

    if (HAL_TIM_IC_Start_IT(remote_timer, remote_channel) != HAL_OK)
    {
        remote_timer = NULL;
        return false;
    }

    __HAL_TIM_ENABLE_IT(remote_timer, TIM_IT_UPDATE);
    return true;
}

bool Remote_PollEvent(remote_key_event_t* event)
{
    if (event == NULL)
    {
        return false;
    }

    uint8_t key = 0U;
    uint32_t raw_data = 0U;

    if (remote_try_decode_frame(&key, &raw_data))
    {
        uint32_t primask = remote_enter_critical();
        remote_last_key = key;
        remote_pending_event.key = key;
        remote_pending_event.repeat_count = remote_repeat_count;
        remote_pending_event.raw_data = raw_data;
        remote_event_pending = true;
        remote_exit_critical(primask);
    }

    uint32_t primask = remote_enter_critical();
    bool pending = remote_event_pending;
    if (pending)
    {
        *event = remote_pending_event;
        remote_event_pending = false;
    }
    remote_exit_critical(primask);

    return pending;
}

void Remote_OnTimerPeriodElapsed(TIM_HandleTypeDef* htim)
{
    if (!remote_is_own_timer(htim))
    {
        return;
    }

    if ((remote_state & REMOTE_STATE_LEADER_RECEIVED) != 0U)
    {
        remote_state &= (uint8_t)~REMOTE_STATE_RISING_CAPTURED;

        if ((remote_state & REMOTE_STATE_OVERFLOW_MASK) == 0U)
        {
            remote_state |= REMOTE_STATE_FRAME_READY;
        }

        if ((remote_state & REMOTE_STATE_OVERFLOW_MASK) < 14U)
        {
            remote_state++;
        }
        else
        {
            remote_state &= (uint8_t)~REMOTE_STATE_LEADER_RECEIVED;
            remote_state &= 0xF0U;
        }
    }
}

void Remote_OnInputCapture(TIM_HandleTypeDef* htim)
{
    if (!remote_is_own_timer(htim))
    {
        return;
    }

    if (remote_read_pin() == GPIO_PIN_SET)
    {
        __HAL_TIM_SET_CAPTUREPOLARITY(remote_timer, remote_channel, TIM_INPUTCHANNELPOLARITY_FALLING);
        __HAL_TIM_SET_COUNTER(remote_timer, 0U);
        remote_state |= REMOTE_STATE_RISING_CAPTURED;
        return;
    }

    uint16_t pulse_us = (uint16_t)HAL_TIM_ReadCapturedValue(remote_timer, remote_channel);
    __HAL_TIM_SET_CAPTUREPOLARITY(remote_timer, remote_channel, TIM_INPUTCHANNELPOLARITY_RISING);

    if ((remote_state & REMOTE_STATE_RISING_CAPTURED) != 0U)
    {
        if ((remote_state & REMOTE_STATE_LEADER_RECEIVED) != 0U)
        {
            if (pulse_us > 300U && pulse_us < 800U)
            {
                remote_data >>= 1U;
                remote_data &= ~(0x80000000UL);
            }
            else if (pulse_us > 1400U && pulse_us < 1800U)
            {
                remote_data >>= 1U;
                remote_data |= 0x80000000UL;
            }
            else if (pulse_us > 2000U && pulse_us < 3000U)
            {
                remote_repeat_count++;

                if (remote_last_key != 0U)
                {
                    remote_push_event_from_isr(remote_last_key, remote_data);
                }

                remote_state &= 0xF0U;
            }
        }
        else if (pulse_us > 4200U && pulse_us < 4700U)
        {
            remote_state |= REMOTE_STATE_LEADER_RECEIVED;
            remote_repeat_count = 0U;
            remote_data = 0U;
        }
    }

    remote_state &= (uint8_t)~REMOTE_STATE_RISING_CAPTURED;
}

static GPIO_PinState remote_read_pin(void)
{
    return HAL_GPIO_ReadPin(REMOTE_IN_GPIO_Port, REMOTE_IN_Pin);
}

static void remote_push_event_from_isr(uint8_t key, uint32_t raw_data)
{
    remote_pending_event.key = key;
    remote_pending_event.repeat_count = remote_repeat_count;
    remote_pending_event.raw_data = raw_data;
    remote_event_pending = true;
}

static bool remote_try_decode_frame(uint8_t* key, uint32_t* raw_data)
{
    uint8_t state_snapshot;
    uint32_t data_snapshot;

    uint32_t primask = remote_enter_critical();
    state_snapshot = remote_state;
    if ((state_snapshot & REMOTE_STATE_FRAME_READY) == 0U)
    {
        remote_exit_critical(primask);
        return false;
    }

    data_snapshot = remote_data;
    remote_state &= (uint8_t)~REMOTE_STATE_FRAME_READY;
    remote_exit_critical(primask);

    uint8_t address = (uint8_t)data_snapshot;
    uint8_t address_inverse = (uint8_t)(data_snapshot >> 8U);
    uint8_t command = (uint8_t)(data_snapshot >> 16U);
    uint8_t command_inverse = (uint8_t)(data_snapshot >> 24U);

    if (address != REMOTE_ID || address != (uint8_t)~address_inverse)
    {
        primask = remote_enter_critical();
        remote_repeat_count = 0U;
        remote_exit_critical(primask);
        return false;
    }

    if (command != (uint8_t)~command_inverse)
    {
        primask = remote_enter_critical();
        remote_repeat_count = 0U;
        remote_exit_critical(primask);
        return false;
    }

    *key = command;
    *raw_data = data_snapshot;
    return true;
}

static bool remote_is_own_timer(const TIM_HandleTypeDef* htim)
{
    return (remote_timer != NULL && htim != NULL && htim->Instance == remote_timer->Instance);
}

static uint32_t remote_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static void remote_exit_critical(uint32_t primask)
{
    if (primask == 0U)
    {
        __enable_irq();
    }
}
