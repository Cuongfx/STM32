# 6.2 — UART: switching the on-board LED from the keyboard, with a small library

**Board:** Nucleo-F446RE (STM32F446RETx)
**IDE:** STM32CubeIDE (GCC / make)

## Learning goal

This project teaches **how to control hardware from a PC over UART**, and how to package the UART code as a small reusable library.

After this project you can:
- explain how a typed character reaches your code as a number (ASCII)
- hand a command from an interrupt to the main loop with a `volatile` flag and a one-byte mailbox
- split a module into a public header and a private source file
- handle unknown input with `switch` / `default` and reply to the sender

## Overview

Type a letter in the terminal and the LED obeys:

```
UART LED ready. Send 'a' = ON, 'b' = OFF
a   →  LED ON                       (LD2 lights up)
b   →  LED OFF                      (LD2 goes dark)
x   →  Please choose a to turn on LED or b to turn off LED
```

It builds on [6_1_UART_HELLO_WORLD](../6_1_UART_HELLO_WORLD) (same interrupt-driven receive), but all UART and LED code now lives in a two-file library, `uart_led.c/.h`, and `main.c` shrinks to a few one-line calls.

## Theory

### How UART works

UART is **asynchronous**: there is no clock wire. Each side runs its own timer and both must be set to the same baud rate in advance. Here that is `115200` baud, 8 data bits, no parity, 1 stop bit (`8N1`). If the two sides disagree by more than roughly 2–3%, the receiver samples at the wrong instants and you get garbage, or nothing at all. A terminal left at `9600` will not understand a board talking at `115200`.

### Two devices, three wires

![Two microcontrollers connected by UART: Tx to Rx, Rx to Tx, shared GND](diagrams/uart_connection.svg)

Every UART device has a **Tx** pin (where it *talks*) and an **Rx** pin (where it *listens*). Two devices are connected by crossing them over:

- **Tx → Rx.** What one side transmits, the other receives. Tx to Tx would put two talkers on one wire and leave nobody listening.
- **Rx ← Tx.** The return path is a *second* wire, so both sides can talk at the same time (**full duplex**).
- **GND — GND.** A logic level is a voltage *relative to something*; the shared ground is that something.

There is no clock wire and no "chip select" wire, which is why the link needs so few pins, and why UART connects exactly **two** devices. Here the Nucleo is one MCU (`PA9` = Tx, `PA10` = Rx) and the CH340G module is the other; the module talks to the PC over USB.

### The frame: start, data, stop

![UART frame format: start bit, eight data bits D0 to D7, optional parity bit, stop bit](diagrams/uart_frame_format.svg)

Each byte is wrapped in a **frame** so the receiver can tell where it begins and ends without a clock:

| Field | Level | Purpose |
|---|---|---|
| **Idle** | HIGH | The resting state of the line between frames |
| **Start bit** | LOW (logic 0) | The falling edge announces "a byte begins now" and starts the receiver's timer |
| **D0 … D7** | data | The eight bits of the byte, **least-significant bit first** |
| **Parity bit** (PB) | even / odd | *Optional.* One extra bit that makes the number of 1s even (or odd), so a single flipped bit can be detected. Not used here |
| **Stop bit** | HIGH (logic 1) | Returns the line to idle and guarantees the next start bit is a fresh falling edge |

**8N1** is 1 + 8 + 1 = **10 bits on the wire per byte**, so at most 80% of the line speed carries data.

The **baud rate** is bits per second and fixes how long each cell lasts. At 115200 baud one bit lasts 1 / 115200 ≈ 8.7µs, so a frame takes about 87µs and the link moves roughly 11,500 bytes per second. With no clock wire, the receiver times every bit with its own timer, started by the falling edge of the start bit and aimed at the *middle* of each bit (the STM32 takes three samples around the middle and votes). That is why both sides must use the same baud rate.

### Worked example: sending 0x93

![The byte 0x93 sent as a UART waveform from TX to RX](diagrams/uart_byte_0x93.svg)

1. Write the byte in binary: `0x93` = `1001 0011`.
2. Reverse the order, because the **least-significant bit goes first**: `1 1 0 0 1 0 0 1`.
3. Put the start bit in front and the stop bit behind:

