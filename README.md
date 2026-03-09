# STM32F411 Elbow Motion Firmware — Technical README

## 1) Purpose and Scope

This firmware runs on an STM32F411 Nucleo board and controls a stepper-driven elbow axis over USART.

Implemented capabilities:
- Receive framed UART commands for move (`<M,VALUE>`) and home (`<H>`).
- Convert angle targets to signed microstep positions.
- Execute motion using a trapezoidal profile (with triangular fallback on short moves).
- Publish compact state packets when switch/motion state changes.
- Use SW1 (PC8) for homing stop/back-off and report SW2/SW3 with software debounce.

For operator-facing usage examples, see `QUICK_START.md` and `USER_INTERFACE.md`.

---

## 2) Repository Structure (Relevant Files)

- `Core/Src/main.c`
  - Application loop, frame parser, command processing, state packet emission, switch state management.
- `Core/Inc/stepper_motor.h`
  - Stepper data model and motion-control API.
- `Core/Src/stepper_motor.c`
  - GPIO pulse generation, direction control, absolute move implementation, trapezoidal profile.
- `Core/Src/gpio.c`
  - GPIO/EXTI configuration for STEP/DIR and switch interrupts.
- `Core/Src/stm32f4xx_it.c`
  - `EXTI9_5_IRQHandler`/`EXTI15_10_IRQHandler` routing for switch/button EXTI lines.
- `cmake/stm32cubemx/CMakeLists.txt`
  - Source list, include paths, STM32 HAL driver object library wiring.
- `CMakeLists.txt`, `CMakePresets.json`, `cmake/gcc-arm-none-eabi.cmake`
  - Toolchain and build entry points.

---

## 3) Hardware and Signal Mapping

### MCU + Peripheral
- MCU family define: `STM32F411xE`
- UART: `USART2`, 115200 baud, 8-N-1

### GPIO Mapping
- `PB1` (`STEP_Pin`): Step pulse output
- `PB2` (`DIR_Pin`): Direction output
- `PC8` (`SW1_Pin`): Homing switch input (pull-up, EXTI)
- `PC5` (`SW2_Pin`): Limit switch input (pull-up, EXTI)
- `PC6` (`SW3_Pin`): Limit switch input (pull-up, EXTI)
- `PA5` (`LD2_Pin`): On-board LED toggled every generated step

Switch normalization in software:
- Electrical low (`GPIO_PIN_RESET`) => pressed (`1`)
- Electrical high => released (`0`)

---

## 4) Protocol Design

## RX Command Frame
Expected frame format:

```text
<M,VALUE>
```

and homing command:

```text
<H>
```

- `<` and `>` delimit a single frame.
- Command key accepts `M`/`m` (move) and `H`/`h` (home).
- `VALUE` is parsed as `float` radians via `strtof`.
- Parsed `VALUE` is clamped to `[0, π/2]` before motion execution.
- Any malformed frame is ignored.
- If the motor is already moving, new commands are ignored.
- Before homing succeeds, `<M,VALUE>` is rejected and a state packet is transmitted with `ELBOW_STATUS=0`.

Parser implementation details (`Parse_MoveFrame` in `main.c`):
- Validates framing and minimum length.
- Copies payload into a bounded local buffer.
- Verifies command and comma separator.
- Rejects partial/invalid numbers by checking parse end pointer.

## TX State Frame
Firmware transmits:

```text
<SW2,SW3,ELBOW_STATUS,PRECHARGE_STATUS>
```

Fields:
- `SW2`: PC5 state (`0` released, `1` pressed)
- `SW3`: PC6 state (`0` released, `1` pressed)
- `ELBOW_STATUS`: `0` needs home, `1` homing, `2` home error, `3` home success, `4` moving, `5` move success, `6` move error
- `PRECHARGE_STATUS`: constant `1`

Transmission policy (`Maybe_SendStatePacket`):
- Keeps a cached `last_sent_packet`.
- Sends only when any field changes.
- Reduces redundant UART traffic while preserving state transitions.

---

## 5) Motion Model and Kinematics

## Mechanical constants
Configured in `main.c`:
- Full steps/rev = 200
- Microstep division = 16
- Gear ratio (motor:elbow) = 7:1

Therefore:

```text
elbow_microsteps_per_rev = 200 * 16 * 7 = 22400
```

## Angle conversion
`RadiansToMicrosteps` computes:

```text
target_microsteps = round(radians * 22400 / (2π))
```

Signed rounding is implemented explicitly for positive/negative values.

## Absolute target behavior
For each accepted command:
- Clamp target radians to `[0, π/2]`.
- Convert clamped target radians to absolute microsteps.
- Compute delta from `current_position_microsteps`.
- Set direction from delta sign.
- Execute `abs(delta)` steps with profile shaping.

## Homing behavior
- `<H>` seeks toward the negative-angle direction.
- Seek distance is limited to `π/2` radians of equivalent travel.
- Homing seek/backoff runs at `1/10` of regular move profile (`max_steps_per_second`, `accel_steps_per_second2`).
- On SW1 press (`PC8` LOW), approach transitions to smooth deceleration stop.
- Firmware then backs off in reverse, and on SW1 release (`PC8` HIGH) transitions to smooth deceleration stop, then marks homed and sets `current_position_microsteps=0`.

