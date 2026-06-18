# USB CDC 代码说明

本文档说明当前工程里的 USB CDC 实现、数据收发路径，以及它和普通串口 USART 的优缺点对比。

## 当前结论

当前工程已经接入 USB CDC，也就是电脑端看到的 Virtual COM Port。它在上位机上表现得像串口，但下位机底层不使用 `USART1`，而是使用 STM32F407 的 `USB_OTG_FS` 外设和 ST USB Device CDC 类。

当前主业务里，`PlotTest` 的发送通道已经配置为 USB CDC：

```c
plot_test_config_t plot_config = {
    .send = InterfaceUsb_CdcSend,
    .user = NULL,
    .channel_count = PLOT_TEST_DEFAULT_CHANNELS,
    .interval_ms = PLOT_TEST_DEFAULT_INTERVAL_MS,
    .auto_start = true,
};
```

也就是说，绘图测试数据现在默认通过 USB CDC 发给电脑。USB CDC 收到的文本命令会复用原来的 `OnUartCmd()`，所以原有的 `plot start`、`plot stop`、`plot rate N`、`plot ch N` 等命令入口仍然可以复用。

## 文件位置

USB CDC 相关文件分成两层：

```text
USB_DEVICE/
├── App/
│   ├── usb_device.c
│   ├── usb_device.h
│   ├── usbd_cdc_if.c
│   ├── usbd_cdc_if.h
│   ├── usbd_desc.c
│   └── usbd_desc.h
└── Target/
    ├── usbd_conf.c
    └── usbd_conf.h

CHESTON_STMHAL/
└── Connectivity/
    └── USB/
        ├── interface_usb.c
        ├── interface_usb.h
        └── README.md
```

两层职责不同：

| 层级 | 主要职责 |
| --- | --- |
| `USB_DEVICE/*` | CubeMX/ST USB Device 生成代码，负责 USB 枚举、描述符、CDC 类注册、端点收发。 |
| `CHESTON_STMHAL/Connectivity/USB/*` | 项目自己的 USB CDC 封装，负责收包缓存、按命令分包、发送函数适配、状态计数。 |

## USB 设备初始化链路

USB CDC 不是在 `main.c` 里直接初始化，而是在 FreeRTOS 默认任务里初始化：

```text
Core/Src/freertos.c
StartDefaultTask()
    MX_USB_DEVICE_Init()
```

`MX_USB_DEVICE_Init()` 位于：

```text
USB_DEVICE/App/usb_device.c
```

初始化过程：

```text
USBD_Init()
    初始化 USB Device Core，绑定 FS_Desc 描述符

USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC)
    注册 CDC 类

USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS)
    注册 CDC 回调函数表

USBD_Start()
    启动 USB Device
```

CDC 回调函数表在 `USB_DEVICE/App/usbd_cdc_if.c`：

```c
USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS,
  CDC_TransmitCplt_FS
};
```

这些函数是 ST USB CDC 类调用项目代码的入口。

## USB 底层硬件配置

USB OTG FS 的底层配置在：

```text
USB_DEVICE/Target/usbd_conf.c
```

当前关键配置：

| 项目 | 当前配置 |
| --- | --- |
| USB 外设 | `USB_OTG_FS` |
| 模式 | Device Only |
| 速度 | Full Speed |
| DM 引脚 | `PA11` |
| DP 引脚 | `PA12` |
| 中断 | `OTG_FS_IRQn`，优先级 `5` |
| DMA | `hpcd_USB_OTG_FS.Init.dma_enable = DISABLE` |
| VBUS sensing | `DISABLE` |

USB 中断入口在：

```text
Core/Src/stm32f4xx_it.c
OTG_FS_IRQHandler()
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS)
```

也就是说，USB 收发事件最终由 `OTG_FS_IRQHandler()` 进入 HAL PCD，再进入 ST USB Device Core 和 CDC 类回调。

## USB 描述符

描述符在：

```text
USB_DEVICE/App/usbd_desc.c
```

当前关键字符串：

