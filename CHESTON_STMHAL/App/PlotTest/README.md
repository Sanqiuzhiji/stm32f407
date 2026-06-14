# PlotTest 绘图测试模块说明

本文档说明当前下位机工程中 JustFloat 绘图测试代码的结构、调用关系、协议格式和后续扩展方式。

## 这套代码解决什么问题

上位机绘图需要持续收到一帧一帧的 float 数据。每帧格式是：

```text
float ch0
float ch1
float ch2
...
float chN
uint32_t end = 0x7F800000
```

其中 `0x7F800000` 是 IEEE754 正无穷的二进制表示，在这里不用它表达数值，只把它当作 JustFloat 协议的帧尾标记。上位机收到这个 4 字节标记后，就知道前面的 float 数据属于同一帧。

当前测试模块默认：

- 使用 `USART1` 发送。
- 使用 `HAL_UART_Transmit_DMA()` 发送。
- 上电后自动开始发送。
- 默认 4 个通道。
- 默认每 5 ms 发送一帧。
- 如果 UART TX DMA 正忙，本帧直接跳过，不阻塞任务。

## 文件位置

```text
CHESTON_STMHAL/
├── App/
│   └── PlotTest/
│       ├── plot_test.h
│       ├── plot_test.c
│       └── README.md
├── Connectivity/
│   ├── Protocol/
│   │   ├── justfloat_protocol.h
│   │   └── justfloat_protocol.c
│   └── USART/
│       ├── interface_uart.h
│       └── interface_uart.c
└── main_link.c
```

## 每个文件负责什么

### `Connectivity/Protocol/justfloat_protocol.h`

这个文件只定义 JustFloat 协议相关的数据结构和常量。

关键内容：

```c
#define JUSTFLOAT_MAX_CHANNELS 16U
#define JUSTFLOAT_FRAME_END_WORD 0x7F800000UL
```

- `JUSTFLOAT_MAX_CHANNELS`：最多 16 路 float 数据。
- `JUSTFLOAT_FRAME_END_WORD`：每帧最后 4 字节的固定帧尾。

```c
typedef struct
{
    float data[JUSTFLOAT_MAX_CHANNELS];
    uint32_t pend;
} justfloat_frame_t;
```

这个结构体是发送缓冲区。它预留了 16 个 float 槽位，最后有一个 `pend` 帧尾槽。

注意：实际发送长度不一定是整个结构体长度。如果只发 4 路，就只发送：

```text
4 路 float + 1 个帧尾 = 5 * 4 = 20 字节
```

### `Connectivity/Protocol/justfloat_protocol.c`

这个文件只负责把 float 数组打包成 JustFloat 帧。

核心函数：

```c
uint16_t JustFloat_BuildFrame(justfloat_frame_t* frame,
                              const float* data,
                              uint8_t channel_count);
```

它做 4 件事：

1. 检查参数是否合法。
2. 限制通道数不超过 16。
3. 把 `data[]` 复制到 `frame->data[]`。
4. 把 `0x7F800000` 写到最后一个有效通道后面。

为什么不用直接写 `frame->pend`？

因为 `frame->pend` 固定在第 16 个 float 后面。如果当前只发 4 路，那么帧尾应该放在 `data[4]` 的位置，而不是结构体末尾。

也就是说：

```text
发送 4 路:
data[0] data[1] data[2] data[3] END

发送 8 路:
data[0] ... data[7] END

发送 16 路:
data[0] ... data[15] END
```

所以代码用字节指针计算真实帧尾位置：

```c
uint8_t* end = (uint8_t*)frame + ((uint32_t)channel_count * sizeof(float));
memcpy(end, &pend, sizeof(pend));
```

### `App/PlotTest/plot_test.h`

这是绘图测试应用模块的对外接口。

外部只需要关心这几个函数：

```c
void PlotTest_Init(const plot_test_config_t* config);
void PlotTest_TaskTick(uint32_t now_ms);
bool PlotTest_HandleCommand(const uint8_t* data, uint16_t len);
bool PlotTest_UartDmaSend(const uint8_t* data, uint16_t len, void* user);
```

