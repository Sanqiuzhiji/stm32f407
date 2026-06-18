# 触摸屏驱动说明

本文档根据参考工程：

```text
E:\Active_Folders\STM32F407探索者资料\2，标准例程-HAL库版本\实验28 触摸屏实验
```

整理触摸屏相关硬件配置，并说明当前工程新增的触摸屏驱动代码。

## 参考工程硬件连接

参考工程同时支持电阻触摸屏和电容触摸屏，两类触摸屏复用了部分 IO。

### 电容触摸屏

支持的芯片包括：

- GT9xxx 系列：GT911、GT9147、GT9271、GT1151/1158 等
- FT5206/FT5426/CST340

引脚：

| 信号 | STM32 引脚 | CubeMX 建议配置 |
| --- | --- | --- |
| CT_SCL | PB0 | GPIO_Output_Open_Drain，Pull-up |
| CT_SDA | PF11 | GPIO_Output_Open_Drain，Pull-up |
| CT_INT | PB1 | GPIO_Input，Pull-up 或 No pull |
| CT_RST | PC13 | GPIO_Output，Push-pull，Pull-up |

参考工程使用软件模拟 I2C，不使用 STM32 硬件 I2C 外设。

### 电阻触摸屏

支持 ADS7843/ADS7846/UH7843/UH7846/XPT2046/TSC2046 类控制器。

引脚：

| 信号 | STM32 引脚 | CubeMX 建议配置 |
| --- | --- | --- |
| T_PEN | PB1 | GPIO_Input，Pull-up |
| T_CS | PC13 | GPIO_Output，Push-pull，Pull-up |
| T_MISO | PB2 | GPIO_Input，Pull-up |
| T_MOSI | PF11 | GPIO_Output，Push-pull，Pull-up |
| T_CLK | PB0 | GPIO_Output，Push-pull，Pull-up |

参考工程使用 GPIO 软件模拟 SPI，不使用 STM32 硬件 SPI 外设。

## STM32CubeMX 配置步骤

在 CubeMX 里按以下方式配置即可。因为当前驱动代码会在 `TouchScreen_Init()` 内再次初始化这些 GPIO，CubeMX 配置的主要作用是让 `.ioc` 和生成代码明确占用关系，避免后续误分配。

### 1. 打开 GPIO 引脚

在 Pinout 视图中设置：

| 引脚 | Label | Mode |
| --- | --- | --- |
| PB0 | TOUCH_SCL / TOUCH_CLK | GPIO_Output |
| PF11 | TOUCH_SDA / TOUCH_MOSI | GPIO_Output |
| PB1 | TOUCH_INT / TOUCH_PEN | GPIO_Input |
| PC13 | TOUCH_RST / TOUCH_CS | GPIO_Output |
| PB2 | TOUCH_MISO | GPIO_Input |

如果只用电容屏，`PB2` 可以不配置。

### 2. GPIO 参数

电容屏推荐：

| 引脚 | Mode | Pull | Speed |
| --- | --- | --- | --- |
| PB0 | Output Open Drain | Pull-up | Very High |
| PF11 | Output Open Drain | Pull-up | Very High |
| PB1 | Input | Pull-up 或 No pull | Very High |
| PC13 | Output Push Pull | Pull-up | Very High |

电阻屏推荐：

| 引脚 | Mode | Pull | Speed |
| --- | --- | --- | --- |
| PB0 | Output Push Pull | Pull-up | Very High |
| PF11 | Output Push Pull | Pull-up | Very High |
| PB1 | Input | Pull-up | Very High |
| PC13 | Output Push Pull | Pull-up | Very High |
| PB2 | Input | Pull-up | Very High |

如果需要兼容电容屏和电阻屏，可以先按电容屏方式在 CubeMX 标注；驱动初始化时会按实际识别结果重新配置引脚。

### 3. 不需要打开 I2C/SPI 外设

参考工程和当前移植代码都使用 GPIO bit-bang：

- 电容触摸：软件 I2C
- 电阻触摸：软件 SPI

所以 CubeMX 中不用启用 I2C1/I2C2/SPI1/SPI2。

### 4. 不需要配置 EXTI

当前驱动采用轮询扫描：

```c
TouchScreen_Scan(&state);
```

`INT/PEN` 引脚只作为普通输入读取，不需要配置外部中断。

### 5. 注意引脚冲突

参考工程中触摸屏占用：

```text
PB0, PB1, PB2, PC13, PF11
```

当前工程已有按键和 LED 不占用这些引脚：

```text
KEY_UP    PA0
KEY_LEFT  PE2
KEY_DOWN  PE3
KEY_RIGHT PE4
LED0      PF9
LED1      PF10
```

因此按当前工程看，触摸屏引脚没有和已有按键/LED 冲突。

## 当前新增代码

新增文件：

```text
CHESTON_STMHAL/MMInterface/Touch/
├── touch_screen.h
├── touch_screen.c
└── README.md
```

当前驱动特点：

| 项目 | 说明 |
| --- | --- |
| 电容屏 | 支持 GT9xxx 和 FT5206/CST340 类控制器 |
| 电阻屏 | 支持 XPT2046/ADS7846 类控制器 |
| I2C/SPI | 全部使用 GPIO 软件模拟 |
| LCD 依赖 | 不依赖 LCD 驱动 |
| EEPROM 依赖 | 不依赖 24CXX |
| 扫描方式 | 轮询 |

当前工程按 2.8 寸 ATK-MD0280 模块使用，默认配置为 `TOUCH_SCREEN_MODE_RESISTIVE`，分辨率为 `240x320`。

## 使用方式

初始化：

```c
#include "Touch/touch_screen.h"

touch_screen_config_t touch_config;
TouchScreen_GetDefaultConfig(&touch_config);
touch_config.mode = TOUCH_SCREEN_MODE_RESISTIVE;
touch_config.width = 240;
touch_config.height = 320;

bool ok = TouchScreen_Init(&touch_config);
```

扫描：

```c
touch_screen_state_t touch_state;

if (TouchScreen_Scan(&touch_state) && touch_state.pressed)
{
    uint16_t x = touch_state.x[0];
    uint16_t y = touch_state.y[0];
}
```

查看识别到的芯片：

```c
touch_screen_chip_t chip = TouchScreen_GetChip();
const char* name = TouchScreen_GetChipName(chip);
```

## 坐标方向配置

如果发现触摸坐标方向不对，可以调整：

```c
touch_config.swap_xy = true;   // X/Y 互换
touch_config.invert_x = true;  // X 方向反转
touch_config.invert_y = true;  // Y 方向反转
```

电阻屏还可以调整原始 AD 到屏幕坐标的映射范围：

```c
touch_config.resistive_x_min = 200;
touch_config.resistive_x_max = 3900;
touch_config.resistive_y_min = 200;
touch_config.resistive_y_max = 3900;
```

这些值需要根据实际屏幕校准。当前工程已经加入运行时五点校准流程，但暂时没有接 24CXX 保存逻辑，所以重新上电后会再次校准。

## 和参考工程的差异

参考工程的 `touch.c` 依赖：

- `lcddev.width / lcddev.height`
- `lcd_clear()`、`lcd_draw_point()` 等 LCD 绘图函数
- `24CXX` EEPROM 保存电阻屏校准参数
- `delay_us()` 系统延时模块

当前工程暂时没有完整 LCD/EEPROM 层，所以本次移植没有直接照搬这些依赖，而是把触摸通信和坐标读取独立出来。

后续如果要做电阻屏精确校准，可以再增加：

1. 五点校准流程
2. 校准参数结构体
3. Flash 或 EEPROM 保存
4. LCD 上显示校准十字