| 项目 | 当前值 |
| --- | --- |
| VID | `1155` |
| PID | `22336` |
| Manufacturer | `STMicroelectronics` |
| Product | `STM32 Virtual ComPort` |
| Configuration | `CDC Config` |
| Interface | `CDC Interface` |

电脑插入设备后，会根据这些描述符枚举出一个 CDC 虚拟串口设备。

## CDC 接收路径

电脑向虚拟串口写入数据后，下位机接收路径如下：

```text
USB OUT packet
    -> USB Device Core
    -> CDC_Receive_FS(Buf, Len)
    -> InterfaceUsb_OnReceive(Buf, *Len)
    -> USBD_CDC_ReceivePacket()
```

代码位置：

```text
USB_DEVICE/App/usbd_cdc_if.c
CHESTON_STMHAL/Connectivity/USB/interface_usb.c
```

`CDC_Receive_FS()` 做两件事：

1. 把收到的数据交给 `InterfaceUsb_OnReceive()`。
2. 调用 `USBD_CDC_ReceivePacket()` 重新打开下一次 OUT 接收。

`InterfaceUsb_OnReceive()` 不直接解析命令，而是把字节写入环形缓冲区：

```c
#define USB_CDC_RX_RING_SIZE 512U
static uint8_t rx_ring[USB_CDC_RX_RING_SIZE];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
```

这样做的目的：

| 设计 | 作用 |
| --- | --- |
| CDC 回调里只搬运数据 | 缩短 USB 回调执行时间，避免在 USB 中断上下文里做复杂业务。 |
| 使用环形缓冲区 | USB 一次可能收到多个字节，也可能分多包到达，环形缓冲用于解耦 USB 包边界和命令边界。 |
| 溢出计数 `rx_overflow_count` | 当业务处理太慢、环形缓冲写满时，记录丢字节次数，方便调试。 |

## CDC 命令解析路径

项目主任务会周期调用：

```text
CHESTON_STMHAL/main_link.c
Start_Task_Main()
    InterfaceUsb_TaskTick()
```

`InterfaceUsb_TaskTick()` 从环形缓冲区取出字节，再交给 `handle_received_byte()`。设计意图是按行解析命令：

```text
rx_ring
    -> InterfaceUsb_TaskTick()
    -> handle_received_byte()
    -> command_buffer
    -> dispatch_command()
    -> rx_callback(command_buffer, command_len)
```

初始化时注册的回调是：

```c
InterfaceUsb_SetRxCpltCallBack(OnUartCmd);
```

所以 USB CDC 收到完整命令后，最终仍然进入：

```text
CHESTON_STMHAL/Connectivity/USART/interface_uart.c
OnUartCmd()
```

这也是为什么 USB CDC 可以复用原来的串口命令处理逻辑。`OnUartCmd()` 里会先让 `PlotTest_HandleCommand()` 处理 `plot ...` 命令，如果不是绘图命令，再处理原来的 `led N` 命令。

## CDC 发送路径

当前 `PlotTest` 使用的发送函数是：

```c
bool InterfaceUsb_CdcSend(const uint8_t* data, uint16_t len, void* user)
```

发送路径：

```text
PlotTest_TaskTick()
    -> JustFloat_BuildFrame()
    -> InterfaceUsb_CdcSend(data, len, NULL)
    -> CDC_Transmit_FS(data, len)
    -> USBD_CDC_SetTxBuffer()
    -> USBD_CDC_TransmitPacket()
    -> USB IN endpoint
```

`InterfaceUsb_CdcSend()` 会先检查：

```c
if (!usb_configured || data == NULL || len == 0U)
{
    return false;
}
```

`usb_configured` 由 CDC 初始化和反初始化回调维护：

```text
CDC_Init_FS()
    InterfaceUsb_SetConfigured(true)

CDC_DeInit_FS()
    InterfaceUsb_SetConfigured(false)
```

`CDC_Transmit_FS()` 还会检查 CDC 发送状态：

