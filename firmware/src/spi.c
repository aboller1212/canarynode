#include "spi.h"
#include <stdint.h>

/*
SCK: PB3: Mode AF5
MISO: PB4: Mode AF5
MOSI: PB5: Mode AF5
CS: PB7: Plan GPIO Output (Software controlled not hardware)
*/
void spi_init(void) {

// GPIOB Enable
RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN; //port B pins (SCK/MISO/MOSI/CS)

// SPI Clock Enable
RCC->APB2ENR |= RCC_APB2ENR_SPI1EN; //spi peripheral (APB2)

// Pin Configuration (10=AF), (01=Output)
GPIOB->MODER &= ~(GPIO_MODER_MODE3 | GPIO_MODER_MODE4 | GPIO_MODER_MODE5 | GPIO_MODER_MODE7); // Clears MODER bits for PB 3,4,5
GPIOB->MODER |= (GPIO_MODER_MODE3_1 | GPIO_MODER_MODE4_1 | GPIO_MODER_MODE5_1 | GPIO_MODER_MODE7_0); // set bit one of each MODER field so PB3,4,5 read 10, and PB7 01
// AFR[1] is pins 8-15, AFR[0] is pins 0-7
// Field for pin N = bits[Nx4 +3: Nx4]
// PB3: [15:12] | PB4: [19:16] | PB5: [23:20] | PB7 needs no AFR
GPIOB->AFR[0] &= ~((0xFu << 12) | (0xFu << 16) | (0xFu << 20)); //Clears but fields(4 bits) for PB 3,4,5
GPIOB->AFR[0] |= ((5u << 12) | (5u << 16) | (5u << 20)); //store 5 for mode AF5 in each location

/*
 CR1 Register Config:
 SPI Mode: CPOL (Clock polarity, what SCK sits at when idle, 0=low, 1=high)
           CPHA (Clock phase, which clock edge the data is captured on, 0=first edge 1=second edge)
 Per BMI270 DS: Supports 00 or 11 for CPOL/CPHA, I choose 00
 MSTR = 1 (Master driving)
 BR = 011 (/16) to take the MCU clock (16MHz) and scale it to 1MHz
 SSM (Software Slave Management), says software handles chip-select
 SSI (Internal Slave Select), pretends the line is HIGH (fake value we hand the MCU)
*/
SPI1->CR1 = (SPI_CR1_MSTR | SPI_CR1_BR_1 | SPI_CR1_BR_0 | SPI_CR1_SSM | SPI_CR1_SSI);

/*
 DS: Data Size, how big each chunk SPI sends. 
 ^ choose 8-bits for 1 byte messages, 8 bit is value 0111 in the DS field
 RXNE: Code waits for it then grabs the byte
 FIFO: 16 bit wide queue for incoming data
 Note: by default RXNE only raises when FIFO has 16-bits(2 bytes) in it.
 ^ FRXTH fixes the case that you only have 1 byte waiting, FRXTH =1 and RXNE fires as soon as 8 bits arrive instead of 16
*/
SPI1->CR2 = (SPI_CR2_DS_0 | SPI_CR2_DS_1 | SPI_CR2_DS_2 | SPI_CR2_FRXTH);

// Chip select is active-low, low=(selected, talk to me) - high=(deselected, ignore the bus)
// need CS to sit high (sensor deselected)
GPIOB->ODR |= GPIO_ODR_OD7; // drives PB7 high

// SPI Enable
SPI1->CR1 |= SPI_CR1_SPE; //turns on the spi for the peripheral
}