`PlotTest_Init()` 用于初始化模块。

`PlotTest_TaskTick()` 放在 FreeRTOS 任务循环里反复调用，它内部会判断是否到达下一帧发送时间。

`PlotTest_HandleCommand()` 用于处理串口收到的 `plot ...` 命令。

`PlotTest_UartDmaSend()` 是当前给 USART1 使用的发送函数。

配置结构体：

```c
typedef struct
{
    plot_test_send_func_t send;
    void* user;
    uint8_t channel_count;
    uint16_t interval_ms;
    bool auto_start;
} plot_test_config_t;
```

字段说明：

| 字段 | 作用 |
| --- | --- |
| `send` | 实际发送函数。现在用 UART DMA，以后可以换 CAN、USB。 |
| `user` | 发送函数需要的外部对象。现在传的是 `&huart1`。 |
| `channel_count` | 一帧发送几路 float。 |
| `interval_ms` | 两帧之间的间隔，单位 ms。 |
| `auto_start` | 是否初始化后自动开始发送。 |

### `App/PlotTest/plot_test.c`

这是测试模块主体，负责生成测试数据、按周期发送、解析命令。

内部状态：

```c
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
```

字段说明：

| 字段 | 作用 |
| --- | --- |
| `send` | 当前使用的底层发送函数。 |
| `user` | 发送函数的上下文参数。UART 模式下是 `UART_HandleTypeDef*`。 |
| `channel_count` | 当前通道数。 |
| `interval_ms` | 当前发送周期。 |
| `enabled` | 是否正在发送绘图数据。 |
| `next_send_ms` | 下一次允许发送的系统 tick。 |
| `sample_index` | 当前样本序号，用来生成变化波形。 |
| `frame` | JustFloat 发送帧缓存。 |
| `values` | 当前帧每个通道的 float 数值。 |

这个模块只有一个全局实例：

```c
static plot_test_t g_plot_test;
```

所以当前工程只有一组绘图测试流。如果以后要同时跑多个发送端，可以把这个全局对象改成外部传入的对象实例。

## 主调用链

### 初始化链路

入口在 `CHESTON_STMHAL/main_link.c`：

```c
void Main_Setup()
{
    plot_test_config_t plot_config = {
        .send = PlotTest_UartDmaSend,
        .user = &huart1,
        .channel_count = PLOT_TEST_DEFAULT_CHANNELS,
        .interval_ms = PLOT_TEST_DEFAULT_INTERVAL_MS,
        .auto_start = true,
    };

    UART_Interrupt_Init();
    PlotTest_Init(&plot_config);
    ...
}
```

含义：

1. 选择 `PlotTest_UartDmaSend` 作为发送函数。
2. 把 `USART1` 的 HAL 句柄 `&huart1` 传给发送函数。
3. 默认 4 路。
4. 默认 5 ms 一帧。
5. 上电后自动开始发。

### 运行链路

入口还是在 `CHESTON_STMHAL/main_link.c`：

```c
void Start_Task_Main(void* argument)
{
    for (;;)
    {
        PlotTest_TaskTick(HAL_GetTick());
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
```

每 1 ms 调用一次 `PlotTest_TaskTick()`。

`PlotTest_TaskTick()` 内部流程：

```text
是否 enabled?
    否 -> 返回
是否配置了 send 函数?
    否 -> 返回
当前时间是否到达 next_send_ms?
    否 -> 返回
生成当前帧 values[]
调用 JustFloat_BuildFrame() 打包
调用 send() 发送
发送成功:
    sample_index++
    next_send_ms = now_ms + interval_ms
发送失败:
    不更新 sample_index
    下次 tick 继续尝试
```

### 发送链路

当前使用的是：

```c
bool PlotTest_UartDmaSend(const uint8_t* data, uint16_t len, void* user)
```

`user` 在初始化时传入的是 `&huart1`，所以函数内部会转换成：

