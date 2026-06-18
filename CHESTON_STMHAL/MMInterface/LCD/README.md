# 2.8 Inch TFTLCD Hardware Notes

The ATK-MD0280 2.8 inch module on the STM32F407 Explorer board uses a 16-bit 8080-style LCD bus through STM32 FSMC. The touch controller on this module is XPT2046 resistive touch.

## CubeMX FSMC Pins

| LCD signal | STM32 pin | CubeMX signal |
| --- | --- | --- |
| LCD_CS | PG12 | FSMC_NE4 |
| LCD_RS/DC | PF12 | FSMC_A6 |
| CubeMX address enable | PF0 | FSMC_A0 |
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

## FSMC Parameters

Use `Bank1 NOR/SRAM4`, because the board connects LCD_CS to `FSMC_NE4`.

| Parameter | Value |
| --- | --- |
| Memory type | SRAM |
| Data width | 16 bits |
| Address/Data mux | Disable |
| Write operation | Enable |
| Extended mode | Enable |
| Access mode | A |
| Read AddressSetupTime | 15 |
| Read DataSetupTime | 60 |
| Write AddressSetupTime | 9 |
| Write DataSetupTime | 9 |

This project now uses CubeMX-generated `MX_FSMC_Init()` to initialize the FSMC bus. The LCD driver only initializes `PB15` backlight and writes LCD controller commands/data through the mapped FSMC address.

## Address Mapping

The reference project uses:

```c
LCD_FSMC_NEX = 4
LCD_FSMC_AX = 6
```

So the LCD command/data address is based on `0x6C000000`, and `FSMC_A6` is used as the LCD RS/DC line.

CubeMX may also require enabling `PF0/FSMC_A0` before it allows an address line group to be configured. That is acceptable. The LCD module does not use PF0 as RS/DC; command/data selection still uses `PF12/FSMC_A6`, so the LCD base address in the driver remains based on `LCD_FSMC_AX = 6`.

## Touch Pins

| Touch signal | STM32 pin |
| --- | --- |
| T_CLK | PB0 |
| T_PEN | PB1 |
| T_MISO | PB2 |
| T_CS | PC13 |
| T_MOSI | PF11 |

The current touch default is `TOUCH_SCREEN_MODE_RESISTIVE`.
