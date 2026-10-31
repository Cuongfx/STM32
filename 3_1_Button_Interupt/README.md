# 3.1 — Button + Interrupt (modular, EXTI dispatcher)

**Board:** Nucleo-F446RE (STM32F446RETx)
**IDE:** STM32CubeIDE (GCC / make)

## Learning goal

This project teaches **how to read a button with a hardware interrupt (EXTI)** and how to organise interrupt handling so that more than one source can be added later.

After this project you can:
- explain the difference between polling and an interrupt
- follow the path from an EXTI edge to your own callback
- register callbacks in a dispatcher instead of editing `main.c` for every new interrupt source

## Overview

The same three button behaviours as [2_Button](../2_Button) (single click, double click, long press), but the button edge now comes from an **EXTI interrupt** instead of polling `HAL_GPIO_ReadPin()`. The code is split into a `lib/` with a reusable dispatcher. For the simpler single-file version, see [3_2_Button_Interrupt](../3_2_Button_Interrupt).

## Theory

### Interrupt vs. polling

Polling ([2_Button](../2_Button)) means the CPU asks "did the pin change?" thousands of times per second and almost always hears "no". With an interrupt the GPIO hardware notifies the CPU only when a real edge happens, so the CPU is free in between.

Mechanical bounce does **not** go away: a raw edge says something changed, not that it has settled. The 15ms debounce filter is still needed. The interrupt callback therefore does the minimum (`pin_changed = 1`) and the debounce and classification still run in the main loop.

### Architecture

![Full pipeline: interrupt, debounce, then the click/hold tree](button_pipeline_diagram.svg)

```
EXTI0_IRQHandler (hardware vector)
  → HAL_GPIO_EXTI_IRQHandler()
    → HAL_GPIO_EXTI_Callback()   (in main.c, overrides HAL's weak default)
      → EXTI_Dispatcher_Handle(GPIO_Pin)
        → looks up a 16-slot table (one per EXTI line) and calls the registered callback
          → Button_EXTI_Callback()   (lib/button.c) sets pin_changed = 1
```

**The dispatcher pattern:** `main.c` does not need to know every module's callback. Each module registers its own callback for its pin once at startup, and the dispatcher calls the right one when that pin fires:

```c
EXTI_Dispatcher_Register(Button_Pin, Button_EXTI_Callback);
```

A second interrupt source later means writing its own `lib/<module>.c` and adding one more `Register` line. `HAL_GPIO_EXTI_Callback()` in `main.c` never changes.

## Hardware & wiring

| Signal | Pin | Notes |
|---|---|---|
| `Button` | PC0 (Arduino header A5) | `GPIO_MODE_IT_RISING_FALLING`, internal pull-up: press and release both raise an interrupt |
| `LEDY` | PC1 (Arduino header A4) | Output, active-high through a series resistor |

![Wiring diagram](wiring_diagram.svg)

The circuit is identical to `2_Button`; only the software changes.

## CubeMX setup

1. `File → New → STM32 Project` → Board Selector → **NUCLEO-F446RE** → Next → finish the wizard.
2. Click pin **PC0** → `GPIO_EXTI0`. **GPIO Mode** = `External Interrupt Mode with Rising/Falling edge trigger detection` (both edges: a long press needs the *release* edge too), **Pull-up/Pull-down** = `Pull-up`, **User Label** = `Button`.
3. Click pin **PC1** → `GPIO_Output`. **User Label** = `LEDY`.
4. **System Core → NVIC** → confirm **EXTI line0 interrupt** is enabled. Without it the callback is never called.
5. **Project Manager → Project** → Toolchain / IDE = `STM32CubeIDE`.
6. **Project Manager → Code Generator** → **`Copy only the necessary library files`**.
7. **GENERATE CODE**.

## Folder structure

```
3_1_Button_Interupt/
├── Core/Src/main.c              - MX_GPIO_Init (EXTI0 + NVIC), HAL_GPIO_EXTI_Callback override
├── Core/Src/stm32f4xx_it.c      - EXTI0_IRQHandler
├── lib/exti_dispatcher.c/.h     - pin → callback lookup table
├── lib/button.c/.h              - debounce + single/double/hold state machine
├── lib/led.c/.h                 - LED reaction to button.c's flags
├── Interupt.ioc                 - CubeMX pin configuration
├── button_pipeline_diagram.svg
└── wiring_diagram.svg
```
