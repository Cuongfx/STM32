# STM32F446RE — Teaching Projects

A collection of small, progressively-harder STM32 embedded projects built on the **Nucleo-F446RE** board, developed as teaching material to introduce students to bare-metal / HAL-based STM32 programming with STM32CubeIDE.

Each project is self-contained: its own STM32CubeIDE project (`.project`/`.cproject`/`.ioc`), its own wiring diagram, and its own `README.md` explaining what it teaches and why it's built the way it is. Start at project 1 and work forward — each one reuses a concept from the previous one.

## Hardware

- **Board:** ST Nucleo-F446RE (STM32F446RETx, Cortex-M4)
- **IDE:** STM32CubeIDE (GCC / make toolchain — not Keil/IAR)

## Projects

| # | Project | Teaches |
|---|---|---|
| 1 | [1_Blink_LED](1_Blink_LED) | Non-blocking timing: blink two LEDs at different rates using `HAL_GetTick()` instead of `HAL_Delay()` |
| 2 | [2_Button](2_Button) | Reading a button by polling: debounce, single click, double click and long press |
| 3.1 | [3_1_Button_Interupt](3_1_Button_Interupt) | Reading a button with an **EXTI interrupt**, organised as a reusable `lib/` module |
| 3.2 | [3_2_Button_Interrupt](3_2_Button_Interrupt) | The same interrupt-driven button, built step by step in a single file (easier to follow than 3.1) |
| 4.1 | [4_1_ADC_LED](4_1_ADC_LED) | Reading a potentiometer with the **ADC** and using it to set LED brightness with **PWM** |
| 4.2 | [4_2_Multi_ADC_LED](4_2_Multi_ADC_LED) | Reading several analog inputs (potentiometer and LDR) at once with the **ADC + DMA** |
| 5 | [5_Timer_Interrupt](5_Timer_Interrupt) | Using **timers and timer interrupts** to read an HC-SR04 distance sensor and drive an LED |
| 6.1 | [6_1_UART_HELLO_WORLD](6_1_UART_HELLO_WORLD) | **UART** basics: send and receive data between the board and a PC through a CH340G module |
| 6.2 | [6_2_UART_Control_LED](6_2_UART_Control_LED) | Controlling an LED from the PC over **UART**, with the UART code organised as a small library |
| 7.1 | [7_1_I2C](7_1_I2C) | **I2C** basics: read and set the time of a DS3231 real-time clock and show it on the PC |

## Folder structure (general)

```
STM32_F446RE/
├── README.md                    - this file
├── 1_Blink_LED/                 - project 1 (see table above)
├── 2_Button/                    - project 2
├── 3_1_Button_Interupt/         - project 3.1
├── 3_2_Button_Interrupt/        - project 3.2
├── 4_1_ADC_LED/                 - project 4.1
├── 4_2_Multi_ADC_LED/           - project 4.2
├── 5_Timer_Interrupt/           - project 5
├── 6_1_UART_HELLO_WORLD/        - project 6.1
├── 6_2_UART_Control_LED/        - project 6.2
└── 7_1_I2C/                     - project 7.1

# each numbered project folder follows the same internal layout:
<project>/
├── Core/Src, Core/Inc           - CubeMX-generated HAL init code + application code
├── Drivers/                     - STM32 HAL + CMSIS (generated, not hand-edited)
├── lib/                         - hand-written application modules, where present
├── <name>.ioc                   - CubeMX pin/clock configuration (open with STM32CubeIDE)
├── README.md                    - what this project teaches, pin mapping, wiring diagram
└── wiring_diagram.svg           - how to physically wire the breadboard for this project
```
