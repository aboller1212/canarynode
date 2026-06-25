//looks in the vendor/cmsis from CMakeLists
#include "stm32g474xx.h" 

int main(void) {
    //Enable the GPIOA clock, sets the GPIOA bit high without changing other bit configurations
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    //Clear PA5:
    // MODER comes up nonzero at reset, so clear PA5's two bits to 00 first, can't just OR 01 onto whatever's there
    // ~ flips the MODE5 mask so we AND 0s onto PA5 (forces them low) and 1s everywhere else (leaves other pins alone)
    GPIOA->MODER &= ~GPIO_MODER_MODE5;

    // PA5 field is 00 now; OR in the low bit to make it 01 = output mode
    GPIOA->MODER |= GPIO_MODER_MODE5_0;

    /*  
        TIM6 fired the interupt every 500ms based on ARR and PSC and input_clock desired (0.5s toggle)
        NVIC enabled for TIM6 so in startup its routed in hardware
        calls TIM6_DAC_IRQHandler() each interrupt
    */

    //RCC is clock enable, APB1 (bus)
    //CMSIS header -> TIM6(simple timer)
    RCC->APB1ENR1 |= RCC_APB1ENR1_TIM6EN;

    //prescaler: 16MHz -> 1kHz counter (1ms per tick)
    TIM6->PSC = 15999;

    //auto reload: count 500 ticks-> 0.5s = T | note the 500-1 being for no 0 setting on this / more range per bit
    TIM6->ARR = 499;

    //Update Interrupt Enable: fire an IRQ on each overflow (ARR reached)
    //DIER = DMA/Interrupt Enable Register - decides which timer events are allows to raise interrupt requests
    //UIE = Update Interrupt Enable (bit0)
    TIM6->DIER |= TIM_DIER_UIE;

    //Lets TIM6 intterupt to actually reach the CPU
    NVIC_EnableIRQ(TIM6_DAC_IRQn);

    //CR1: TIM6 main control register
    //start TIM6 counting
    TIM6->CR1 |= TIM_CR1_CEN;


    //enables the register access clock on APB1 (like every peripherhal so we can read/write its perihpherals)
    RCC->APB1ENR1 |= RCC_APB1ENR1_FDCANEN;

    //enables the kernel clock that drives the CAN bit timing
    //we need FDCANSEL -> 10 = PCLK1(clock delivered to perhipherals on the APB1 bus)
    RCC->CCIPR &= ~RCC_CCIPR_FDCANSEL; //clears the 2 bits to 0
    RCC->CCIPR |= RCC_CCIPR_FDCANSEL_1; //sets to 10 = PCLK1 (16MHz Clock)

    /*
        PCLK1: 16MHz peripheral clock on APB1 -> set the kernel to use this clock
        bit rate: bits per second on the bus: I use 500 kbit/s as a standard CAN rate
        Tq: (time quantum) the smallest time slice of a bit | 16MHz / 500 kbit | 1 bit = 32 Tq
        -> prescaler: divides the kernel clock to set how long one Tq is
        -> prescaler = 1 | Tq = prescaler / kernel_clock = 1/16Hz = 62.5ns = 1 Tq
        SYNC: the fixed 1 Tq slot at the start of each bit (node clock realignment)
        Tseg1: The chunk of Tq from SYNC up to the sample point
        Tseg2: The chunk of Tq from the sample point to the bits end
        -> Want to read at ~80% for the sample point (don't want to take too early)
        -> SYNC(1) + Tseg1 + Tseg2 = 32 | Tseg1 + Tseg2 = 31
        -> (1 + Tseg1) / 32 = 0.80 | Tseg1 = 25 -> Tseg2 = 6 -> 26/32 = 81.3%
    */

    //CCCR: FDCANs main control register
    //FDCAN_CCCR_INIT = Initialization bit
    //FDCAN_CCCR_CCE = Configuration Change Enable
    FDCAN1->CCCR |= FDCAN_CCCR_INIT; //request init mode
    
    //Blocking-busy wait: fine for one-time setup
    while(!(FDCAN1->CCCR & FDCAN_CCCR_INIT)); // & checks only the init bit in the CCCR register, loops until its set

    FDCAN1->CCCR |= FDCAN_CCCR_CCE; // config change enable, unlocks NBTP

    // (=) we are writing the whole register : (|) packs all four into one 32-bit word
    //from RMOS: bit time = (1 + (NTSEG1+1) + (NTSEG2+1)) × (NBRP+1) so thats why we subtract 1 going in
    //each xu is unsigned number x being stored in their respective areas for the NBTP register
    FDCAN1->NBTP = (0u << FDCAN_NBTP_NSJW_Pos) //SJW = 1 -> 0
                  | (0u << FDCAN_NBTP_NBRP_Pos) //prescaler = 1 -> 0
                  | (24u << FDCAN_NBTP_NTSEG1_Pos) //Tseg1 = 25 -> 24
                  | (5u << FDCAN_NBTP_NTSEG2_Pos); //Tseg2 = 6 -> 5

    /*
        CAN needs a dedicated spot in RAM because CAN-FD messages can be 64-bits of data
        this is a lot so FDCAN will put the data in a small RAM areas and I congfig registers to divide the RAM
        Filter list: which incoming IDs do I accept?
        Rx FIFO 0: incoing messages land here (N slots)
        Tx buffers: outgoing messages I write here (M slots)
        To transmit: write message (ID+data) into a Tx buffer slot in the RAM, then poke register TXBAR telling it to send buffer
        To receive: FDCAN puts the messages into the Rx FIFO area, we read status register RXFOS to see how many are waiting and to read out of RAM
    */

    

    //NOTE; even though code is held here the hardware still calls the function
    while(1) {
        //Nothing - was in place for old-polling blink
    }
}

void TIM6_DAC_IRQHandler(void){
    //clears the UPDATE flag
    //SR=status register, reports than an overflow has happened
    TIM6->SR &= ~TIM_SR_UIF;

    //toggle PA5
    GPIOA->ODR ^= GPIO_ODR_OD5;
}
