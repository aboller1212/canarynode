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

//t_fine = fine temperature : intermediate value that temperature compensation computes as a side effect
static int32_t t_fine;


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

/*
    Compensation formulas taken from the Bosch Data Sheet
    The substitution rules:
    Bosch's typedefs -> stdint.h names:
    BME280_S32_t -> int32_t
    BME280_U32_t -> uint32_t
    BME280_S64_t -> int64_t
    dig_XX -> calib.XX — ex. dig_T1 -> calib.T1, dig_P6 -> calib.P6, dig_H4 -> calib.H4
    adc_T / adc_P / adc_H — leave as the function parameter (the raw reading passed in)
    t_fine — leave as-is; it's your file-scope global.
    Make each function static 
*/

// Returns temperature in DegC, resolution 0.01 DegC. "5123" = 51.23 DegC.
// Sets the file-scope t_fine, used later by pressure & humidity compensation.
static int32_t bme280_compensate_T(int32_t adc_T) {
    int32_t var1, var2, T;
    var1 = ((((adc_T >> 3) - ((int32_t)calib.T1 << 1))) * ((int32_t)calib.T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)calib.T1)) * ((adc_T >> 4) - ((int32_t)calib.T1))) >> 12) * ((int32_t)calib.T3)) >> 14;
    t_fine = var1 + var2;
    T = (t_fine * 5 + 128) >> 8;
    return T;
}

// Returns pressure in Pa as unsigned Q24.8 (24 int + 8 frac bits).
// "24674867" = 24674867/256 = 96386.2 Pa = 963.862 hPa. Needs t_fine set first.
static uint32_t bme280_compensate_P(int32_t adc_P) {
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib.P6;
    var2 = var2 + ((var1 * (int64_t)calib.P5) << 17);
    var2 = var2 + (((int64_t)calib.P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib.P3) >> 8) + ((var1 * (int64_t)calib.P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib.P1) >> 33;
    if (var1 == 0) {
        return 0; // avoid exception caused by division by zero
    }
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib.P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib.P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)calib.P7) << 4);
    return (uint32_t)p;
}

// Returns humidity in %RH as unsigned Q22.10 (22 int + 10 frac bits).
// "47445" = 47445/1024 = 46.333 %RH. Needs t_fine set first.
static uint32_t bme280_compensate_H(int32_t adc_H) {
    int32_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)calib.H4) << 20) - (((int32_t)calib.H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)calib.H6)) >> 10) * (((v_x1_u32r * ((int32_t)calib.H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)calib.H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)calib.H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    return (uint32_t)(v_x1_u32r >> 12);
}

/*
    One read burst can read all 8 measurment bytes starting at address 0xF7-0xFE
    The data register map packs the bytes in big endian (MSB is at the lowest address)
    Temperature and Pressure are both 20 bits so they need 3 registers 8+8+4
    20 bit pressure: buf[0](pres[19:12]), buf[1]pres[11:4], buf[2](top nibble)(pres[3:0] in bits [7:4])
    20 bit temperature: buf[3](temp[19:12]), buf[4](temp[11:4]), buf[5](temp[3:0] in bits [7:4] top nibble)
    16 bit humidity: buf[6](hum[15:8]), buf[7](hum[7:0])
*/

bme280_reading_t bme280_read(void) {
    //declaring the buffer for the burst read
    uint8_t buf[8];
    // read all 8 registers for all of the data and store into buf[0..7]
    i2c_read_burst(0x76, 0xF7, buf, 8);
    //                MSB in [19:12]     buf[1] in [11:4]   top 4 bits of buf[2] into [3:0]
    int32_t adc_P = ( buf[0] << 12 ) | ( buf[1] << 4 ) | ( buf[2] >> 4 );
    //              buf[3] [19:12]      buf[4] [11:4]   buf[5] top bits into [3:0]
    int32_t adc_T = ( buf[3] << 12 ) | ( buf[4] << 4 ) | ( buf[5] >> 4 );
    //                  high bits       low bits  -> 16 bits not 20   
    int32_t adc_H = ( ( buf[6] << 8 ) | buf[7] );

    //NOTE: bme280_compensate_T must run first as it SETS t_fine
    bme280_reading_t result; //struct with temp, pressure, humidity
    //temp reading
    result.temperature = bme280_compensate_T(adc_T);
    //pressure reading
    result.pressure = bme280_compensate_P(adc_P);
    //humidity reading
    result.humidity = bme280_compensate_H(adc_H);

    return result;
}