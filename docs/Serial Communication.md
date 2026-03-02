# Austin's Auspicious, Assiduous, Serial Protocol (3ASP)

## Motivation
The STM32 board communicates with the Raspberry Pi over USB. The Pi transmits commands, while the STM32 board responds with statuses.

## Description
This is an ASCII based protocol. Each packet starts with a `<` and ends with a `>`.

## Transport
- Interface: USB CDC (virtual COM port)
- Encoding: ASCII
- Framing: `<...>`

## RX (Host -> STM32)

### Supported commands
- `<H>`
	- Meaning: Home command
	- Action: Sends `CMD_HOME` to the elbow service queue

- `<M,VALUE>`
	- Meaning: Move command
	- `VALUE` is parsed as a float using `sscanf("<M,%f>", ...)`
	- Action: Sends `CMD_MOVE` with parsed value to the elbow service queue

- `<P,VALUE>`
	- Meaning: Precharge power command
	- `VALUE` is parsed as an integer using `sscanf("<P,%d>", ...)`
	- Action:
		- `VALUE == 0` sends `CMD_OFF` to the precharge service queue
		- `VALUE != 0` sends `CMD_ON` to the precharge service queue

### RX parser behavior
- Incoming bytes are read from the USB RX ring buffer.
- Parser scans byte-by-byte for framed messages.
- If multiple complete packets are present in one drain cycle, only the most recent complete packet is kept and parsed.
- Bytes outside frames are ignored.
- Unknown packet types are ignored.
- Alphabetic characters are normalized to uppercase before parsing (for example `<h>` and `<p,1>` are accepted).

### RX buffer policy
- Ring buffer policy is drop-new-on-full.
- If the buffer is full, newly received bytes are dropped.
- Existing unread bytes are not overwritten.

## TX (STM32 -> Host)

### Packet format
- `<SW2,SW3,ELBOW_STATUS,PRECHARGE_STATUS>`
- All fields are emitted as unsigned integer ASCII values.

### Field definitions
- `SW2`: limit switch 2 state
	- `0` = `LIMITSWITCH_RELEASED`
    - `1` = `LIMITSWITCH_PRESSED`

- `SW3`: limit switch 3 state
	- `0` = `LIMITSWITCH_RELEASED`
    - `1` = `LIMITSWITCH_PRESSED`

- `ELBOW_STATUS`: `elbow_to_serial_status_t`
	- `0` = `STATUS_NEEDS_HOME`
	- `1` = `STATUS_HOMING`
	- `2` = `STATUS_HOME_ERROR`
	- `3` = `STATUS_HOME_SUCCESS`
	- `4` = `STATUS_MOVING`
	- `5` = `STATUS_MOVE_SUCCESS`
	- `6` = `STATUS_MOVE_ERROR`

- `PRECHARGE_STATUS`: `precharge_to_serial_status_t`
	- `0` = `STATUS_OFF`
	- `1` = `STATUS_PRECHARGE_ON`
	- `2` = `STATUS_MAIN_POWER_ON`

### TX behavior
- TX is non-blocking and checks CDC busy state before starting a new transfer.
- If USB is busy, TX retries in later service iterations.

## Examples
- Host -> STM32
	- `<H>`
	- `<M,1.5708>`
	- `<P,1>`
	- `<P,0>`

- STM32 -> Host
	- `<1,1,0,0>`
	- `<0,1,4,1>`
	- `<0,0,5,2>`