```c
USBD_CDC_HandleTypeDef *hcdc =
    (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;

if (hcdc == NULL) {
    return USBD_BUSY;
}

if (hcdc->TxState != 0) {
    return USBD_BUSY;
}
```

如果上一包还没发完，`CDC_Transmit_FS()` 返回 `USBD_BUSY`，`InterfaceUsb_CdcSend()` 会增加：

```c
tx_busy_count++;
```

这和 UART DMA 发送的设计思路类似：发送资源忙时返回失败，不在任务里长时间阻塞。

## 当前缓冲区

CDC 原始收发缓冲区在：

```text
USB_DEVICE/App/usbd_cdc_if.h
```

当前大小：

```c
#define APP_RX_DATA_SIZE  2048
#define APP_TX_DATA_SIZE  2048
```

项目自定义接收环形缓冲区在：

```text
CHESTON_STMHAL/Connectivity/USB/interface_usb.c
```

当前大小：

```c
#define USB_CDC_RX_RING_SIZE 512U
#define USB_CDC_CMD_BUFFER_SIZE 128U
```

含义：

| 缓冲区 | 作用 |
| --- | --- |
| `UserRxBufferFS` | ST CDC 类直接使用的 USB OUT 接收缓冲。 |
| `UserTxBufferFS` | ST CDC 类初始化时设置的默认 TX 缓冲。当前实际发送会通过 `USBD_CDC_SetTxBuffer()` 指向调用方传入的数据。 |
| `rx_ring` | 项目层的接收环形缓冲。 |
| `command_buffer` | 项目层的单条命令缓存。 |

## 和 PlotTest 的关系

`PlotTest` 本身不关心底层是 UART 还是 USB CDC，它只依赖一个函数指针：

```c
typedef bool (*plot_test_send_func_t)(const uint8_t* data,
                                      uint16_t len,
                                      void* user);
```

以前可以用 UART DMA：

```c
.send = PlotTest_UartDmaSend,
.user = &huart1,
```

现在可以用 USB CDC：

```c
.send = InterfaceUsb_CdcSend,
.user = NULL,
```

这说明当前代码已经把“绘图数据怎么生成”和“字节流怎么发出去”分开了。以后如果要切 CAN、RS485、以太网，也可以继续使用类似的发送函数适配。

## 串口 USART 当前实现概览

USART1 代码主要在：

```text
Core/Src/usart.c
Core/Src/stm32f4xx_it.c
CHESTON_STMHAL/Connectivity/USART/interface_uart.c
CHESTON_STMHAL/Connectivity/USART/retarget.c
```

当前 USART1 配置：

| 项目 | 当前配置 |
| --- | --- |
| 外设 | `USART1` |
| TX | `PA9` |
| RX | `PA10` |
| 波特率 | `115200` |
| 数据格式 | 8 data bits, no parity, 1 stop bit |
| RX DMA | `DMA2_Stream2` |
| TX DMA | `DMA2_Stream7` |
| 中断 | `USART1_IRQn`，优先级 `5` |

串口接收使用 DMA + IDLE 中断：

```text
USART1_IRQHandler()
    检测 UART_FLAG_IDLE
    HAL_UART_DMAStop()
    计算本次接收长度
    OnRecvEnd(rx_buffer, rxLen)
    HAL_UART_Receive_DMA()
```

串口发送有两类：

| 函数 | 方式 |
| --- | --- |
| `_write()` | 阻塞式 `HAL_UART_Transmit()`，用于普通 `printf()` 重定向。 |
| `printf_DMA()` | `HAL_UART_Transmit_DMA()`，用于非阻塞格式化输出。 |
| `PlotTest_UartDmaSend()` | `HAL_UART_Transmit_DMA()`，用于绘图数据发送。 |

## USB CDC 和串口的共同点

| 共同点 | 说明 |
| --- | --- |
| 都能传字节流 | 上层可以把它们都看成连续字节流。 |
| 都能接文本命令 | 当前 USB CDC 和 USART 都可以复用 `OnUartCmd()`。 |
| 都适合调试通信 | 上位机可以用串口助手或自定义工具收发数据。 |
| 都需要处理忙状态 | UART DMA 会忙，USB CDC 的 `TxState` 也会忙。 |
| 都没有天然消息边界 | 上层协议仍然要自己定义命令结束符、帧尾或长度。 |