```c
UART_HandleTypeDef* huart = (UART_HandleTypeDef*)user;
```

发送前会检查：

```c
huart->gState == HAL_UART_STATE_READY
huart->hdmatx->State == HAL_DMA_STATE_READY
```

如果 UART 或 TX DMA 正忙，就返回 `false`。这样做的原因是绘图数据是连续测试流，宁愿跳过或推迟一帧，也不要阻塞 FreeRTOS 任务。

真正发送：

```c
HAL_UART_Transmit_DMA(huart, (uint8_t*)data, len)
```

## 测试波形怎么生成

生成函数在 `plot_test.c`：

```c
static void generate_values(plot_test_t* test)
```

当前每个通道都会生成不同形状或不同偏移的波形：

| 通道 | 波形 |
| --- | --- |
| ch0 | 三角波 |
| ch1 | 方波 |
| ch2 | 锯齿波 |
| ch3 及以后 | 三角波，不同周期和偏移 |

每个通道还会加一个偏移：

```c
test->values[i] = wave + ((float)i * 1.5f);
```

这样上位机里多路曲线不会全部挤在同一个纵向位置。

## 串口命令

命令入口在：

```text
CHESTON_STMHAL/Connectivity/USART/interface_uart.c
```

串口收到一段数据后会调用：

```c
OnUartCmd(uint8_t* _data, uint16_t _len)
```

函数开头先交给 PlotTest：

```c
if (PlotTest_HandleCommand(_data, _len))
{
    return;
}
```

如果是 `plot ...` 命令，就直接返回，不再回显文本。这一点很重要：JustFloat 是二进制流，如果在数据流中间混入 `printf` 文本，上位机解析可能会乱。

当前支持命令：

| 命令 | 作用 |
| --- | --- |
| `plot start` | 开始发送绘图数据。 |
| `plot stop` | 停止发送绘图数据。 |
| `plot rate 1` | 设置发送间隔为 1 ms。 |
| `plot rate 5` | 设置发送间隔为 5 ms。 |
| `plot ch 4` | 设置 4 个通道。 |
| `plot ch 16` | 设置 16 个通道。 |
| `plot status` | 当前只识别命令，不输出状态。 |

`plot status` 现在不输出文本，是为了避免污染绘图流。后续如果需要状态反馈，建议先 `plot stop`，再输出状态文本。

原来的 `led N` 命令仍然保留。如果不是 `plot ...` 命令，就会继续走原来的 LED 命令解析。

## 为什么要分 App、Connectivity、Protocol

当前拆分原则是：

```text
App/PlotTest
    负责“我要生成什么测试数据、什么时候发”

Connectivity/Protocol
    负责“数据按什么协议打包”

Connectivity/USART
    负责“串口怎么收命令、怎么发字节”
```

这样以后增加 CAN、USB 时，尽量不用重写 PlotTest 的业务逻辑。

比如现在的发送函数类型是：

```c
typedef bool (*plot_test_send_func_t)(const uint8_t* data, uint16_t len, void* user);
```

只要新的通信方式也提供一个类似函数，就可以接入。

## 以后接 CAN 怎么改

可以新增：

```text
CHESTON_STMHAL/Connectivity/CAN/
├── interface_can.h
└── interface_can.c
```

然后实现一个发送函数：

```c
bool PlotTest_CanSend(const uint8_t* data, uint16_t len, void* user)
{
    ...
}
```

但是要注意：CAN 一帧装不下完整 JustFloat 数据。经典 CAN 一帧最多 8 字节，CAN FD 才能更多。所以 CAN 需要额外设计分片格式，例如：

```text
CAN frame 0: frame_id, offset, payload[0..]
CAN frame 1: frame_id, offset, payload[...]
...
```

也就是说，CAN 不建议直接复用当前串口的连续字节流发送方式，最好在 CAN 层做分包。

## 以后接 USB CDC 怎么改

USB CDC 更接近串口流，可以比较自然地复用 JustFloat。

可以新增：

