# OnsetFirmware

STM32G431 firmware for the Onset capstone project at the University of Waterloo. This embedded firmware provides real-time control and USB communication capabilities using the STM32G431KBTx microcontroller.

## Features

- Interrupt-driven limit switches
- Battery voltage sensing
- Event streaming over USB 2.0
- Precharge circuit control
- Hardware timer based stepper motor control
- PWM-based servo control
- LED strip control
- Interrupt driven encoders
- SWD programming/debugging

## Hardware Requirements

- **Main Board**: Custom PCBA with the STM32G431KBTx
- **Debugger**: ST-Link V2/V3 or compatible SWD debugger
- **USB Cable**: USB-C (for programming and communication)

## Prerequisites

### Build Tools
- **CMake** 3.20 or higher
- **Ninja** build system
- **ARM GNU Toolchain**: `arm-none-eabi-gcc`
- **OpenOCD** (for debugging)

## Project Structure

```
Core/              # Application code (main, peripherals, FreeRTOS tasks)
  Inc/             # Header files
  Src/             # Source files
Drivers/           # STM32 HAL drivers and CMSIS
Middlewares/       # FreeRTOS and USB middleware
USB_Device/        # USB device configuration and handlers
cmake/             # CMake toolchain files
build/             # Build output directory
```

## Building

### Configure and Build in VS Code

1. Open the Command Palette: `Ctrl+Shift+P`
2. Run: **CMake: Configure** to set up the project
3. Run: **CMake: Build** to compile the firmware

**Output**: `build/Debug/OnsetFirmware.elf`

Alternatively, use the CMake extension sidebar for one-click building.

## Debugging

### Start Debugging in VS Code

1. Ensure your ST-Link debugger is connected
2. Press `F5` or use Command Palette: `Ctrl+Shift+P` → **Debug: Start Debugging**
3. The firmware will be built automatically and loaded onto the device
4. Use the Debug sidebar to step through code, set breakpoints, and inspect variables

**Debug Configuration**: [.vscode/launch.json](.vscode/launch.json)

### Manual GDB Server (Optional)
If you need to start the GDB server manually:
```powershell
openocd -f interface/stlink.cfg -f target/stm32g4x.cfg
```

## USB Communication

The firmware supports USB CDC (Communications Device Class) for serial communication. Connect via:
- **Baud Rate**: 115200
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None

## Development

### Code Generation
The project is generated from `OnsetFirmware.ioc` using STM32CubeMX. To regenerate:
1. Open `OnsetFirmware.ioc` in STM32CubeMX
2. Make your changes
3. Click **Generate Code**
4. Rebuild with CMake

## Troubleshooting

### Build Failures
- Ensure the ARM toolchain is in your PATH: `arm-none-eabi-gcc --version`
- Verify CMake version: `cmake --version`
- Clean and reconfigure: `rm -r build && cmake --preset=default`

### Debugging Issues
- Check ST-Link connection: `openocd -f interface/stlink.cfg`
- Verify device in Device Manager (Windows) or `lsusb` (Linux)
- Ensure the correct SVD file path in `.vscode/launch.json`

## License

This project is part of the University of Waterloo Capstone program.

## Contributors

Austin Lew