The position accumulator is updated per step in `Stepper_Step`, so state remains consistent across commands.

---

## 6) Trapezoidal Profile Implementation

Primary motion API:
- `Stepper_MoveToPositionMicrosteps(...)` in `stepper_motor.c`

Parameters used by caller (`Process_Command` in `main.c`):
- `max_steps_per_second = 9600`
- `accel_steps_per_second2 = 24000`

Algorithm summary:
1. Initialize speed (`current_speed`) with a low non-zero start.
2. At each step, compute distance-to-stop estimate:
   - `steps_to_stop = v² / (2a)`
3. If `steps_to_stop >= steps_remaining`, decelerate; else accelerate toward max speed.
4. Generate one step pulse and wait `1/current_speed` seconds (converted to microseconds).

Behavioral notes:
- Short moves naturally become triangular profiles.
- A minimum pulse interval clamp is applied in code (`>= 50 µs`).
- Motion is blocking (executed in-line), but periodic callback service is invoked every step.

---

## 7) Real-Time Servicing During Motion

To keep I/O state responsive while a move is in progress:
- `Stepper_SetServiceCallback(Motion_ServiceCallback)` is registered at startup.
- The callback runs once per generated step.
- Callback actions:
  - Refresh debounced switch states.
  - Emit state packet if changed.

This avoids waiting for motion completion before state updates are reported.

---

## 8) Switch Handling and Debounce

### Interrupt path
- `gpio.c` configures PC8/PC5/PC6 as EXTI inputs and enables `EXTI9_5_IRQn`.
- `stm32f4xx_it.c` dispatches these lines through `HAL_GPIO_EXTI_IRQHandler`.
- `HAL_GPIO_EXTI_Callback` in `main.c` captures candidate switch state + timestamp.

### Debounce path
`Refresh_SwitchStates` in `main.c` implements time-based debounce:
- Debounce window: `20 ms`
- Tracks:
  - last stable state
  - candidate state
  - candidate-change timestamp
- Promotes candidate to stable state only if unchanged for debounce duration.

When a stable state changes, it is reflected in the next state comparison and transmitted if different.
During homing, SW1 press can stop motion through EXTI callback handling.

---

## 9) Execution Flow

Startup (`main`):
1. HAL init + clocks
2. GPIO + USART init
3. `Stepper_Init`
4. Register motion service callback
5. Initial switch sample
6. Welcome banner
7. Initial state packet

Main loop:
1. Poll UART for one byte (`HAL_UART_Receive(..., timeout=50ms)`).
2. Assemble framed command between `<` and `>`.
3. On complete frame, call `Process_Command`.
4. Independently refresh switch debounce state.
5. Conditionally transmit state packet.

Command execution (`Process_Command`):
1. Parse and validate frame.
2. Reject if currently moving.
3. Set status to moving and transmit update.
4. Execute absolute move profile.
5. Set status to move success and transmit update.

---

## 10) Build System Notes

- Build system: CMake + Ninja
- Toolchain file: `cmake/gcc-arm-none-eabi.cmake`
- Key generated output: `build/Debug/NucleoFirmware.elf`

`cmake/stm32cubemx/CMakeLists.txt`:
- Adds HAL/CMSIS include paths and compile definitions.
- Compiles HAL sources into `STM32_Drivers` object library.
- Links application sources including `Core/Src/stepper_motor.c`.

Typical VS Code tasks available in this workspace:
- `configure`
- `build`
- `clean`
- `rebuild`

---

## 11) Current Behavioral Constraints

- Motion commands are single-threaded and blocking.
- Move commands are gated until a successful homing command is completed.
- New `<M,VALUE>` commands are ignored while moving.
- SW1 is used for homing stop/back-off and as a regular-move safety interlock.
- If SW1 is pressed during a regular move, firmware hard-stops immediately and marks axis as needs-home (`ELBOW_STATUS=0`).
- `PRECHARGE_STATUS` is fixed to `1`.

---

## 12) Verification Checklist (Hardware-in-the-loop)

1. After reset, send `<M,0.5>` and verify movement is rejected with `ELBOW_STATUS=0` state output.
2. Send `<H>` and verify seek-to-SW1-press then back-off-until-release behavior.
3. Confirm post-home state transitions `ELBOW_STATUS: 5 -> 4 -> 5` around move commands.
4. Send `<M,-0.7854>`, `<M,0.0>`, `<M,1.5708>`, `<M,2.0>` and verify clamp to `[0, π/2]`.
5. Toggle PC5/PC6 and verify `SW2/SW3` field updates in TX packets.
6. Confirm generated firmware image loads and runs from `NucleoFirmware.elf`.

---

## 13) Cross-References

- User-level operation: `QUICK_START.md`
- Protocol-focused usage notes: `USER_INTERFACE.md`
- Elbow feature change log/context: `ELBOW_MOTION_IMPLEMENTATION.md`