```
idle   start   D0 D1 D2 D3 D4 D5 D6 D7   stop   idle
 1       0      1  1  0  0  1  0  0  1     1      1
```

The receiver reverses the process: it waits for the line to fall, samples the eight bits in turn, stacks them with the first in the lowest position, checks that the stop bit is HIGH, and ends up with `0x93`. A waveform on an oscilloscope reads left to right as the binary number **backwards**.

### A keystroke is a number

The PC does not send the *letter* `a`. It sends the number **97**, and the terminal agrees to draw 97 as `a`. That agreement is the ASCII table:

| Key | Decimal | Hex | Binary |
|---|---|---|---|
| `a` | 97 | `0x61` | `0110 0001` |
| `b` | 98 | `0x62` | `0110 0010` |
| Enter | 13 + 10 | `0x0D 0x0A` | `\r` `\n` |

In C, `'a'` is just another way to write `97`. So `case 'a':` compares the received byte with the number 97, and `default:` catches every other value from 0 to 255.

### One keystroke on the wire

![One UART frame carrying the letter 'a' (0x61), and the instant the RXNE flag is raised](uart_frame_diagram.svg)

`0x61` is `0110 0001`, but it travels as `1 0 0 0 0 1 1 0` because the lowest bit goes first. A whole frame takes about **87µs**, and for that time the CPU is **not involved**: the USART1 peripheral shifts the bits in by itself. Only when the stop bit ends does the byte land in the data register `DR` and the hardware raise the `RXNE` flag. That instant is what everything below hangs off.

### From the flag to your function

`HAL_UART_RxCpltCallback()` is never called by this project's own code, yet it runs. The chain (RXNE → `USART1_IRQHandler` → HAL → callback) is drawn in [6.1's interrupt chain diagram](../6_1_UART_HELLO_WORLD/interrupt_chain_diagram.svg). Two details fail **silently** when wrong:

- **Three switches must all be on:** `RXNEIE` in `USART1->CR1` (set by `HAL_UART_Receive_IT()`), the NVIC line for USART1 (only if the CubeMX **NVIC** box is ticked), and `USART1_IRQHandler()` in `stm32f4xx_it.c` (generated from the same tick).
- **The callback is a weak symbol.** HAL ships an empty `__weak` `HAL_UART_RxCpltCallback`; your definition replaces it at link time, but a one-letter typo gives no error and HAL keeps calling the empty one.

### From keystroke to LED

![The interrupt side leaves a note; the main loop side acts on it](command_flow_diagram.svg)

The work is split across two places on purpose.

- **Interrupt side**, a few microseconds, only when a byte arrives: copy the byte into a mailbox (`rx_cmd`), raise a flag (`rx_ready = 1`), re-arm the receiver.
- **Main-loop side**: every pass of `while(1)` checks the flag. Almost always it is `0` and the function returns at once. When it is `1`, the loop takes the command, clears the flag, and does the slow part: drive the pin and send the reply.

The reply (`HAL_UART_Transmit`) blocks until every character is out: about 1ms for `LED ON`, about 4.6ms for the warning. Inside an interrupt that would hold off every other interrupt of the same or lower priority, SysTick among them, so an interrupt should be as short as a doorbell press: record that it happened and get out.

### Why a library

| File | Contains | Visible to `main.c`? |
|---|---|---|
| `uart_led.h` | The four function prototypes | Yes: the public contract |
| `uart_led.c` | `led_uart`, `rx_byte`, `rx_cmd`, `rx_ready`, `UART_LED_Send()` and the function bodies | Only the four public functions |
| `main.c` | `#include "uart_led.h"`, two HAL callbacks that forward to the library, three one-line calls | — |

`static` means "private to this `.c` file": `main.c` cannot touch `rx_ready` by accident, and the library can be rewritten as long as the four prototypes stay the same. The UART is passed in as an **argument** (`UART_LED_Init(&huart1)`), so moving to `USART2` is a one-argument change in `main.c`.

## Hardware & wiring

The LED is **LD2**, the green one on the board, wired to `PA5`. It lights when `PA5` is HIGH (`GPIO_PIN_SET`). No resistor or wire needed.

