# CANaryNode

A CAN FD sensor node built around an STM32G474. It reads an IMU and an
environmental sensor and puts the data on a CAN bus. The firmware is
bare-metal — no HAL, no RTOS, no generated code, just CMSIS headers and the
reference manual. I'm building it this way because I want to understand what
every peripheral is actually doing at the register level, not call somebody's
library function and hope.

I designed the board as well, so the schematic and layout live in this repo
alongside the firmware.

## Hardware

The custom board is a 4-layer PCB with:

- STM32G474RCT6 (Cortex-M4F, 256K flash)
- BMI270 IMU on SPI
- BME280 temperature/pressure/humidity sensor on I2C (SDO tied to ground, so
  address 0x76)
- TI TCAN CAN FD transceiver
- USB-C
- Four LEDs, a button, and headers for SWD and UART

Most of the driver work was done on a Nucleo-G474RE first, bringing up one
peripheral at a time, so that porting to the real board would mostly be
remapping pins. The custom board is here now and boots.

A couple of things differ between the two: the Nucleo has a G474RET6 (512K
flash) and routes CAN to FDCAN1 on PA11/PA12, while my board uses an RCT6
(256K) and puts CAN on PB5/PB6, which is FDCAN2. That's an instance swap, not
just a pin change, so it needs its own message RAM offset. My board uses
PA11/PA12 for USB instead.

Pin map on the custom board:

| Function | Pins |
| --- | --- |
| LEDs | PB0, PB1, PB2, PC4 |
| I2C1 (BME280) | PA15 SCL, PB7 SDA |
| SPI1 (BMI270) | PA4 CS, PA5 SCK, PA6 MISO, PA7 MOSI |
| FDCAN2 | PB5 RX, PB6 TX |
| USB | PA11, PA12 |
| SWD | PA13, PA14, NRST on PG10 |
| USART2 | PA2, PA3 |
| BOOT0 jumper | PB8 |
| IMU interrupt | PB10 |

## Layout

- `firmware/` — the STM32 firmware, where most of the work is
- `hardware/` — schematic and board files
- `docs/` — datasheets and my bring-up notes
- `tools/` — scripts

## Building

CMake + Ninja with the arm-none-eabi GCC toolchain.

    cd firmware
    cmake -B build -G Ninja
    cmake --build build

Produces `build/canary_firmware` (ELF) and a `.bin` next to it.

## Flashing

Two ways, depending on the board.

The custom board can be programmed over USB with no debugger at all. The
STM32 has a bootloader burned into ROM — jumper BOOT0 high, plug in USB-C,
and it enumerates as a DFU device:

    STM32_Programmer_CLI -c port=USB1 -w build/canary_firmware.bin 0x08000000

Pull the jumper and power cycle to run the firmware instead. That's handy for
quick iteration, but DFU only programs — for actual debugging I use OpenOCD as
a GDB server with arm-none-eabi-gdb over SWD.

On the Nucleo I use an external ST-Link V2 on SWD (SWDIO to PA13, SWCLK to
PA14, plus GND and 3V3). The onboard ST-Link V3E doesn't work reliably from an
Apple Silicon Mac, so its USB stays unplugged and the target gets power from
the V2.

## Status

Working and verified on the custom board:

- It powers up, enumerates over USB, and takes a verified flash write.
- LED blink driven by a TIM6 timer interrupt. Small thing on its own, but it's
  the first proof that my startup code, vector table, linker script and clock
  setup are all correct on this silicon.
- I2C master and the BME280 driver, remapped to PA15/PB7. The sensor answers
  with the right chip ID.

Verified earlier on the Nucleo, not yet ported over:

- The full BME280 read path — calibration read and reassembly plus Bosch's
  fixed-point compensation math. It streams real temperature, pressure and
  humidity, and the humidity climbs when you breathe on it.
- FDCAN at 500 kbit/s in internal loopback. A frame goes out, loops through
  the peripheral's message RAM, and comes back byte for byte.

In progress:

- BMI270 over SPI. The driver is written, including the 8 KB configuration
  blob the sensor needs uploaded before it will do anything, but the chip
  isn't answering yet — the chip ID read comes back wrong. The wiring checks
  out against the schematic, so it's something in the SPI setup or the read
  timing. Waiting on a 1.27mm SWD adapter so I can put a debugger on it
  instead of guessing.

Not working yet:

- CAN on a real bus. The MCU side checks out — I can read the FDCAN registers
  mid-transmit and see clean frames going out with no errors — but the
  transceiver breakout I was using never drives the bus. CANH and CANL sit at
  0.3 V when they should idle around 2.3 V, so the transceiver isn't operating
  at all. Either the part is dead or its slew rate resistor is limiting it
  well below 500 kbit/s.

## What's left

Port the remaining peripherals to the custom board, get the BMI270 talking,
write a UART driver so I can print values instead of stepping through a
debugger, pack sensor readings into CAN frames, and get those frames onto a
real bus where my laptop can decode them.
