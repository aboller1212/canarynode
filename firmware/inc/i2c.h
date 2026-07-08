#ifndef I2C_H
#define I2C_H // prevents the text of i2c.h from being pasted into main.c twice at compile time

//stdint.h will land in main.c indirectly, so main.c can now use uint8_t... etc even though it never included stdint.h
#include <stdint.h>

//prototypes
void i2c_init(void);
void i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val);
uint8_t i2c_read_reg(uint8_t addr, uint8_t reg);
void i2c_read_burst(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n);

#endif