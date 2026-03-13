# Austin's Auspicious, Assiduous, Serial Protocol (3ASP)

## Motivation
The STM32 board communicates with the Raspberry Pi over USB. The Pi transmits commands, while the STM32 board responds with statuses.

## Description
This is an ASCII based protocol. Command and status packets start with a `<` and end with a `>`. On boot, the STM32 also emits an unframed ASCII welcome banner:

- `[Onset] Ready\r\n`

## Transport
- Interface: USB CDC (virtual COM port)
- Encoding: ASCII
- Framing: `<...>`

## RX (Host -> STM32)

### Supported commands
- `<S>`
	- Meaning: Status request
	- Action: Triggers an immediate status transmission using the current TX packet format

- `<H>`
	- Meaning: Home command
	- Action: Sends `CMD_SERIAL_ELBOW_HOME` to the elbow service queue

- `<M,VALUE>`
	- Meaning: Move command
	- `VALUE` is parsed as a float using `sscanf("<M,%f>", ...)`
	- `VALUE` is clamped to the elbow travel range `[0, pi/2]` before motion is commanded
	- Action: Sends `CMD_SERIAL_ELBOW_MOVE` with parsed value to the elbow service queue

- `<P,VALUE>`
	- Meaning: Precharge power command
	- `VALUE` is parsed as an integer using `sscanf("<P,%d>", ...)`
	- Action:
		- `VALUE == 0` sends `CMD_SERIAL_PRECHARGE_OFF` to the precharge service queue
		- `VALUE != 0` sends `CMD_SERIAL_PRECHARGE_ON` to the precharge service queue

- `<L,VALUE>` / `<L,VALUE,R,G,B>`
	- Meaning: LED command
	- `VALUE` is parsed as an integer using `sscanf("<L,%d>", ...)`
	- Action:
		- `VALUE == 0` sends `CMD_SERIAL_LED_OFF` to the LED service queue
		- `VALUE == 1` sends `CMD_SERIAL_LED_LAUNCH` to the LED service queue
		- `VALUE == 2` parses `R`, `G`, `B` as integers using `sscanf("<L,%d,%d,%d,%d>", ...)` and sends `CMD_SERIAL_LED_SINGLE_COLOUR` with those colour values to the LED service queue

### RX parser behavior
- Incoming bytes are read from the USB RX ring buffer.
- Parser scans byte-by-byte for framed messages.
- If multiple complete packets are present in one drain cycle, only the most recent complete packet is kept and parsed.
- A new `<` always starts a new candidate frame, even if a previous frame was incomplete.
- Frames that exceed the packet buffer are discarded.
- Bytes outside frames are ignored.
- Unknown packet types are ignored.
- Alphabetic characters are normalized to uppercase before parsing (for example `<h>` and `<p,1>` are accepted).

### RX buffer policy
- RX ring buffer size is `1024` bytes.
- Ring buffer policy is drop-new-on-full.
- If the buffer is full, newly received bytes are dropped.
- Existing unread bytes are not overwritten.

## TX (STM32 -> Host)

### Packet format
- `<SW2,SW3,ELBOW_STATUS,PRECHARGE_STATUS,LED_STATUS>`
- All fields are emitted as unsigned integer ASCII values.

### Field definitions
- `SW2`: limit switch 2 state
	- `0` = `LIMITSWITCH_RELEASED`
	- `1` = `LIMITSWITCH_PRESSED`

- `SW3`: limit switch 3 state
	- `0` = `LIMITSWITCH_RELEASED`
	- `1` = `LIMITSWITCH_PRESSED`

- `ELBOW_STATUS`: `elbow_to_serial_status_t`
	- `0` = `STATUS_ELBOW_SERIAL_NEEDS_HOME`
	- `1` = `STATUS_ELBOW_SERIAL_HOMING`
	- `2` = `STATUS_ELBOW_SERIAL_HOME_ERROR`
	- `3` = `STATUS_ELBOW_SERIAL_HOME_SUCCESS`
	- `4` = `STATUS_ELBOW_SERIAL_MOVING`
	- `5` = `STATUS_ELBOW_SERIAL_MOVE_SUCCESS`
	- `6` = `STATUS_ELBOW_SERIAL_MOVE_ERROR`

- `PRECHARGE_STATUS`: `precharge_to_serial_status_t`
	- `0` = `STATUS_PRECHARGE_SERIAL_OFF`
	- `1` = `STATUS_PRECHARGE_SERIAL_PRECHARGE_ON`
	- `2` = `STATUS_PRECHARGE_SERIAL_MAIN_POWER_ON`

- `LED_STATUS`: `led_to_serial_status_t`
	- `0` = `STATUS_LED_SERIAL_DISABLED`
	- `1` = `STATUS_LED_SERIAL_OFF`
	- `2` = `STATUS_LED_SERIAL_LAUNCH`
	- `3` = `STATUS_LED_SERIAL_SINGLE_COLOUR`

### TX behavior
- TX uses the current values of:
	- limit switch 2 state
	- limit switch 3 state
	- latest elbow status
	- latest precharge status
	- latest LED status
- TX is triggered by any of the following:
	- host sends `<S>`
	- either limit switch changes state
	- elbow service publishes a new status
	- precharge service publishes a new status
	- LED service publishes a new status
- TX is non-blocking and checks CDC busy state before starting a new transfer.
- If USB is busy or the USB stack rejects the transmit request, TX retries in later serial service iterations.
- The serial service task runs every `10 ms`, so status responses and retries are quantized to that cadence.

## Examples
- Host -> STM32
	- `<S>`
	- `<H>`
	- `<M,1.5708>`
	- `<P,1>`
	- `<P,0>`
	- `<L,1>`
	- `<L,2,255,0,128>`
	- `<L,0>`

- STM32 -> Host
	- `[Onset] Ready\r\n`
	- `<1,1,0,0,0>`
	- `<0,1,4,1,1>`
	- `<0,0,5,2,2>`