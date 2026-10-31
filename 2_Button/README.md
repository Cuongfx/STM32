# 2 — Button (polling, debounce, single/double click, long-press)

**Board:** Nucleo-F446RE (STM32F446RETx)
**IDE:** STM32CubeIDE (GCC / make)

## Learning goal

This project teaches **how to read a push button reliably by polling**, including mechanical debounce and click detection built as a non-blocking state machine.

After this project you can:
- explain why a raw button signal bounces and how a debounce filter fixes it
- tell a single click, a double click and a long press apart with timestamps
- separate "detect the event" from "react to the event" in two functions

## Overview

One button, three behaviours:

1. **Single click** (press and release once) → the LED toggles once.
2. **Double click** (two clicks within 400ms) → the LED blinks 3 times.
3. **Long press** (held longer than 500ms) → the LED blinks continuously until the button is released, then turns off.

The button is read by **polling**: the `while(1)` loop calls `HAL_GPIO_ReadPin()` on every iteration. No interrupts are used; [3_1_Button_Interupt](../3_1_Button_Interupt) and [3_2_Button_Interrupt](../3_2_Button_Interrupt) solve the same problem with EXTI interrupts so the two approaches can be compared.

## Theory

### Mechanical bounce

A real push button does not produce one clean transition. The metal contacts bounce, giving several rapid on/off changes within a few milliseconds, so reading the raw pin would count one press as several.

The fix is a **non-blocking debounce filter**: every time the raw reading changes, restart a 15ms timer. Only when the reading has stayed stable for the full 15ms is it trusted and copied into `button_current`. Nothing blocks; the check runs once per loop with the `HAL_GetTick()` timestamp pattern from [1_Blink_LED](../1_Blink_LED).

### Click classification

![Button decision tree with variables](button_decision_tree.svg)

On top of the debounced signal:
- On press, `time_press_start` is recorded.
- On release, if the press crossed the 500ms threshold it is a **hold**. Otherwise it counts toward `click_count`; a second release within 400ms (`DOUBLE_CLICK_WINDOW_MS`) is a **double click**, and if the window expires it resolves to a **single click**.

`Button_Handle()` only sets flags (`event_single_click`, `event_double_click`, `is_holding`); `LED_Handle()` only reacts to them. Because the two are separate, the LED could be replaced by a buzzer or a UART message without touching the button logic.

## Hardware & wiring

| Signal | Pin | Notes |
|---|---|---|
| `Button` | PC0 (Arduino header A5) | Input, internal pull-up: idle = HIGH, pressed = LOW |
| `LEDY` | PC1 (Arduino header A4) | Output, active-high through a series resistor |

![Wiring diagram](wiring_diagram.svg)

No external resistor for the button: `GPIO_PULLUP` is enabled in software, so one leg goes to `PC0` and the other to `GND`.

## CubeMX setup

1. `File → New → STM32 Project` → Board Selector → **NUCLEO-F446RE** → Next → finish the wizard.
2. Click pin **PC0** → `GPIO_Input`. **GPIO Pull-up/Pull-down** = `Pull-up`, **User Label** = `Button`.
3. Click pin **PC1** → `GPIO_Output`. **User Label** = `LEDY`.
4. **Project Manager → Project** → Toolchain / IDE = `STM32CubeIDE`.
5. **Project Manager → Code Generator** → **`Copy only the necessary library files`**.
6. **GENERATE CODE**.

## Code

The logic lives in two hand-written modules; `main.c` only calls them from the loop.

```c
/* USER CODE BEGIN 3 */
Button_Handle();   // debounce + classify, sets event flags
LED_Handle();      // react to the flags
/* USER CODE END 3 */
```

## Folder structure

```
2_Button/
├── Core/Src/main.c       - MX_GPIO_Init, while(1) loop calling Button_Handle() + LED_Handle()
├── lib/button.c/.h       - debounce + single/double/hold state machine
├── lib/led.c/.h          - LED reaction to the flags set by button.c
├── Button.ioc            - CubeMX pin configuration
├── button_decision_tree.svg
└── wiring_diagram.svg
```
