#include "i2c.h"
#include "stm32g474xx.h"

void i2c_init(void) {
    // --- GPIOB Enable ---
    //Enable the GPIOB clock, sets the GPIOB bit high without changing other bit configurations
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOAEN;

    // --- I2C1 clock enable ---
    RCC->APB1ENR1 |= RCC_APB1ENR1_I2C1EN;
    //Kernel Clock Defaults to 00=PCLK1, no line needed

    // --- Pin Configuration ---
    // CHANGE WITH CUSTOM
    // AF corresponds to 10
    //SCL PIN
    GPIOA->MODER &= ~(GPIO_MODER_MODE15); //clears the MODER bits for PB 8,9
    GPIOA->MODER |= (GPIO_MODER_MODE15_1); //sets both PA15 to AF

    //SDA PIN
    GPIOB->MODER &= ~(GPIO_MODER_MODE7);
    GPIOB->MODER |= (GPIO_MODER_MODE7_1); //PB7 for SCL 
    // PB 7 -> AFR[0], PA15 ->AFR[1], remember formula (N-8) * 4 for bit positions
    GPIOA->AFR[1] &= ~((0xFu << 28)); // clears the 4-bit AFR fields
    GPIOA->AFR[1] |= ((4u << 28)); // sets the value to 4 in each location

    GPIOB->AFR[0] &= ~((0xFu << 28)); // clears the 4-bit AFR fields
    GPIOB->AFR[0] |= ((4u << 28));

    //OTYPER = GPIO Output Type Register
    GPIOA->OTYPER |= (GPIO_OTYPER_OT15); //this makes both PA15, PB7 open drain which is neccesary for i2c

    GPIOB->OTYPER |= (GPIO_OTYPER_OT7);

    // --- Bus Speed (TIMINGR) ---
    //locate in RMO kernel speed = 16MHz, target is 100kHz - values then plugged into i2c calculator
    I2C1->TIMINGR = 0x30420F13; //Calculator did this

    // --- Peripheral Enable --- 
    //note that PE = 1 means peripheral enabled
    I2C1->CR1 |= I2C_CR1_PE; //enables peripheral
}

//i2c everything moves 8 bits at a time
//addr-> whatever device your talking to, reg-> which register you write into, val-> value written
void i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {

    /*
        SADD: slave address sits at [7:1] -> this is why I shift addr by one bit
        RD_WRN: 0=write (leave unset) - at bit 0 this is the R/W bit
        NBYTES: [23:16] -> how much data to transfer (2 for mine: reg byte + val byte)
        AUTOEND: auto generate stop at NBYTES
        START: launches the transaction
    */
    I2C1->CR2 = (addr << 1) | (2u << I2C_CR2_NBYTES_Pos) | (I2C_CR2_AUTOEND) | (I2C_CR2_START);

    /*
        TXIS: Transmit Interrupt Status, tells when empty and ready to receive a new byte
        TXDR: Transmit Data Register, write the byte we want to send (clears TXIS) 
    */
    // waits for the TXIS bit in ISR - ready for a byte
    // packs the value to be sent into TXDR
    while (!(I2C1->ISR & I2C_ISR_TXIS)); //checks the TXIS bit on ISR
    I2C1->TXDR = reg;
    while (!(I2C1->ISR & I2C_ISR_TXIS)); 
    I2C1->TXDR = val;

    /*
        STOPF: ISR(status), hardware sets it when STOP is done
        STOPCF: ICR(clear), write it to clear STOPF
    */
    while(!(I2C1->ISR & I2C_ISR_STOPF)); //read STOPF in ISR - wait until its done
    I2C1->ICR = I2C_ICR_STOPCF; //write 1 to clear register
}

uint8_t i2c_read_reg(uint8_t addr, uint8_t reg) {
    //READ FLOW: [addr+W] | reg | RESTART | [addr+R] | [data] | STOP

    //took out AUTOEND because we want a repeated START (holding the bus)
    //1u because only one byte (reg address)
    I2C1->CR2 = (addr << 1) | (1u << I2C_CR2_NBYTES_Pos) | (I2C_CR2_START);

    while(!(I2C1->ISR & I2C_ISR_TXIS)); //wait for the TXIS bit
    I2C1->TXDR = reg; 

    //TC: Transfer Complete, flag in the ISR
    //Since we left AUTOEND=0, once NYBTES are sent, the hardware sets TC and waits
    while(!(I2C1->ISR & I2C_ISR_TC)); //wait for TC 

    //set RD_WRN to 1 to read, we reqrite CR2 to read the register we have just written into TXDR
    I2C1->CR2 = (addr << 1) | (1u << I2C_CR2_NBYTES_Pos) | (I2C_CR2_START) 
                            | (I2C_CR2_AUTOEND) | (I2C_CR2_RD_WRN);

    /*
        RXNE (ISR) : Receive Data register Not Empty -> if set then there is a receivefd byte in RXDR ready to grab
        RXDR: (I2C1) Receive Data Register -> reading it gives you the byte and clears RXNE
    */
    while(!(I2C1->ISR & I2C_ISR_RXNE)); //make sure there is something to read 

    uint8_t val = I2C1->RXDR; // write the RXDR into val

    //AUTOEND is on so we need to clear the STOPF bit before returning
    while(!(I2C1->ISR & I2C_ISR_STOPF)); //read STOPF in ISR - wait until its done
    I2C1->ICR = I2C_ICR_STOPCF; //write 1 to clear register

    return val;
}

// data is stored in conitguous register blocks
// buf is a pointer to a caller provided array where the bytes get stored.
// n is the number of bytes
// reads N consecutive bytes starting at reg into a buffer
void i2c_read_burst(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n) {
    //Phase 1: point at register, same as read 
    I2C1->CR2 = (addr << 1) | (1u << I2C_CR2_NBYTES_Pos) | (I2C_CR2_START);

    while(!(I2C1->ISR & I2C_ISR_TXIS)); //wait for the TXIS bit
    I2C1->TXDR = reg; 

    while(!(I2C1->ISR & I2C_ISR_TC)); //wait for TC, No AUTOEND

    //Phase 2: store n in NBYTES, loops through each byte an assigns it (n bytes)
    I2C1->CR2 = (addr << 1) | (n << I2C_CR2_NBYTES_Pos) | (I2C_CR2_START) 
                            | (I2C_CR2_AUTOEND) | (I2C_CR2_RD_WRN);


    //loop through 
    for(int i = 0; i < n; i++) {
        while(!(I2C1->ISR & I2C_ISR_RXNE));
        buf[i] = I2C1->RXDR;
    }

    //Same STOP from read, AUTOEND on
    while(!(I2C1->ISR & I2C_ISR_STOPF)); //read STOPF in ISR - wait until its done
    I2C1->ICR = I2C_ICR_STOPCF;
}