# TFT LCD 与 CubeMX 配置说明

本文档说明当前工程中 2.8 寸 TFT LCD 的硬件连接、STM32CubeMX 配置、FSMC 地址映射以及 `TftLcd_Init()` 的返回值规则。

## 1. 当前硬件连接

开发板上的 ATK-MD0280 2.8 寸屏使用 16 位 8080 并口，通过 STM32F407 的 FSMC 访问。当前工程使用 `Bank1 NOR/SRAM4`，因为 LCD 片选连接到 `FSMC_NE4`。

| LCD 信号 | STM32 引脚 | CubeMX 信号 |
| --- | --- | --- |
| LCD_CS | PG12 | FSMC_NE4 |
| LCD_RS/DC | PF12 | FSMC_A6 |
| CubeMX 地址线占位 | PF0 | FSMC_A0 |
| LCD_WR | PD5 | FSMC_NWE |
| LCD_RD | PD4 | FSMC_NOE |
| LCD_D0 | PD14 | FSMC_D0 |
| LCD_D1 | PD15 | FSMC_D1 |
| LCD_D2 | PD0 | FSMC_D2 |
| LCD_D3 | PD1 | FSMC_D3 |
| LCD_D4 | PE7 | FSMC_D4 |
| LCD_D5 | PE8 | FSMC_D5 |
| LCD_D6 | PE9 | FSMC_D6 |
| LCD_D7 | PE10 | FSMC_D7 |
| LCD_D8 | PE11 | FSMC_D8 |
| LCD_D9 | PE12 | FSMC_D9 |
| LCD_D10 | PE13 | FSMC_D10 |
| LCD_D11 | PE14 | FSMC_D11 |
| LCD_D12 | PE15 | FSMC_D12 |
| LCD_D13 | PD8 | FSMC_D13 |
| LCD_D14 | PD9 | FSMC_D14 |
| LCD_D15 | PD10 | FSMC_D15 |
| LCD_BL | PB15 | GPIO_Output |

触摸芯片为 XPT2046 电阻触摸，当前默认使用 `TOUCH_SCREEN_MODE_RESISTIVE`。

## 2. CubeMX 中的 FSMC 参数

当前 `.ioc` 和 `Core/Src/fsmc.c` 对应的核心配置如下：

| 参数 | 当前值 |
| --- | --- |
| Bank | Bank1 NOR/SRAM4 |
| Memory type | SRAM |
| Data width | 16 bits |
| Address/Data mux | Disable |
| Write operation | Enable |
| Extended mode | Enable |
| Access mode | A |
| 读 AddressSetupTime | 15 |
| 读 DataSetupTime | 60 |
| 写 AddressSetupTime | 9 |
| 写 DataSetupTime | 9 |

`MX_FSMC_Init()` 先由 CubeMX 生成默认时序，再在 `USER CODE BEGIN FSMC_Init 2` 中覆盖 BTCR/BWTR：

```c
FSMC_Bank1->BTCR[7] = (15U << 0U) | (60U << 8U);
FSMC_Bank1E->BWTR[6] = (9U << 0U) | (9U << 8U);
```

如果重新生成 CubeMX 工程，需要确认这段用户代码仍然保留。

## 3. FSMC 地址映射

当前 LCD 驱动使用下面的配置：

```c
#define TFT_LCD_FSMC_NEX 4U
#define TFT_LCD_FSMC_RS_AX 6U
```

因此 Bank4 基地址为：

```text
0x60000000 + 0x04000000 * (4 - 1) = 0x6C000000
```

FSMC 使用 16 位总线时，外部地址线 `A6` 对应 CPU 地址 bit7，所以：

| 用途 | 地址 | 说明 |
| --- | --- | --- |
| LCD 命令寄存器 | `0x6C000000` | `FSMC_A6 = 0` |
| LCD 数据/GRAM | `0x6C000080` | `FSMC_A6 = 1` |

PF0/FSMC_A0 只是为了满足 CubeMX 地址线配置，不作为 LCD 的 RS/DC 选择线。LCD 的命令/数据选择仍然由 PF12/FSMC_A6 决定。

## 4. `TftLcd_Init()` 返回值规则

现在 `TftLcd_Init()` 不再始终返回 `true`。

| 检测结果 | 行为 | 返回值 |
| --- | --- | --- |
| `0x9341` | 执行 ILI9341 初始化 | `true` |
| `0x7789` | 执行 ST7789 初始化 | `true` |
| `0x8552` | 作为 ST7789 的 RDDID 兼容结果处理 | `true` |
| 其他 ID、`0x0000`、`0xFFFF` 等 | 关闭背光，不初始化屏幕 | `false` |

应用层 `Main_Setup()` 会保存该返回值。LCD 初始化失败时，会跳过触摸初始化、触摸校准和后续 LCD 绘点，避免在屏幕总线异常时继续访问 LCD。

## 5. 初始化顺序

当前 `Core/Src/main.c` 的初始化顺序满足 LCD 要求：

```c
MX_GPIO_Init();
MX_DMA_Init();
MX_USART1_UART_Init();
MX_TIM7_Init();
MX_RTC_Init();
MX_FSMC_Init();
Main_Setup();
```

`TftLcd_Init()` 在 `Main_Setup()` 中调用，因此 FSMC 已经初始化完成。不要把 `TftLcd_Init()` 提前到 `MX_FSMC_Init()` 之前。

## 6. 调试建议

串口日志会打印两组 ID 探测结果：

```text
LCD RDDID D3: ....
LCD RDDID 04: ....
LCD ID: 0x....
```

常见现象和检查点：

| 现象 | 优先检查 |
| --- | --- |
| ID 为 `0x0000` | FSMC 未初始化、NOE/NWE/NE4 未生效、LCD 未上电 |
| ID 为 `0xFFFF` | 数据线悬空、读时序过短、LCD_RD 连接异常 |
| ID 不稳定 | FSMC GPIO 复用、上拉、读 DataSetupTime |
| 能读 ID 但白屏 | 背光 PB15、控制器型号、MADCTL/像素格式、GRAM 地址 |

当前工程支持 ILI9341 和 ST7789 两类初始化序列。若后续换屏，需要先确认 LCD 控制器 ID，再新增对应初始化序列，而不是恢复未知 ID 默认成功的逻辑。
