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

    /*
        After the config load, accel and gyro are powered off
        PWR_CTRL: Register 0x7D, turns on accel and gyro: bit1=gyr_en, bit2=acc_en
        ^ 0000 0110 = 0x06, write to reg 0x7D
    */  
    spi_write_reg(0x7D, 0x06);

    /*
        ACC_CONF (reg 0x40): 
        acc_odr[3:0]: samples per second, table provided-> 0x08=100Hz 
        acc_bwp[6:4]: filtering/averaging 0x02 = norm_avg4 (normal)
        acc_filter_perf[7]: ulp vs hp -> use hp(performance optimized)(0x01)
        so we get:  1 010 1000 -> 0xA8
    */
    spi_write_reg(0x40, 0xA8);

    /*
        GYR_CONF (reg 0x42):
        gyr_odr[3:0]: clock speed. 100Hz = 0x08
        gyr_bwp[5:4]: 0x02 = normal mode
        gyr_noise_perf[6]: performance optimized: 0x01
        gyr_filtre_perf[7]: performance optimized: 0x01
        final:   1 1 10 1000 -> 1110 1000 -> 0xE8
    */
    spi_write_reg(0x42, 0xE8);

    /* 
        ACC_RANGE(reg 0x41)
        +/- 4g, gravity being 1g, so 2g headroom is tight, use 4g 
        acc_range[1:0]: 0x01 -> range 4g
    */
    spi_write_reg(0x41, 0x01);

    /*
        GYR_RANGE(reg 0x43)
        +/- 2000 deg/s
        gyr_range[2:0]: 0x00 = 2000 deg/s
        ois_range[3]: for stabilization (leave off) 0x00
    */
    spi_write_reg(0x43, 0x00);

    return 0;
}

bmi270_reading_t bmi270_read(void) {
    //6 readings, 2 registers per reading (12), 8 bits per register
    uint8_t buf[12];

    //store results into buf
    spi_read_burst(0x0C, buf, 12);

    //Unpacking little endian style
    int16_t acc_x = (buf[1] << 8) | buf[0];
    int16_t acc_y = (buf[3] << 8) | buf[2];
    int16_t acc_z = (buf[5] << 8) | buf[4];

    int16_t gyr_x = (buf[7] << 8) | buf[6];
    int16_t gyr_y = (buf[9] << 8) | buf[8];
    int16_t gyr_z = (buf[11] << 8) | buf[10];

    // combining the results to return
    bmi270_reading_t result;

    result.acc_x = acc_x;
    result.acc_y = acc_y;
    result.acc_z = acc_z;
    result.gyr_x = gyr_x;
    result.gyr_y = gyr_y;
    result.gyr_z = gyr_z;

    return result;
}