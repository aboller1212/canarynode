# CANaryNode

A CAN FD sensor node built around an STM32G474. It reads an IMU and an
environmental sensor and puts the data on a CAN FD bus. The firmware is
bare-metal, written directly against the CMSIS register definitions with
no HAL, mostly because I want to actually understand what every peripheral
is doing at the register level.

## Hardware

Right now everything runs on a Nucleo-G474RE dev board (STM32G474RET6,
Cortex-M4F) while I bring the peripherals up one at a time. The real target
is a custom PCB with:

- STM32G474 MCU
- BMI270 IMU on SPI
- BME280 temperature/pressure/humidity sensor on I2C (SDO grounded, so
  address 0x76)
- TI TCAN CAN FD transceiver
- USB-C for power

The board is programmed over SWD. I flash it with an external ST-Link V2
rather than the Nucleo's onboard ST-Link (see the note below).

## Layout

- `firmware/` - the STM32 firmware (this is where most of the work is)
- `hardware/` - schematic and board files
- `docs/` - datasheets, reference notes, and bring-up write-ups
- `tools/` - scripts and helpers

## Building

The firmware uses CMake with Ninja and the arm-none-eabi GCC toolchain.

    cd firmware
    cmake -B build -G Ninja
    cmake --build build

That produces `build/canary_firmware` (ELF) and a `.bin` alongside it.

## Flashing

I use an external full-speed ST-Link V2 on SWD (SWDIO to PA13, SWCLK to
PA14, plus GND and 3V3). The Nucleo's onboard ST-Link V3E does not work
reliably from an Apple Silicon Mac, so I keep the Nucleo's own USB
unplugged and power the target from the V2.

Debugging is done with OpenOCD as a GDB server and arm-none-eabi-gdb.

## Status

Getting the peripherals working one at a time.

- LED blink and a timer interrupt (TIM6) are working on hardware.
- FDCAN is set up for internal loopback at 500 kbit/s. Written, not yet
  verified on a real bus.
- I2C master and a BME280 driver are written and compile. Next step is
  confirming the sensor's chip ID over the bus, which will prove the I2C
  side end to end.

Still to do: SPI and the BMI270, sending sensor data out over CAN, moving
off loopback to a real CAN bus, and porting everything to the custom board.
