# Austin's Auspicious, Assiduous, Serial Protocol (3ASP)

## Motivation
The STM32 board communicates with the Raspberry Pi over USB. The Pi transmits commands, while the STM32 board responds with statuses.

## Description
This is an ASCII based protocol. Each packet starts with a `<` and ends with a `>`.