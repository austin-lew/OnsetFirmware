# Stepper Motor Controller - Quick Start Guide

## Building the Project

### Option 1: VS Code (Recommended)
```
1. Open the workspace in VS Code
2. Press Ctrl+Shift+B to build (or use Tasks → Run Task → build)
3. Output: build/Debug/NucleoFirmware.elf
```

### Option 2: Command Line
```bash
cd build/Debug
ninja
```

## Flashing the Device

### Option 1: VS Code Debugging
```
1. Connect Nucleo board via USB
2. Press F5 (or Debug → Start Debugging)
3. Automatically builds, flashes, and starts debugging
```

### Option 2: STM32CubeProgrammer
```
1. Open STM32CubeProgrammer
2. Connect via ST-LINK
3. File → Open File → build/Debug/NucleoFirmware.elf
4. Ctrl+Shift+D (Download)
```

## Serial Communication

### Setup
1. **Baud Rate**: 115200
2. **Data Bits**: 8
3. **Stop Bits**: 1
4. **Parity**: None
5. **Flow Control**: None

### Terminal Software
- **Windows**: PuTTY, Tera Term, or VS Code Serial Monitor
- **Linux/Mac**: minicom, picocom, or screen

### Finding COM Port
- Windows: Device Manager → Ports → STLINK
- Linux: `ls /dev/tty*`
- Mac: `ls /dev/tty.usb*`

## Motor Driver Connections

```
Nucleo Board    ↔    Motor Driver
PB1 (STEP)      ↔    STEP pin
PB2 (DIR)       ↔    DIR pin
GND             ↔    GND
5V (if avail)   ↔    VDD
```

## Testing the Connection

1. Open serial terminal at 115200 baud
2. Power on the board
3. You should see:
   ```
   ========================================
     STM32F411 STEPPER MOTOR CONTROLLER
     Nucleo Board
   ========================================
   ```

## Basic Command Examples

```
> HELP                    Show available commands
> STATUS                  Display motor status
> STEP                    Execute one step
> STEPS 100               Execute 100 steps
> DIR 0                   Set direction clockwise
> DIR 1                   Set direction counter-clockwise
> SPEED 10                Set 10ms delay between steps (~100 steps/sec)
> RESET                   Reset step counter
> STOP                    Emergency stop
```

## Files Added/Modified

### New Files
- `Core/Inc/stepper_motor.h` - Stepper motor driver header
- `Core/Src/stepper_motor.c` - Stepper motor driver implementation
- `.vscode/launch.json` - Debug and flash configuration
- `.vscode/tasks.json` - Build tasks
- `USER_INTERFACE.md` - Detailed user guide
- `QUICK_START.md` - This file

### Modified Files
- `Core/Src/main.c` - Added command interface and motor control

## Troubleshooting

| Issue | Solution |
|-------|----------|
| No serial output | Check baud rate (115200), verify STLINK driver |
| Commands not working | Type HELP, check command spelling |
| Motor won't move | Verify PB1/PB2 connections, check driver power |
| Board won't flash | Install arm-none-eabi-gdb, check STLINK connection |

## Performance Notes

- **Max step rate**: ~1 kHz (1 ms delay minimum)
- **Typical speeds**: 10-100 steps/sec
- **Power consumption**: <50 mA (board + motor driver dependent)

For detailed information, see **USER_INTERFACE.md**
