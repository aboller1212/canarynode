#include "bme280.h"
#include "i2c.h"
//no #include stm... because bme280 only accesses registers through i2c helper

/*
    T/P/H are the calibration coefficients, per-chip constants
    The Letter represents which function they are apart of -> Temp, Humidity, or Pressure
    The TYPE matters for the formula calculations to be made to reveal data
*/
typedef struct {
    uint16_t T1, P1;
    int16_t T2, T3, P2, P3, P4, P5, P6, P7, P8, P9, H2, H4, H5;
    uint8_t H1, H3;
    int8_t H6;
} bme280_calib_t;

//a variable declared inside bme280_init() dies when that function returns, but bme280_read() will need these coefficients on every measurement so we declare it static(lives in RAM the entire run)
static bme280_calib_t calib;


//0x76 is the i2c bus, the ID (reg) passes through is the specific device passed, 0x60 is the value in the reg(0xD0)
//returns 0 = success and nonzero if failure - IMPORTANT to return a status to know if functional
uint8_t bme280_init(void) {

    // ds -> i2c address is 0x76 because we tied SDO->GND
    // ds -> register(located at 0x76) -> 0xD0(id) reads 0x60 for a BME280
    uint8_t whoami = i2c_read_reg(0x76, 0xD0);
    if(whoami != 0x60) return 1; // return 1 if fail

    // reset:
    i2c_write_reg(0x76, 0xE0, 0xB6); //ds -> if you write 0xB6 into reg 0xE0, the device resets
    //after reset we must WAIT for im_update to complete (bit 0 of status register 0xF3) | complete = 0 means done
    while(i2c_read_reg(0x76, 0xF3) & 0x01); // spin while im_update is set, spin while im_update=1 (in progress) checks only the 0 bit (0x01)
    
    /*
        BME has a flat register map, calibration blocks are laid out into 2 sections within the register map
        Block 1: 0x88-0xA1 (dig_T1..T3, P1..P9, H1) -> 26 Bytes
        Block 2: 0xE1-0xE7 (dig_H2..H6) -> 7 bytes
        This will be parsed in 2 burst reads. Note each of the byte is +1 (6+1 & 25+1) to read through all registers in the block
    */
    // arrays for each block
    uint8_t calib1[26];
    uint8_t calib2[7]; 

    //same addr(bme280), respective starting addr, resective buffers, sizes
    i2c_read_burst(0x76, 0x88, calib1, 26);
    i2c_read_burst(0x76, 0xE1, calib2, 7);

    /*
        little-endian: the LSB comes first(lower address)
        ex: calib1[0] = 0x88: T1 low byte
            calib1[1] = 0x89: T1 high byte
        To shift: value = (high << 8) | low
        C will promote uint8_t -> int (32 bit) so we need to cast the SIGNED calibrations
        Note: register map 4.2.2 BME280 has the map and array index
    */

    // Temperature unpacking
    calib.T1 = (calib1[1] << 8) | calib1[0];
    calib.T2 = (int16_t)((calib1[3] << 8) | calib1[2]);
    calib.T3 = (int16_t)((calib1[5] << 8) | calib1[4]);
    
    // Pressure unpacking
    calib.P1 = (calib1[7] << 8) | calib1[6];
    calib.P2 = (int16_t)((calib1[9] << 8) | calib1[8]);
    calib.P3 = (int16_t)((calib1[11] << 8) | calib1[10]);
    calib.P4 = (int16_t)((calib1[13] << 8) | calib1[12]);
    calib.P5 = (int16_t)((calib1[15] << 8) | calib1[14]);
    calib.P6 = (int16_t)((calib1[17] << 8) | calib1[16]);
    calib.P7 = (int16_t)((calib1[19] << 8) | calib1[18]);
    calib.P8 = (int16_t)((calib1[21] << 8) | calib1[20]);
    calib.P9 = (int16_t)((calib1[23] << 8) | calib1[22]);

    // Humidity unpacking
    //Note: H1 bit is in calib1[25] addr(0xA1), 0xA0(calib1[24] is reserved)
    calib.H1 = calib1[25];
    calib.H2 = (int16_t)((calib2[1] << 8) | calib2[0]);
    calib.H3 = calib2[2];
    // register 0xE7 = calib2[6]
    calib.H6 = (int8_t)calib2[6];
    //Note: H4 and H5 are each 12-bit signed values, they share one byte ( reg 0xE5 )
    /*
        Table 16: 
        dig_H4 (12 bits):  0xE4 = bits [11:4]  +  0xE5 low nibble  = bits [3:0]
        dig_H5 (12 bits):  0xE6 = bits [11:4]  +  0xE5 high nibble = bits [3:0]
    */
    // H4: 8 bits from 0xE4 placed in [11:4] plus low nibble of 0xE5 in [3:0]
    // calib2[3] << 4 : the top 8 bits(0xE4) shifted to [11:4]
    // calib2[4] & 0x0F : mask calib2[4](0xE5) so it only has bits [3:0]
    calib.H4 = (int16_t)((calib2[3] << 4) | (calib2[4] & 0x0F));
    
    //H5: 8 high bits from 0xE6 = calib2[5] in [11:4], plus the high nibble of 0xE5 = calib2[4]
    // calib2[5] << 4 : top 8 bits placed in [11:4]
    // calib2[4] >> 4 : high nibble shifted to [3:0]
    calib.H5 = (int16_t)((calib2[5] << 4) | (calib2[4] >> 4));

    /*
        Oversampling: sensor takes N samples per reading and averages them to cut noise
        Mode: 00 = sleep, 01/10 = forced (one-shot then sleep), 11 = normal (i will use this)
        ctrl_hum(0xF2) : humidity oversampling
        ctrl_meas(0xF4) : temp + pressure oversampling + mode
        per ds -> "changes to ctrl_hum only take effect after a subsequent write to ctrl_meas"
        ^ ctrl_hum is needed FIRST
    */

    //ctrl_hum write: we use 0x01 as the value of field osrs_h = 001 in ds
    i2c_write_reg(0x76, 0xF2, 0x01);

    //ctrl_meas write: we use fields from ds -> 001 001 11 = 0010 0111 = 0x27
    i2c_write_reg(0x76, 0xF4, 0x27);

    return 0; //success
}