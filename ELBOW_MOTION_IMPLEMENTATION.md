# Elbow Motion Feature Implementation

## Scope Implemented

This firmware update adds:

1. Framed USART move command: `<M,VALUE>`
2. Framed homing command: `<H>`
3. Move gating: `<M,VALUE>` rejected until homing succeeds
4. Absolute elbow target interpretation in radians
5. Trapezoidal (with triangular fallback) motion stepping
6. Limit switch interrupt handling from PC8/PC5/PC6
7. Change-driven state packet TX:
   `<SW2,SW3,ELBOW_STATUS,PRECHARGE_STATUS>`

`PRECHARGE_STATUS` is hardcoded to `1` per requirement.

## Files Modified

- `Core/Inc/stepper_motor.h`
- `Core/Src/stepper_motor.c`
- `Core/Src/main.c`
- `Core/Src/gpio.c`
- `Core/Src/stm32f4xx_it.c`
- `Core/Inc/stm32f4xx_it.h`
- `cmake/stm32cubemx/CMakeLists.txt`
- `USER_INTERFACE.md`

## Motion Model

### Mechanical Conversion

Given:
- 200 full steps/rev
- 1/16 microstepping
- 7:1 gear ratio (stepper:elbow)

Elbow microsteps per elbow revolution:

`200 * 16 * 7 = 22400 microsteps/rev`

Angle to microsteps conversion:

`target_microsteps = round(radians * 22400 / (2*pi))`

### Absolute Target Behavior

`VALUE` in `<M,VALUE>` is interpreted as an absolute elbow target.

Before conversion, firmware clamps input radians to `[0, pi/2]`:
- `VALUE < 0` -> `0`
- `VALUE > pi/2` -> `pi/2`

Implementation computes:

- `delta_microsteps = target_microsteps - current_position_microsteps`
- direction based on sign of delta
- total step count from absolute delta

### Trapezoidal Profile

Motion uses fixed firmware constants:

- `max_speed = 9600` microsteps/s
- `accel = decel = 24000` microsteps/s²

At each step, speed is adjusted based on distance-to-stop estimate:

`steps_to_stop = v^2 / (2a)`

- If `steps_to_stop >= steps_remaining`, decelerate
- Else accelerate toward max speed

For short moves, this naturally becomes triangular.

## Runtime State and Packeting

### State Fields

- `SW2`: derived from PC5 pin level
- `SW3`: derived from PC6 pin level
- `ELBOW_STATUS`: `0` needs home, `1` homing, `2` home error, `3` home success, `4` moving, `5` move success, `6` move error
- `PRECHARGE_STATUS`: constant `1`

### Packet Trigger Rule

Firmware caches last transmitted tuple and sends a new packet only when any field changes.

Typical transitions:

1. Boot starts in `ELBOW_STATUS=0` (needs home)
2. Homing start: `1`, then `3` on success or `2` on error
3. Move start: `4`, then `5` on success or `6` on error
4. PC5/PC6 transitions: SW2/SW3 changes

## Limit Switch Interrupt Integration

PC8 (SW1), PC5 (SW2), and PC6 (SW3) are configured as EXTI lines with pull-up and wired in firmware by:

- enabling EXTI9_5 NVIC IRQ
- enabling EXTI15_10 NVIC IRQ (board button line)
- implementing `EXTI9_5_IRQHandler`
- implementing `EXTI15_10_IRQHandler`
- handling pin events through `HAL_GPIO_EXTI_Callback`

Current requirement behavior:

- SW1 events can stop homing approach when pressed
- SW2/SW3 remain report-only in state packet

## Command Handling Notes

Parser accepts framed payloads for move/home commands:

- valid: `<M,1.57>`
- valid: `<H>`
- invalid/malformed frames are ignored
- if motion is active, incoming commands are ignored
- before homing, incoming `<M,VALUE>` is rejected and status is transmitted as needs-home

This preserves deterministic behavior and satisfies the single-command-at-a-time requirement.

## Build Fix Applied

Linker failures occurred due to missing `stepper_motor.c` in CMake source list.

Fix:
- Added `Core/Src/stepper_motor.c` to `MX_Application_Src` in `cmake/stm32cubemx/CMakeLists.txt`

Result:
- Build now links successfully (`NucleoFirmware.elf` produced).

## Validation Performed

- CMake build executed successfully after source-list fix.
- Verified symbols from stepper module link correctly.

## Remaining Hardware Validation (on target)

1. Send `<M,VALUE>` and confirm status packet transitions around motion.
2. After reset, confirm `<M,VALUE>` is rejected until `<H>` succeeds.
3. Send `<H>` and verify seek-to-SW1-press then back-off-until-release behavior.
4. Confirm second command during active motion/homing is ignored.
5. Toggle PC5/PC6 and verify SW2/SW3 packet field updates.
6. Verify final position mapping against known angles (e.g., 0, pi/2, -pi/4).
   - confirm `-pi/4` clamps to `0` and values above `pi/2` clamp to `pi/2`.
