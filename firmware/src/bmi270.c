#include "bmi270.h"
#include "spi.h"

// tells compiler the array exists and is defined elsewhere
extern const uint8_t bmi270_config_file[];

/*
    The BMI270 powers up in I2C by default: switch to SPI by triggering CSB activity
    i.e. do one throwaway read of any register at the start, this pulls CSB low
*/
uint8_t bmi270_init(void) {
    spi_read_reg(0x00); // Dummy read to toggle CSB (i2c->spi)
    uint8_t whoami = spi_read_reg(0x00); // Once the chip is in SPI, check register 0x00 for the chip ID (expect 0x24)
    if(whoami != 0x24) return 1; // check chip ID is correct before initializing

    /*
        Reg 0x7E = CMD (command register) | Value 0xB6 = soft-reset command
        Per DS -> wait 2ms after issuing it
    */
    spi_write_reg(0x7E, 0xB6); //writes 0xB6 to register 0x7E
    for (volatile int i = 0; i < 30000; i++); //roughly 9-13ms at 16MHz

    spi_read_reg(0x00); // Dummy read, reset put chip back into i2c mode

    /*
        BMI270s feature engine is empty at power-up, must upload an 8KB configuration blob
        Sequence per DS: 
        1. Disabled advanced power save (write 0x00 to PWR_CONF(0x7C), wait 450 us)
        2. Prep the load (write 0x00 to INIT_CTRL(0x59); tells the chip config upload starting)
        3. Upload the blob (spi_write_burst the 8KB array into INIT-DATA(0x5E))
        4. Finish the load (write 0x01 to INIT_CTRL(0x59); upload done)
        5. Verify; wait 20ms, read INTERNAL_STATUS (0x21) and check bit0 =1 (init_ok)
    */
    spi_write_reg(0x7C, 0x00); //disable power save
    for(volatile int i = 0; i < 2000; i++); //wait
    spi_write_reg(0x59, 0x00); //prep load
    spi_write_burst(0x5E, bmi270_config_file, 8192); // write the whole config array
    spi_write_reg(0x59, 0x01); //finish the load, upload done
    for(volatile int i =0; i < 70000; i++); // wait 20ms
    uint8_t status = spi_read_reg(0x21);
    if((status & 0x0F) != 0x01) return 1; // checks the internal status is 0000 0001



    return 0;
}