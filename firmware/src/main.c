//looks in the vendor/cmsis from CMakeLists
#include <stdint.h>
#include "bme280.h" 
#include "i2c.h"
#include "fdcan.h"
#include "blink.h"

int main(void) {

    // -- Peripheral bring-up (buses + LED pin) --
    led_init(); //PA5 -> output, LED off
    i2c_init(); //i2c1 master on PB8/PB9 must come before sensor access
    fdcan_init(); //FDCAN1 @500k, internal loopback

    // --- FDCAN Test ---
    // Send a frame, in loopback it comes straight bazck to our own RX FIFO
    //test for Tx, id=0x123, d0,     d1
    fdcan_send(0x123, 0xDEADBEEF, 0x12345678);
    //initialize varibles to write into
    //volatile because main never reads these, they exist for the debugger
    volatile uint32_t id, dlc, d0, d1;
    //send the locations of each variable 
    fdcan_recv(&id, &dlc, &d0, &d1);

    // -- I2C + BME280 Smoke Test -- 
    // bme280_init() reads chip ID 0xD0: returns 0 if it == 0x60, else 1
    uint8_t bme_status = bme280_init();

    // Fail Trap:
    // On failure: light the LED solid and halt, before TIM6 ever starts
    // Because timer never runs the ISR never fires, nothing toggles
    // PA5 -> LED stays solid will fail
    if (bme_status != 0) {
        led_on();
        while(1) {}
    }

    // -- reached only on succcess -- 
    // It is safe to read now, the bus is known-good
    // volatile because inspected by GDB not read by code
    volatile bme280_reading_t reading;

    // -- PASS: start the heartbeat --
    tim6_init();

    // re-read forever so live values apppear in the debugger
    while (1) {
        reading = bme280_read();
        volatile float temp_c    = reading.temperature / 100.0f;   // C
        volatile float press_hpa = reading.pressure    / 25600.0f;  // hPa
        volatile float hum_rh    = reading.humidity    / 1024.0f;   // %RH
        for (volatile int i = 0; i < 400000; i++);                  // pace (a few reads/sec)
    }
}