```text
CHESTON_STMHAL/Connectivity/USB/
├── interface_usb.h
└── interface_usb.c
```

然后实现：

```c
bool PlotTest_UsbCdcSend(const uint8_t* data, uint16_t len, void* user)
{
    ...
}
```

最后在 `Main_Setup()` 里把：

```c
.send = PlotTest_UartDmaSend,
.user = &huart1,
```

替换为：

```c
.send = PlotTest_UsbCdcSend,
.user = ...,
```

PlotTest 本身不用知道底层是 UART 还是 USB。

## 常见修改点

### 修改默认通道数

文件：

```text
CHESTON_STMHAL/App/PlotTest/plot_test.h
```

修改：

```c
#define PLOT_TEST_DEFAULT_CHANNELS 4U
```

也可以不改默认值，运行时用命令：

```text
plot ch 8
```

### 修改默认发送周期

文件：

```text
CHESTON_STMHAL/App/PlotTest/plot_test.h
```

修改：

```c
#define PLOT_TEST_DEFAULT_INTERVAL_MS 5U
```

也可以运行时用命令：

```text
plot rate 1
```

### 修改默认是否上电自动发送

文件：

```text
CHESTON_STMHAL/main_link.c
```

修改：

```c
.auto_start = true,
```

如果不想上电自动发送，改为：

```c
.auto_start = false,
```

然后通过串口命令启动：

```text
plot start
```

### 修改波形内容

文件：

```text
CHESTON_STMHAL/App/PlotTest/plot_test.c
```

修改函数：

```c
static void generate_values(plot_test_t* test)
```

例如要把 ch0 改成固定递增值，可以改成类似：

```c
test->values[0] = (float)test->sample_index;
```

### 修改 UART 波特率

文件：

```text
Core/Src/usart.c
```

当前：

```c
huart1.Init.BaudRate = 115200;
```

如果要更高频绘图，建议上位机和下位机一起改成更高波特率，例如：

```c
huart1.Init.BaudRate = 921600;
```

## 数据量估算

每帧字节数：

```text
(通道数 + 1 个帧尾) * 4 字节
```

例子：

| 通道数 | 每帧字节数 | 5 ms 周期约等于 |
| --- | --- | --- |
| 4 | 20 字节 | 4 KB/s |
| 8 | 36 字节 | 7.2 KB/s |
| 16 | 68 字节 | 13.6 KB/s |

115200 波特率理论最大约 11 KB/s 左右，实际还会更低。所以如果使用 16 通道、5 ms 周期，115200 很可能不够，需要提高波特率或降低发送频率。

## 当前设计里需要注意的点

1. `printf_DMA()` 和 JustFloat 发送共用 USART1 TX DMA，所以绘图高速发送期间不要频繁 `printf`。
2. `plot` 命令故意不回显文本，避免污染二进制绘图流。
3. 如果 TX DMA 忙，当前帧会发送失败，`sample_index` 不增加，下次 tick 会继续尝试。
4. JustFloat 帧尾是按有效通道数动态写入的，不是固定使用结构体最后的 `pend`。
5. 当前 `PlotTest` 只有一个全局实例，适合单路测试流。

## 快速对照阅读顺序

建议按这个顺序看代码：

1. `CHESTON_STMHAL/main_link.c`
   看 `Main_Setup()` 和 `Start_Task_Main()`，先理解模块什么时候初始化、什么时候被调度。

2. `CHESTON_STMHAL/App/PlotTest/plot_test.h`
   看对外接口和配置结构体。

3. `CHESTON_STMHAL/App/PlotTest/plot_test.c`
   看 `PlotTest_TaskTick()`，这是绘图测试的主流程。

4. `CHESTON_STMHAL/Connectivity/Protocol/justfloat_protocol.c`
   看 `JustFloat_BuildFrame()`，理解帧尾如何放到有效通道后面。

5. `CHESTON_STMHAL/Connectivity/USART/interface_uart.c`
   看 `OnUartCmd()`，理解串口命令如何控制绘图测试。

