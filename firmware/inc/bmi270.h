#ifndef BMI270_H
#define BMI270_H

#include <stdint.h>

//prototypes
uint8_t bmi270_init(void); // returns 0 = success, nonzero = fail

/*
    Need to return a struct with 6 readings, ACC x/y/z & GYR x/y/z
    Little endian stored: LSB sits at lower address, reassemly (MSB << 8) | LSB
    Need to use signed data types -> 16bits per reading
*/
typedef struct {
    int16_t acc_x;
    int16_t acc_y;
    int16_t acc_z;

    int16_t gyr_x;
    int16_t gyr_y;
    int16_t gyr_z;
} bmi270_reading_t;

bmi270_reading_t bmi270_read(void);




#endif