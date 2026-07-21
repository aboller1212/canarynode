#ifndef SPI_H
#define SPI_H

#include <stdint.h>

//prototypes
void spi_init(void);
uint8_t spi_transfer(uint8_t tx); // primitive send one byte, get one byte
uint8_t spi_read_reg(uint8_t reg); // read one register
void spi_write_reg(uint8_t reg, uint8_t val); // write one register
void spi_read_burst(uint8_t reg, uint8_t *buf, uint16_t n); //read n consecutive 
void spi_write_burst(uint8_t reg, const uint8_t *buf, uint16_t n); //write n (BMI270 config upload)

#endif 