As in [6.1](../6_1_UART_HELLO_WORLD), the UART is `USART1` on `PA9`/`PA10` with an external **CH340G** module (jumper on **3V3**; it appears as `USB-SERIAL CH340 (COMx)` in Device Manager). The module details and the logic-level check are in 6.1.

![Wiring diagram](wiring_diagram.svg)

| CH340G | Nucleo-F446RE | Location |
|---|---|---|
| `TXD` | **PA10** (`USART1_RX`) | header `CN9`, pin `D2` |
| `RXD` | **PA9** (`USART1_TX`) | header `CN5`, pin `D8` |
| `GND` | `GND` | power header `CN6` |
| `3V3` / `5V` | **not connected** | — |

Three wires and two USB cables, one per board. TX goes to RX; GND is mandatory because a logic level needs a shared reference; `VCC` stays unconnected because both boards are already powered over USB.

**Terminal:** open the CH340G's COM port at `115200`, 8 data bits, no parity, 1 stop bit, no flow control (in Hercules: Baud `115200`, Data size `8`, Parity `none`, Handshake `OFF`). Send the letter alone; no line terminator is needed.

## CubeMX setup

1. `File → New → STM32 Project` → **Board Selector** → **NUCLEO-F446RE** → Next. Project name `6_2_UART_Control_LED`, language `C`.
2. Answer **Yes** to *"Initialize all peripherals with their default Mode?"*. This sets `PA5` as `GPIO_Output` labelled **`LD2`** and selects the accurate 8MHz `HSE` clock from the ST-Link.
3. **Pinout view**: click `PA5` and confirm `GPIO_Output` with label `LD2` (add it with right-click → *Enter User Label* if missing). The label generates `LD2_Pin` and `LD2_GPIO_Port`, which the library uses.
4. **Connectivity → USART1** → **Mode** = `Asynchronous`; Baud Rate `115200`, Word Length `8 Bits`, Parity `None`, Stop Bits `1`, Data Direction `Receive and Transmit`, Over Sampling `16 Samples`.
5. Confirm `PA9` = `USART1_TX` and `PA10` = `USART1_RX` (move them back if CubeMX picked `PB6`/`PB7`).
6. **USART1 → NVIC Settings** (or **System Core → NVIC**): tick **USART1 global interrupt**. Without it the callback is never reached and the build gives no warning.
7. **Project Manager → Code Generator** → **`Copy only the necessary library files`**.
8. **GENERATE CODE** (`Ctrl+S`). Verify that `stm32f4xx_hal_msp.c` contains `HAL_NVIC_EnableIRQ(USART1_IRQn)` and `stm32f4xx_it.c` contains `USART1_IRQHandler()`.

## Code

Create `Core/Inc/uart_led.h` and `Core/Src/uart_led.c` (CubeIDE already searches `Core/Inc` and builds `Core/Src`). In `main.c`, write only between the `USER CODE BEGIN/END` markers; anything outside them is erased when CubeMX regenerates.

Reference solution: [uart_led.h](Core/Inc/uart_led.h), [uart_led.c](Core/Src/uart_led.c), [main.c](Core/Src/main.c).

### API

`uart_led.h` holds only these prototypes behind an include guard (`#ifndef` … `#endif`, so a second inclusion is skipped) and includes `main.h` for HAL and the generated `LD2_*` names.

| Function | Parameters | Returns | Purpose |
|---|---|---|---|
| `UART_LED_Init` | `UART_HandleTypeDef *huart` | – | Keep the UART, switch the LED off, send the greeting, arm reception |
| `UART_LED_Process` | – | – | Call from `while(1)`: act on a waiting command and reply |
| `UART_LED_RxCallback` | `UART_HandleTypeDef *huart` | – | Interrupt side: store the byte, raise the flag, re-arm |
| `UART_LED_ErrorCallback` | `UART_HandleTypeDef *huart` | – | Recover from an overrun |

### State in `uart_led.c`

Define each function above the code that calls it. The library keeps four private (`static`) variables:

