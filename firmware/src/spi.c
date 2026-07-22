#include "spi.h"
#include <stdint.h>
#include "stm32g474xx.h"

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

/*
    SPI hardware only knows how to 'swap' a byte: when the spi peripheral is handed one byte
    it clocks 8 bits out on MOSI and 8 bits in on MISO at the same time. 
    Drop byte into data register-> wait for hardware to say sent-> wait for it to say a byte came back-> read recevied byte
*/
uint8_t spi_transfer(uint8_t tx) { //tx is the byte sent, returns the received byte
    //TXE=Transmit Buffer Empty (send buffer is free)
    while(!(SPI1->SR & SPI_SR_TXE)); //spin until TXE is set
    
    // writing to Data Register (DR), must write tx
    // &SPI1->DR: address of DR
    // (volatile uint8_t *): treat addres as a pointer to a single byte (this forces the 16 bit read to 8)
    // *(...): write to the byte at that address 
    *(volatile uint8_t *)&SPI1->DR = tx; //8-bit write

    //RXNE: Receive buffer not empty (byte recieved)
    while(!(SPI1->SR & SPI_SR_RXNE)); // wait until a byte is ready to read

    // taking address of DR and relabeling its type to a pointer to a 8-bit value
    // derefencing the byte pointer, the compiler reads the 8-bits at that address
    uint8_t rx = *(volatile uint8_t *)&SPI1->DR;

    return rx;
}

/*
    BMI 270 specific: we need to send 3 transfers:
    1. MOSI(reg | 0x80), tells it to read this register, (MISO) sensor returns garbage
    2. MOSI(dummy(0x00)), just to make clock tick (full-duplex), (MISO) sensor returns the dummy byte
    3. MOSI(dummt(0x00)), makes the clock tick again, (MISO) sensor returns register value (KEEP)
*/
uint8_t spi_read_reg(uint8_t reg) { // read one register
    //Bit 7 of the address byte = R/W flag. bit7=1: read, bit7=0: write
    //ex: read register 0x00, set bit 7 to 1 -> 0x00 | 0x80 = 0x80, or with the 7 bit set (0x80)

    // Step 1: Drive CS(PB7) low
    GPIOB->ODR &= ~(GPIO_ODR_OD7);

    // Step 2: send address, ignore return, OR with bit7=1 for read
    spi_transfer(reg | 0x80); 

    // Step 3: dummy transfer, bmi270 specific
    spi_transfer(0x00);

    // Step 4: send another dummy, keep result
    uint8_t value = spi_transfer(0x00);

    // Step 5: CS high
    GPIOB->ODR |= GPIO_ODR_OD7;

    return value;
}

void spi_write_reg(uint8_t reg, uint8_t val) {
    //Step 1: Pull CS low
    GPIOB->ODR &= ~(GPIO_ODR_OD7);
    
    // Step 2: send address to write at
    spi_transfer(reg);

    // Step 3: send value to write
    spi_transfer(val);

    //Step 4: Pull CS high
    GPIOB->ODR |= GPIO_ODR_OD7;
}

void spi_read_burst(uint8_t reg, uint8_t *buf, uint16_t n) {
    //Step 1: Pull CS low
    GPIOB->ODR &= ~(GPIO_ODR_OD7);

    // Step 2: send address, ignore return, OR with bit7=1 for read
    spi_transfer(reg | 0x80); 

    // Step 3: dummy transfer, bmi270 specific (Only have to discard one time)
    spi_transfer(0x00);

    // Step 4: read each transfer into the buffer
    for (int i = 0; i < n; i++) {
        buf[i] = spi_transfer(0x00); //sesnor auto incrememnts wihtin the register from the initial transfer
    }

    //Step 5: CS high
    GPIOB->ODR |= GPIO_ODR_OD7;
}

void spi_write_burst(uint8_t reg, const uint8_t *buf, uint16_t n) {
    //Step 1: Pull CS low
    GPIOB->ODR &= ~(GPIO_ODR_OD7);
    
    // Step 2: send address to write at, bit7=0 so sensor knows write
    spi_transfer(reg);

    //Step 3: Write to the register multiple increments
    for (int i = 0; i < n; i++) {
        spi_transfer(buf[i]); //writes each consecutive item in the buffer
    }

    //Step 4: CS high
    GPIOB->ODR |= GPIO_ODR_OD7;
}