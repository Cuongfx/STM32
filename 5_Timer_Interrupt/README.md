# 5 — Timer + Interrupt: HC-SR04 Ultrasonic Distance → LED Brightness

**Board:** Nucleo-F446RE (STM32F446RETx)
**IDE:** STM32CubeIDE (GCC / make)

## Learning goal

This project teaches **how to use timers and interrupts together to measure time precisely**, using an HC-SR04 ultrasonic sensor, without `HAL_Delay()` or busy-polling.

After this project you can:
- measure a pulse width with a free-running timer and an EXTI interrupt
- explain why three timers are used and what each one does
- calculate timer settings from Prescaler and Counter Period
- keep a sensor driver in its own module, separate from the application

## Overview

An HC-SR04 measures the distance to an object and the LED brightness follows it: bring your hand closer and the LED gets brighter, pull it away and it dims. The whole pipeline is built from timer and interrupt primitives:

| Timer / interrupt | Role |
|---|---|
| **TIM3** | Free-running 1µs counter (no interrupt): shared timebase for `delay_us()` and for timestamping the Echo pulse |
| **TIM6** | Update interrupt every 60ms: schedules each ranging cycle |
| **EXTI1 (PA1)** | External interrupt, both edges: catches the Echo pin's rising and falling edges |
| **TIM4 CH1** | PWM, 1kHz: turns the distance into LED brightness |

## Theory

### How the HC-SR04 works

The sensor has four pins: `VCC` (5V), `GND`, `Trig` (input) and `Echo` (output). Range is 2 to 400cm. It does nothing on its own: it waits for the MCU to drive `Trig`.

Hold `Trig` HIGH for at least 10µs and the sensor sends an 8-cycle 40kHz ultrasonic burst. Right after, `Echo` goes HIGH and stays HIGH until the reflection returns, then goes LOW. The time `T` that `Echo` stays HIGH is the round-trip time of the sound, so it is proportional to the distance:

```
S = v × T / 2 = 340 m/s × (T in µs) / 1,000,000 / 2 = 0.017 × T   (cm)
```

`v` is the speed of sound and the `/2` accounts for the trip to the object **and back**. The datasheet bounds `T`: about 118µs at 2cm, 23.5ms at 400cm, and `Echo` drops by itself after about 38ms if nothing is in range.

![HC-SR04 timing: trigger pulse, 8-cycle burst, Echo pulse width T, and how TIM3 timestamps both edges](hcsr04_timing_diagram.svg)

### Why three timers

![TIM3 / TIM6 / TIM4 roles and how TIM3 is shared between two steps in the same cycle](timers_role_diagram.svg)

- **TIM6 decides WHEN to measure.** It drives no pin; its Update interrupt every 60ms calls `Sensor_Trigger()`. The HC-SR04 needs at least 60ms between triggers so the previous echo has died out.
- **TIM3 measures HOW LONG.** A free-running counter, started once and never reset. `delay_us()` uses it for the 10µs `Trig` pulse, and the EXTI callback uses it to timestamp both Echo edges. Both just read `CNT` at different moments and subtract.
- **TIM4 outputs the RESULT.** It knows nothing about the sensor: it receives a `duty` value each cycle and free-runs the PWM in hardware.

### Timer settings

TIM3, TIM4 and TIM6 are on APB1, where the timer clock is **90MHz**:

```
Counter clock = Timer input clock / (PSC + 1)
Tick period   = 1 / Counter clock
Update period = Tick period × (ARR + 1)
```

| Timer | PSC | ARR | Result |
|---|---|---|---|
| TIM3 | 89 | 65535 | 1MHz → 1µs per tick; `CNT` wraps after 65.5ms, longer than the 38ms no-echo timeout |
| TIM6 | 8999 | 599 | 10kHz → 0.1ms per tick × 600 = **60ms** Update period |
| TIM4 | 89 | 999 | 1MHz / 1000 steps = **1kHz** PWM, 1000 brightness steps |

## Hardware & wiring

| Signal | Nucleo pin | Notes |
|---|---|---|
| HC-SR04 `VCC` | 5V | |
| HC-SR04 `GND` | GND | |
| HC-SR04 `Trig` | PB0 (**A3** on the Arduino header) | Direct: the sensor reads a 3.3V HIGH fine |
| HC-SR04 `Echo` | PA1 (**A1**), through a **1kΩ/2kΩ divider** | Do **not** connect straight to PA1 |
| LEDY (PWM) | PB6 (**D10**), `TIM4_CH1` | Through a ~330Ω resistor to GND |

![Wiring diagram](wiring_diagram.svg)

- **Check the silkscreen against the pin.** On the Nucleo-F446RE, `A0` = PA0 and `A3` = PB0. `Trig` goes to **A3**, not A0.
- **Echo needs a voltage divider.** The sensor's logic is 5V, and a 5V signal can exceed the STM32's 3.3V GPIO tolerance. Wire `Echo → 1kΩ → node → 2kΩ → GND` and tap `PA1` from the node: `5V × 2kΩ / (1kΩ + 2kΩ) ≈ 3.3V`.

## CubeMX setup

1. `File → New → STM32 Project` → Board Selector → **NUCLEO-F446RE** → Next → finish the wizard.
2. **Pinout view:**
   - **PB0** → `GPIO_Output`, label `TRIG`.
   - **PA1** → `GPIO_EXTI1`, label `ECHO`. **GPIO Mode** = `External Interrupt Mode with Rising/Falling edge trigger detection` (both edges: start and end of the pulse), **Pull-up/Pull-down** = `No pull`.
   - **PB6** → `TIM4_CH1`.