```c
static UART_HandleTypeDef *led_uart;   // the UART given to Init
static uint8_t rx_byte;                // landing slot HAL writes the received byte into
static volatile uint8_t rx_cmd;        // mailbox: the command, kept for the main loop
static volatile uint8_t rx_ready;      // flag: 1 = a new command is waiting
```

`rx_cmd` and `rx_ready` are written by the interrupt and read by `while(1)`, so they are `volatile`: otherwise the compiler may read them once and never again (works at `-O0`, breaks at `-O2`). `rx_byte` is only touched by HAL and the callback, so it is not. A private helper `UART_LED_Send(msg)` wraps `HAL_UART_Transmit(led_uart, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY)`.

### Idea per function

- **`UART_LED_Init`**: 1) store `huart` in `led_uart` (the parameter dies when Init returns, the static lives on); 2) switch `LD2` off; 3) send the greeting, which proves the TX wire, baud rate and COM port; 4) call `HAL_UART_Receive_IT(led_uart, &rx_byte, 1)`. It receives nothing itself: it enables the interrupt and returns.
- **`UART_LED_RxCallback`** (interrupt side): 1) return if `huart != led_uart` (HAL calls this for every UART on the chip); 2) copy the byte into the mailbox and raise the flag; 3) re-arm.
  ```c
  rx_cmd   = rx_byte;                           // copy out before the next byte overwrites rx_byte
  rx_ready = 1;
  HAL_UART_Receive_IT(led_uart, &rx_byte, 1);   // one-shot: re-arm or the board goes deaf
  ```
- **`UART_LED_ErrorCallback`**: USART1 has no receive queue. If a byte arrives before the previous one is read, the overrun flag is raised and HAL cancels the armed reception. Clear it and re-arm, or the UART goes permanently deaf while sending still looks healthy.
  ```c
  __HAL_UART_CLEAR_OREFLAG(led_uart);
  HAL_UART_Receive_IT(led_uart, &rx_byte, 1);
  ```
- **`UART_LED_Process`** (main loop): 1) return at once if `rx_ready` is 0, since this runs millions of times a second; 2) copy `rx_cmd` into a local, then clear the flag; 3) `switch` on the command: `'a'` sets `LD2` (`GPIO_PIN_SET`) and replies `LED ON`, `'b'` resets it and replies `LED OFF`, `'\r'` and `'\n'` are ignored, `default` sends the warning.
  ```c
  uint8_t cmd = rx_cmd;   // local copy stays valid even if a new byte arrives
  rx_ready = 0;
  ```
  *Watch out:* copy the command **before** clearing the flag. Ignoring `'\r'` and `'\n'` stops terminals that append Enter from adding two warnings after every `a`.

### Where it goes in `main.c`

| Marker | Code |
|---|---|
| `Includes` | `#include "uart_led.h"` |
| `0` | `HAL_UART_RxCpltCallback` and `HAL_UART_ErrorCallback`, each one line forwarding to the library |
| `2` | `UART_LED_Init(&huart1);` after `MX_GPIO_Init()` and `MX_USART1_UART_Init()` |
| `3` (in `while(1)`) | `UART_LED_Process();` |

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  UART_LED_RxCallback(huart);
}
```

The HAL callbacks live in `main.c` because a weak symbol may be overridden **once** per program: if the library defined it, a second UART module (GPS, Bluetooth) would fail to link. One definition in `main.c` forwarding to each library lets every module have its turn.

## Folder structure

```
6_2_UART_Control_LED/
├── Core/Inc/uart_led.h                - library: public prototypes
├── Core/Src/uart_led.c                - library: state, receive, dispatch, replies
├── Core/Src, Core/Inc                 - CubeMX-generated HAL init code + main.c
├── Drivers/                           - STM32 HAL + CMSIS (generated, not hand-edited)
├── 6_2_UART_Control_LED.ioc           - CubeMX pin/clock configuration
├── README.md                          - this file
├── wiring_diagram.svg                 - CH340G to Nucleo wiring
├── uart_frame_diagram.svg             - the letter 'a' as one UART frame
├── command_flow_diagram.svg           - keystroke → interrupt → flag → main loop → LED
└── diagrams/                          - UART basics: uart_connection, uart_frame_format, uart_byte_0x93 (.svg + .png)
```
