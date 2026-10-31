# 3.2 — Button + Interrupt (guided, single-file build)

**Board:** Nucleo-F446RE (STM32F446RETx)
**IDE:** STM32CubeIDE (GCC / make)

## Learning goal

This project teaches **how to build an interrupt-driven button step by step**, testing each stage on hardware before adding the next.

After this project you can:
- configure an EXTI pin with both-edge detection in CubeMX
- verify the interrupt chain with a raw test before writing real logic
- build debounce and click detection on top of an interrupt flag

## Overview

The same single / double click and long-press behaviour as [3_1_Button_Interupt](../3_1_Button_Interupt), but everything lives in one file, `Core/Src/main.c`: no `lib/`, no dispatcher. Build this one from scratch first; 3.1 shows how to restructure it once more than one interrupt source is needed.

## Theory

### Interrupt vs. polling

An interrupt lets the GPIO hardware notify the CPU only when a real edge happens, instead of the CPU asking on every loop iteration (see [2_Button](../2_Button) for the polling version). Mechanical bounce still happens, so a 15ms debounce filter is required however the edge was detected.

### Logic

![Full pipeline: interrupt, debounce, then the click/hold tree](button_pipeline_diagram.svg)

`HAL_GPIO_EXTI_Callback()` does the minimum (`pin_changed = 1`). Debounce, click counting, hold detection and the LED reaction all run in `Button_Handle()` / `LED_Handle()` inside the main loop, using the timestamp pattern from [1_Blink_LED](../1_Blink_LED).

## Build order

Each stage must compile and work on hardware before the next one starts, so a bug always points at the last thing added.

1. **CubeMX pin config only**: `PC1` as `GPIO_Output`, `PC0` as `GPIO_EXTI0` with pull-up and both-edge trigger, NVIC EXTI0 enabled.
2. **Raw LED test**: blink the LED with `HAL_Delay()`. Confirms the LED wiring and pin.
3. **Raw interrupt test**: `HAL_GPIO_EXTI_Callback()` toggles the LED on every edge. Confirms the whole interrupt chain (vector, NVIC enable, callback override).
4. **Debounce**: the callback only sets `pin_changed = 1`; a non-blocking 15ms timer in `Button_Handle()` confirms the pin has settled.
5. **Click/hold state machine**: `click_count`, `time_first_click`, `is_holding`, `time_press_start` classify the debounced signal.
6. **Separate `LED_Handle()`**: detection and reaction in two functions, each with one reason to change.

## Hardware & wiring

| Signal | Pin | Notes |
|---|---|---|
| `Button` | PC0 (Arduino header A5) | `GPIO_MODE_IT_RISING_FALLING`, internal pull-up |
| `LEDY` | PC1 (Arduino header A4) | Output, active-high through a series resistor |

![Wiring diagram](wiring_diagram.svg)

## CubeMX setup

1. `File → New → STM32 Project` → Board Selector → **NUCLEO-F446RE** → Next → finish the wizard.
2. Click pin **PC0** → `GPIO_EXTI0`. **GPIO Mode** = `External Interrupt Mode with Rising/Falling edge trigger detection`, **Pull-up/Pull-down** = `Pull-up`, **User Label** = `Button`.
3. Click pin **PC1** → `GPIO_Output`. **User Label** = `LEDY`.
4. **System Core → NVIC** → confirm **EXTI line0 interrupt** is enabled.
5. **Project Manager → Project** → Toolchain / IDE = `STM32CubeIDE`.
6. **Project Manager → Code Generator** → **`Copy only the necessary library files`**.
7. **GENERATE CODE**.

## Folder structure

```
3_2_Button_Interrupt/
├── Core/Src/main.c          - everything: MX_GPIO_Init, HAL_GPIO_EXTI_Callback,
│                               Button_Handle(), LED_Handle(), while(1) loop
├── Core/Src/stm32f4xx_it.c  - EXTI0_IRQHandler
├── Interrupt.ioc            - CubeMX pin configuration
├── button_pipeline_diagram.svg
└── wiring_diagram.svg
```