## USB CDC 的优点

| 优点 | 说明 |
| --- | --- |
| 速度高于常见 UART | USB Full Speed 理论 12 Mbit/s，实际 CDC 吞吐通常也明显高于 115200 串口。 |
| 电脑连接方便 | 一根 USB 线即可通信，不需要 USB 转 TTL 模块。 |
| 上位机体验接近串口 | Windows/Linux/macOS 上通常会枚举为虚拟 COM/TTY 设备。 |
| 不占用 USART 引脚 | 不使用 `PA9/PA10`，可以释放 USART1 给其他外设。 |
| 适合高速连续数据 | 对当前 JustFloat 绘图流，比 115200 UART 更适合。 |
| 可以复用串口类工具 | 很多串口助手、Python `pyserial` 程序可以直接打开 CDC 虚拟串口。 |

## USB CDC 的缺点

| 缺点 | 说明 |
| --- | --- |
| 依赖 USB 主机 | USB CDC 需要电脑或 USB Host 枚举，不能像 UART 那样两个 MCU 直接互连。 |
| 协议栈更复杂 | 需要 USB Device Core、CDC 类、描述符、端点、中断等配置。 |
| 调试早期启动不方便 | USB 枚举完成前不能通信，复位、断点、低功耗都可能导致主机端口断开重连。 |
| 实时性受主机调度影响 | USB CDC 吞吐高，但包调度由 USB 主机控制，延迟抖动通常比裸 UART 更复杂。 |
| 发送缓冲生命周期要注意 | `CDC_Transmit_FS()` 会把传入指针设置为 TX buffer，数据在发送完成前不应被覆盖。 |
| 驱动和系统行为有差异 | 不同操作系统对 CDC 端口名、打开关闭、DTR、缓存刷新行为可能不同。 |

## 普通串口 USART 的优点

| 优点 | 说明 |
| --- | --- |
| 简单直接 | TX/RX/GND 即可通信，协议栈远比 USB 简单。 |
| 适合 MCU 对 MCU | 两个 MCU 或 MCU 对传感器、模块通信很方便。 |
| 启动早 | MCU 初始化 USART 后即可通信，不需要等待 USB 枚举。 |
| 实时性更可控 | 波特率固定，收发时序更直观，适合简单实时控制或日志。 |
| 调试工具成熟 | 逻辑分析仪、示波器、USB 转 TTL 模块都容易接入。 |
| 可扩展 RS485/RS232 | 加收发器后可以做长线、总线或工业现场通信。 |

## 普通串口 USART 的缺点

| 缺点 | 说明 |
| --- | --- |
| 速度受波特率限制 | 当前 `115200` 理论有效吞吐约 11 KB/s，跑多通道高频绘图容易不够。 |
| 需要额外转接器 | 电脑通常需要 USB 转 TTL 模块。 |
| 占用固定引脚 | 当前占用 `PA9/PA10`，还占用 USART1 和 DMA 资源。 |
| 电平要匹配 | MCU TTL、RS232、RS485 不能直接混接，需要注意电平和收发器。 |
| 长线抗干扰较弱 | 裸 TTL UART 不适合长线复杂环境。 |
| printf 和数据流容易冲突 | 当前 `printf_DMA()` 和绘图数据可能共用 USART1 TX DMA，高速发送时容易互相抢资源。 |

## 选择建议

| 场景 | 建议 |
| --- | --- |
| 电脑上位机高速看波形 | 优先 USB CDC。 |
| 简单命令调试、少量日志 | UART 或 USB CDC 都可以。 |
| 设备需要和另一个 MCU 通信 | 优先 UART，必要时加 RS485/CAN。 |
| 设备必须脱离电脑运行 | UART 更通用，USB CDC 只能作为调试或上位机接口。 |
| 数据量较大，例如 JustFloat 多通道高频流 | 优先 USB CDC，或者提高 UART 波特率到 921600 以上。 |
| 需要最简单、最容易排错的链路 | UART 更直接。 |

