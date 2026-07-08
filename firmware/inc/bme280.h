#ifndef BME280_H
#define BME280_H //Guard

#include <stdint.h>

//public struct (main.c has to touch its fields)
//BME280 DS 4.2.3 reference code, they used signed for temperature, and unisgned for pressure and humidity
typedef struct {
    int32_t temperature; // hundredths of degC (5123 = 51.23 C)
    uint32_t pressure; // Q24.8 Pa (24 integer bits 8 fractional bits) ( /256 )
    uint32_t humidity; // Q22.10 %RH ( /1024 )
} bme280_reading_t;

//prototypes
uint8_t bme280_init(void);

bme280_reading_t bme280_read(void);

#endif 
