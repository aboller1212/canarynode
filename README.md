# CANaryNode

A CAN FD sensor node built around an STM32G474. It reads an IMU and an
environmental sensor and puts the data on a CAN bus. The firmware is
bare-metal — no HAL, no generated code, just CMSIS headers and the
reference manual. I'm building it this way because I want to understand
what every peripheral is actually doing at the register level, not call
somebody's library function and hope.

## Hardware

The custom board is a 4-layer PCB with:

- STM32G474RCT6 (Cortex-M4F, 256K flash)
- BMI270 IMU on SPI
- BME280 temperature/pressure/humidity sensor on I2C
- TI TCAN CAN FD transceiver
- USB-C

Most of the driver work was done on a Nucleo-G474RE first, bringing up one
peripheral at a time, so that porting to the real board would mostly be
remapping pins. The custom board is here now and boots.

A couple of things differ between the two: the Nucleo has a G474RET6 (512K
flash) and routes CAN to FDCAN1 on PA11/PA12, while my board uses an RCT6
and puts CAN on PB5/PB6, which is FDCAN2. That's an instance swap, not just
a pin change, so it needs its own message RAM offset.

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

On the Nucleo I use an external ST-Link V2 on SWD (SWDIO to PA13, SWCLK to
PA14, plus GND and 3V3). The onboard ST-Link V3E doesn't work reliably from
an Apple Silicon Mac, so its USB stays unplugged and the target gets power
from the V2.

The custom board can be programmed over USB with no debugger at all. The
STM32 has a bootloader burned into ROM — jumper BOOT0 high, plug in USB-C,
and it enumerates as a DFU device:

    STM32_Programmer_CLI -c port=USB1 -w build/canary_firmware.bin 0x08000000

Pull the jumper and power cycle to run the firmware instead. That's handy,
but DFU only programs — for actual debugging I use OpenOCD as a GDB server
with arm-none-eabi-gdb over SWD.

## Status

Working and verified on hardware:

- LED blink and a TIM6 timer interrupt
- I2C master and the full BME280 driver — chip ID, calibration read and
  reassembly, and Bosch's compensation math. It streams real temperature,
  pressure and humidity, and the humidity climbs when you breathe on it.
- FDCAN at 500 kbit/s in internal loopback. A frame goes out, loops through
  the peripheral's message RAM, and comes back byte for byte.
- The custom board itself: it powers up, enumerates over USB, and takes a
  verified flash write.

Written and compiling, not yet tested on hardware:

- SPI master driver
- BMI270 driver, including the 8 KB configuration blob the sensor needs
  uploaded before it will do anything

Not working yet:

- CAN on a real bus. The MCU side is fine — I can see it transmitting
  cleanly in the registers — but the transceiver breakout I bought has a
  10k resistor on its slew rate pin that caps it well below 500 kbit/s.
  Waiting on a different transceiver.

## What's left

Port the pin assignments to the custom board, get the BMI270 talking, pack
sensor readings into CAN frames, and get those frames onto a real bus.
