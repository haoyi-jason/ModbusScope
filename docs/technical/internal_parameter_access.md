# Internal Parameter Access Protocol (Modbus RTU)

## Overview

ModbusScope supports a device-specific internal-parameter access mechanism via
Modbus RTU (FC03 / FC06). This feature allows reading and writing device-internal
parameters that are not directly accessible as ordinary holding registers.

The feature is **RTU-only** and requires logging to be **stopped** before use.

---

## Register Map (0-based protocol addresses)

| Address | Name              | Description                                     |
|---------|-------------------|-------------------------------------------------|
| 98      | Action register   | Write **1** to trigger a read, **3** to trigger a write |
| 100     | Parameter address | 16-bit address of the internal parameter        |
| 101     | Data register #1  | First (or only) 16-bit word of the parameter value |
| 102     | Data register #2  | Second 16-bit word (32-bit parameters only)     |

---

## Read Sequence

1. **FC06** – Write parameter address to register 100.
2. **FC06** – Write `1` to action register (98) to trigger a read.
3. Wait **200 ms** for the device to fetch the value.
4. **FC03** – Read data register(s) starting at 101 (quantity 1 for 16-bit, 2 for 32-bit).

## Write Sequence

1. **FC06** – Write parameter address to register 100.
2. **FC06** – Write Word 1 to register 101.
3. **FC06** – Write Word 2 to register 102 *(32-bit only)*.
4. **FC06** – Write `3` to action register (98) to trigger a write.
5. Wait **200 ms** for the device to store the value.
6. **FC03** – Read back registers 101–102 (readback displayed in UI).

> **Note:** FC16 (Write Multiple Registers) is intentionally **not** used.
> Each register is written individually with FC06.

---

## Endianness / Word Order

Word ordering for 32-bit values is **not** handled by ModbusScope. The UI
presents two separate 16-bit fields (Word 1 = reg 101, Word 2 = reg 102), and
the user is responsible for placing the correct words in the correct fields.

---

## UI Usage

Open the dialog via **Project → Internal Parameter Access (RTU)...**

| Control           | Description                                            |
|-------------------|--------------------------------------------------------|
| Connection        | Select the RTU connection (only serial connections work) |
| Slave ID          | Modbus slave/device address (1–255)                    |
| Parameter Address | 16-bit internal parameter address                      |
| Mode              | **16-bit** or **32-bit (two words)**                   |
| Word 1 (reg 101)  | First 16-bit word (write: value to send; read: populated on success) |
| Word 2 (reg 102)  | Second 16-bit word (32-bit mode only)                  |
| Read              | Execute the read sequence; result shown in Word 1/Word 2 |
| Write             | Execute the write sequence; readback shown on completion |

The status bar at the bottom of the dialog reports success or error details.

---

## Constraints

- **RTU only**: TCP connections are rejected with an error message.
- **Stop logging first**: Starting an internal-parameter transaction while
  polling is active is rejected to prevent serial-port conflicts.
- **One transaction at a time**: Clicking Read/Write while a transaction is in
  progress has no effect until the previous transaction completes.