3. **Timers → TIM3**: tick **Activated** under Clock Source (base timer only, all channels `Disable`). **Prescaler** = `89`, **Counter Mode** = `Up`, **Counter Period** = `65535`, **auto-reload preload** = `Disable`.
4. **Timers → TIM4**: **Channel1** = `PWM Generation CH1`. **Prescaler** = `89`, **Counter Period** = `999`, **Pulse** = `0`.
5. **Timers → TIM6**: tick **Activated**. **Prescaler** = `8999`, **Counter Period** = `599`.
6. **System Core → NVIC**: confirm **TIM6 global interrupt** and **EXTI line1 interrupt** are enabled. Without them neither callback runs.
7. **Project Manager → Project** → Toolchain / IDE = `STM32CubeIDE`.
8. **Project Manager → Code Generator** → **`Copy only the necessary library files`**.
9. **GENERATE CODE**.

## Code

The driver in `lib/` is hand-written and handles **sensor signal processing only**: Trig, Echo and a distance in cm. It knows nothing about LEDs, PWM or TIM6; the brightness mapping stays in `main.c`. Reference solution: [lib/sensor.h](lib/sensor.h), [lib/sensor.c](lib/sensor.c), [Core/Src/main.c](Core/Src/main.c).

To make CubeIDE build `lib/`: add `../lib` under **Properties → C/C++ Build → Settings → MCU GCC Compiler → Include paths**, and add the `lib` folder under **C/C++ General → Paths and Symbols → Source Location** (once per configuration, Debug and Release).

### Architecture

- `sensor.c` owns TIM3 (the µs timebase), the Trig and Echo pins and the latest distance. Its handles live in `static` variables set by `Sensor_Init`.
- `main.c` connects the pieces: the TIM6 callback calls `Sensor_Trigger()`, the EXTI callback calls `Sensor_EXTI_Callback()`, and the loop turns the distance into a TIM4 duty value.

| Function | Parameters | Returns | Purpose |
|---|---|---|---|
| `Sensor_Init` | µs timer handle, Trig port + pin, Echo port + pin | – | Keep the handles, start TIM3 free-running |
| `Sensor_Trigger` | – | – | Call every 60ms: send the 10µs Trig pulse (ignored while the last echo is still pending) |
| `Sensor_EXTI_Callback` | `uint16_t GPIO_Pin` | – | Forward every EXTI interrupt: timestamp both Echo edges |
| `Sensor_GetDistanceCm` | – | `uint32_t` | Latest distance, safe to call at any time |

### Idea per function

- **`delay_us`** (private): busy-wait on TIM3's counter without ever resetting it. The 16-bit subtraction stays correct when `CNT` wraps around.
  ```c
  while ((uint16_t)((uint16_t)__HAL_TIM_GET_COUNTER(s_htim_us) - start) < us) { }
  ```
- **`Sensor_Trigger`**: 1) return if a measurement is still busy, else mark it busy; 2) Trig HIGH; 3) `delay_us(10)`; 4) Trig LOW.
- **`Sensor_EXTI_Callback`**: 1) ignore other pins; 2) read the Echo level to learn which edge fired: HIGH stores `CNT` as the start, LOW stores it as the end; 3) on the falling edge, convert the pulse width to a distance, clamp it to 2–400cm, store it and clear the busy flag.
  ```c
  uint16_t pulse_width_us = (uint16_t)(s_echo_end - s_echo_start);
  uint32_t distance = (uint32_t)(0.017f * (float)pulse_width_us);   // S = 0.017 × T
  ```
  *Watch out:* one both-edge EXTI line serves both edges, so the pin level inside the callback is the only way to tell them apart.

### In `main.c`

| Marker | Code |
|---|---|
| `Includes` | `#include "sensor.h"` and the two constants `LED_ACTIVE_RANGE_CM` (50) and `PWM_MAX_DUTY` (999) |
| `2`, after the `MX_..._Init()` calls | `Sensor_Init(&htim3, TRIG_GPIO_Port, TRIG_Pin, ECHO_GPIO_Port, ECHO_Pin)`, `HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1)`, `HAL_TIM_Base_Start_IT(&htim6)` |
| `3` (in `while(1)`) | read the distance, map it, write the compare register |
| `4` | `HAL_GPIO_EXTI_Callback` calls `Sensor_EXTI_Callback(GPIO_Pin)`; `HAL_TIM_PeriodElapsedCallback` calls `Sensor_Trigger()` when `htim->Instance == TIM6` |

The mapping is a linear inverse: 2cm gives `duty = 999` (brightest), 50cm gives `duty = 0` (off).

```c
uint32_t led_cm = (distance_cm > LED_ACTIVE_RANGE_CM) ? LED_ACTIVE_RANGE_CM : distance_cm;
uint32_t duty = PWM_MAX_DUTY - ((led_cm - 2) * PWM_MAX_DUTY / (LED_ACTIVE_RANGE_CM - 2));
__HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, duty);
```

The clamp keeps `led_cm` at 50 or below, so a far object reads as "off" instead of the unsigned subtraction wrapping into a huge number.

## Folder structure

```
5_Timer_Interrupt/
├── Core/Src/main.c              - MX_TIM3/4/6_Init, LED brightness mapping, EXTI + TIM6 callbacks
├── lib/sensor.h, lib/sensor.c   - sensor-only driver: TIM3 ownership, Trig/Echo, distance
├── 5_Timer_Interrupt.ioc        - CubeMX project (TIM3 base, TIM4 CH1 PWM, TIM6 base+IT, PA1 EXTI, PB0 output)
├── wiring_diagram.svg
├── hcsr04_timing_diagram.svg    - Trigger/Echo timing and how TIM3 + EXTI measure the pulse width
└── timers_role_diagram.svg      - why three timers and how TIM3 is shared across two steps
```
