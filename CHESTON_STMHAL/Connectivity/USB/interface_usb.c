#include "Connectivity/USB/interface_usb.h"

#include "usbd_cdc_if.h"

#include <string.h>

#define USB_CDC_RX_RING_SIZE 512U
#define USB_CDC_CMD_BUFFER_SIZE 128U

#if ((USB_CDC_RX_RING_SIZE & (USB_CDC_RX_RING_SIZE - 1U)) != 0U)
#error "USB_CDC_RX_RING_SIZE must be a power of two"
#endif

static uint8_t rx_ring[USB_CDC_RX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile bool usb_configured;
static volatile uint32_t rx_overflow_count;
static volatile uint32_t tx_busy_count;

static uint8_t command_buffer[USB_CDC_CMD_BUFFER_SIZE];
static uint16_t command_len;
static bool command_overflowed;
static void (*rx_callback)(uint8_t* data, uint16_t len);

static bool rx_ring_pop(uint8_t* data);
static void handle_received_byte(uint8_t data);
static void dispatch_command(void);

void InterfaceUsb_Init(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    usb_configured = false;
    rx_overflow_count = 0U;
    tx_busy_count = 0U;
    command_len = 0U;
    command_overflowed = false;
    rx_callback = NULL;
    memset(command_buffer, 0, sizeof(command_buffer));
}

void InterfaceUsb_SetRxCpltCallBack(void (*callback)(uint8_t* data, uint16_t len))
{
    rx_callback = callback;
}

void InterfaceUsb_OnReceive(const uint8_t* data, uint32_t len)
{
    if (data == NULL || len == 0U)
    {
        return;
    }

    for (uint32_t i = 0U; i < len; i++)
    {
        uint16_t next_head = (uint16_t)((rx_head + 1U) & (USB_CDC_RX_RING_SIZE - 1U));

        if (next_head == rx_tail)
        {
            rx_overflow_count++;
            continue;
        }

        rx_ring[rx_head] = data[i];
        rx_head = next_head;
    }
}

void InterfaceUsb_TaskTick(void)
{
    uint8_t data = 0U;

    while (rx_ring_pop(&data))
    {
        handle_received_byte(data);
    }
}

bool InterfaceUsb_CdcSend(const uint8_t* data, uint16_t len, void* user)
{
    (void)user;

    if (!usb_configured || data == NULL || len == 0U)
    {
        return false;
    }

    if (CDC_Transmit_FS((uint8_t*)data, len) != USBD_OK)
    {
        tx_busy_count++;
        return false;
    }

    return true;
}

void InterfaceUsb_SetConfigured(bool configured)
{
    usb_configured = configured;
}

bool InterfaceUsb_IsConfigured(void)
{
    return usb_configured;
}

uint32_t InterfaceUsb_GetRxOverflowCount(void)
{
    return rx_overflow_count;
}

uint32_t InterfaceUsb_GetTxBusyCount(void)
{
    return tx_busy_count;
}

static bool rx_ring_pop(uint8_t* data)
{
    if (rx_tail == rx_head)
    {
        return false;
    }

    *data = rx_ring[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) & (USB_CDC_RX_RING_SIZE - 1U));
    return true;
}

static void handle_received_byte(uint8_t data)
{
    if (data == '\r' || data == '\n')
    {
        if (command_overflowed)
        {
            command_overflowed = false;
            command_len = 0U;
        }
        else
        {
            dispatch_command();
        }
        return;
    }

    if (command_len >= (USB_CDC_CMD_BUFFER_SIZE - 1U))
    {
        command_len = 0U;
        command_overflowed = true;
        rx_overflow_count++;
        return;
    }

    if (command_overflowed)
    {
        return;
    }

    command_buffer[command_len++] = data;
}

static void dispatch_command(void)
{
    if (command_len == 0U)
    {
        return;
    }

    command_buffer[command_len] = '\0';

    if (rx_callback != NULL)
    {
        rx_callback(command_buffer, command_len);
    }

    command_len = 0U;
}