## 当前代码需要注意的点

1. `USB CDC` 在电脑上显示为串口，但下位机端不受 `USART1` 波特率影响。上位机设置的 CDC baud rate 当前没有在 `CDC_Control_FS()` 中实际使用。
2. `CDC_Control_FS()` 目前只保留了 ST 模板里的 case，没有处理 `CDC_SET_LINE_CODING`、`CDC_GET_LINE_CODING`、`CDC_SET_CONTROL_LINE_STATE` 等请求参数。如果后续要根据 DTR 判断上位机是否打开端口，需要在这里补逻辑。
3. `InterfaceUsb_CdcSend()` 只允许一个 CDC IN 传输在路上。如果 `hcdc->TxState != 0`，会返回失败并增加 `tx_busy_count`。
4. `InterfaceUsb_OnReceive()` 的环形缓冲满了会丢弃新字节，并增加 `rx_overflow_count`。如果命令频繁或任务处理太慢，需要增大 `USB_CDC_RX_RING_SIZE` 或提高 `InterfaceUsb_TaskTick()` 调度频率。
5. `command_buffer` 只有 128 字节。长命令需要扩容或改成长度帧协议。
6. `handle_received_byte()` 使用 `\r` 或 `\n` 作为命令结束符。上位机发送 `plot start\n` 或 `plot start\r\n` 都可以触发解析；如果不发送换行，命令会先停留在 `command_buffer` 里。
7. 如果一条命令超过 `USB_CDC_CMD_BUFFER_SIZE - 1` 字节，当前实现会丢弃这整条命令，直到收到下一个 `\r` 或 `\n` 后恢复解析，同时增加 `rx_overflow_count`。

## 当前 CDC 命令分包逻辑

`handle_received_byte()` 当前行为如下：

```text
收到 '\r' 或 '\n':
    如果 command_len > 0:
        dispatch_command()
    返回

command_buffer 未满:
    command_buffer[command_len++] = data

command_buffer 已满:
    丢弃当前命令，直到收到下一个 '\r' 或 '\n'
```

推荐实现原则：

| 原则 | 原因 |
| --- | --- |
| 支持 `\r`、`\n`、`\r\n` | 兼容不同串口助手和终端。 |
| 不在 USB 回调里直接跑业务 | 避免 USB 中断/回调耗时过长。 |
| 溢出时清空当前命令 | 避免半条坏命令继续污染后续解析。 |
| 回调前补 `'\0'` | 方便复用 `sscanf()` 这类 C 字符串解析函数。 |

## 快速阅读顺序

建议按这个顺序看代码：

1. `CHESTON_STMHAL/main_link.c`
   看 `Main_Setup()` 和 `Start_Task_Main()`，理解 USB CDC 怎么接入 PlotTest 和主任务。

2. `USB_DEVICE/App/usb_device.c`
   看 `MX_USB_DEVICE_Init()`，理解 USB Device Core、CDC 类和接口回调如何注册。

3. `USB_DEVICE/App/usbd_cdc_if.c`
   看 `CDC_Init_FS()`、`CDC_Receive_FS()`、`CDC_Transmit_FS()`，理解 CDC 类回调。

4. `CHESTON_STMHAL/Connectivity/USB/interface_usb.c`
   看 `InterfaceUsb_OnReceive()`、`InterfaceUsb_TaskTick()`、`InterfaceUsb_CdcSend()`，理解项目层封装。

5. `CHESTON_STMHAL/Connectivity/USART/interface_uart.c`
   看 `OnUartCmd()`，理解 USB CDC 和 UART 为什么能复用同一套命令处理。

6. `Core/Src/usart.c` 和 `Core/Src/stm32f4xx_it.c`
   对照 USART 的 DMA + IDLE 中断收包方式，理解它和 USB CDC 的差异。
