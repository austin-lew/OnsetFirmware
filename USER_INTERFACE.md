# STM32F411RE Elbow Motion USART Interface

## Overview
Firmware uses a framed ASCII USART protocol to command elbow motion and publish status updates.

- **RX command formats**: `<M,VALUE>`, `<H>`
  - `VALUE` is an absolute elbow target in radians.
- **TX state format**: `<SW2,SW3,ELBOW_STATUS,PRECHARGE_STATUS>`
- **USART**: USART2 @ **115200-8-N-1**

## Hardware Mapping

- **PB1**: Step pulse output
- **PB2**: Direction output
- **PC8**: Homing limit switch input (`SW1`)
- **PC5**: Limit switch input reported as `SW2`
- **PC6**: Limit switch input reported as `SW3`

Inputs PC8/PC5/PC6 are configured with pull-up and EXTI rising/falling. Switch states are encoded as:
- `0` = released
- `1` = pressed

## Command Protocol

### Move Command

```
<M,VALUE>
```

Examples:

```
<M,0.000>
<M,1.5708>
<M,-0.7854>
```

Behavior:
- `VALUE` is interpreted as an **absolute** elbow target angle in radians.
- `VALUE` is clamped to `[0, pi/2]` before conversion (`<0` -> `0`, `>pi/2` -> `pi/2`).
- Command frames must start with `<` and end with `>`.
- Only `M`/`m` command is accepted.
- Commands received while a move is active are ignored.
- If homing has not completed, move commands are rejected and a state packet is transmitted with `ELBOW_STATUS = 0`.

### Home Command

```
<H>
```

Behavior:
- Runs at `1/10` of regular move max speed and acceleration.
- Seeks in the negative-angle direction up to `pi/2` radians of travel.
- On `SW1` press (`PC8` LOW), seek transitions into smooth deceleration stop.
- Backs off in reverse and on `SW1` release (`PC8` HIGH), transitions into smooth deceleration stop.
- On success, marks the axis homed so move commands are accepted.

## State Packet Protocol

### Packet Format

```
<SW2,SW3,ELBOW_STATUS,PRECHARGE_STATUS>
```

Field definitions:
- `SW2`: PC5 switch state (`0` released, `1` pressed)
- `SW3`: PC6 switch state (`0` released, `1` pressed)
- `ELBOW_STATUS`:
  - `0` = needs home
  - `1` = homing
  - `2` = home error
  - `3` = home success
  - `4` = moving
  - `5` = move success
  - `6` = move error
- `PRECHARGE_STATUS`: always `1`

Packets are transmitted whenever any field changes.

## Motion Conversion Constants

- Stepper: 200 full steps/rev
- Microstepping: 1/16
- Gear ratio (stepper:elbow): 7:1

So elbow microsteps per elbow revolution:

```
200 * 16 * 7 = 22400 microsteps/rev
```

Target conversion:

```
target_microsteps = round(VALUE_radians * 22400 / (2*pi))
```

## Motion Profile

Elbow moves use a trapezoidal profile with fixed firmware parameters:
- Maximum speed: `9600` microsteps/s
- Acceleration/deceleration: `24000` microsteps/s²

## Notes

- `SW1` is used for homing stop/back-off behavior.
- `SW2`/`SW3` are currently report-only and do not block or stop normal motion.
- During active motion, switch transitions are still reported via state packets.
- TX packet format remains `<SW2,SW3,ELBOW_STATUS,PRECHARGE_STATUS>` (no dedicated `SW1` field).
- If `SW1` is pressed during a regular move, firmware performs an immediate hard stop, clears homed state, and requires a new home (`ELBOW_STATUS=0`) before the next move.
