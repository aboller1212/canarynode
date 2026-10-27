//looks in the vendor/cmsis from CMakeLists
#include <stdint.h>
#include "bme280.h"
#include "i2c.h"
#include "fdcan.h"
#include "blink.h"
#include "spi.h"
#include "bmi270.h"

int main(void) {

    // -- Peripheral bring-up (buses + LED pin) --
    led_init(); //PB0 -> output, LED off
    i2c_init(); //i2c1 master on PA15/PB7 must come before sensor access
    // fdcan_init(); //FDCAN1 @500k, internal loopback (not ported to FDCAN2 yet)

    for (volatile int i = 0; i < 200000; i++);   // ~60ms, let the BMI270 finish POR
    spi_init(); //spi en

    // // --- FDCAN Test ---
    // // Send a frame, in loopback it comes straight bazck to our own RX FIFO
    // //test for Tx, id=0x123, d0,     d1
    // fdcan_send(0x123, 0xDEADBEEF, 0x12345678);
    // //initialize varibles to write into
    // //volatile because main never reads these, they exist for the debugger
    // volatile uint32_t id, dlc, d0, d1;
    // //send the locations of each variable
    // fdcan_recv(&id, &dlc, &d0, &d1);

    // -- I2C + BME280 Smoke Test --
    // bme280_init() reads chip ID 0xD0: returns 0 if it == 0x60, else 1
    uint8_t bme_status = bme280_init();

    // -- SPI + BMI270 Smoke Test --
    // bmi270_init() checks chip ID 0x24, uploads the 8KB config, checks init_ok: returns 0 on success
    uint8_t bmi_status = bmi270_init();

    // Fail Trap:
    // On failure: light the LED solid and halt, before TIM6 ever starts
    // Because timer never runs the ISR never fires, nothing toggles
    // PB0 -> LED stays solid will fail
    if (bme_status != 0 || bmi_status != 0) {
        led_on();
        while(1) {}
    }

    // -- reached only on succcess --
    // It is safe to read now, the bus is known-good
    // volatile because inspected by GDB not read by code
    volatile bme280_reading_t reading;

    volatile bmi270_reading_t acc_gyr_result;

    // -- PASS: start the heartbeat --
    tim6_init();

    // re-read forever so live values apppear in the debugger
    while (1) {
        reading = bme280_read();
        acc_gyr_result = bmi270_read();
        volatile float temp_c    = reading.temperature / 100.0f;   // C
        volatile float press_hpa = reading.pressure    / 25600.0f;  // hPa
        volatile float hum_rh    = reading.humidity    / 1024.0f;   // %RH
        for (volatile int i = 0; i < 400000; i++);                  // pace (a few reads/sec)
     }



}
