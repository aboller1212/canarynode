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
        Rx FIFO 0: incoming messages land here (N slots)
        ^ Note: FDCAN has two receive inboxes FIFO0 and FIFO1, They're seperate queues in the message RAM, so you can route traffic to different inboxes 
        Tx buffers: outgoing messages I write here (M slots)
        To transmit: write message (ID+data) into a Tx buffer slot in the RAM, then poke register TXBAR telling it to send buffer
        To receive: FDCAN puts the messages into the Rx FIFO area, we read status register RXFOS to see how many are waiting and to read out of RAM
    */

    // RXGFC = RX Global Filter Configuration : controls FDCAN's global rule for what happens to incoming frames
    // ^ normally a node sets up filters, a list of IDs it cares about, either frames match a filter or don't
    // ANFS = Accept Non-matching Frames, Standard, bit 4 : no filter->00 = put it in Rx FIFO 0 (01 = FIFO 1, 10/11 = reject)
    // ^ 2 bit field in RXGFC (RMO0440). 00=accept Rx in FIFO 0, 01=accept Rx in FIFO 1, 10/11 = reject
    // LSS = List Size Standard, bit 16 : how many standard filters you've defined -> I've set 0
    
    //Sets the values in the RXGFC register for LSS and ANFS, the other fields just get their reset values
    FDCAN1->RXGFC = (0u << FDCAN_RXGFC_LSS_Pos) //there are 0 standard filters
                  | (0u << FDCAN_RXGFC_ANFS_Pos); //non-matching std frames -> Rx FIFO 0

    //Internal loopback (self-test, no pins or bus)
    FDCAN1->CCCR |= FDCAN_CCCR_TEST; // enable test mode (unlocks the TEST register)
    FDCAN1->CCCR |= FDCAN_CCCR_MON; // bus monitoring -> maked loopback INTERNAL : MON = TX loops to RX inside the chip, pins disconnected
    FDCAN1->TEST |= FDCAN_TEST_LBCK; // turn loopback on

    //Note: you can only change FDCANs config if INIT=1 but FDCAN will be stopped
    FDCAN1->CCCR &= ~FDCAN_CCCR_INIT; //leave init mode, FDCAN goes live (clears just the init bits)
    while(FDCAN1->CCCR & FDCAN_CCCR_INIT); //wait until FDCAN is running, use this because INIT may still read 1 for a moment after I write 0, exits once 0

    /*
        Loopback Test:
        1. Write a frame (an ID + some data bytes) into a Tx buffer in RAM
        2. Request send (set the buffers bit in TXBAR (Tx Buffer Add Request))
        3. Check it arrived (read Rx FIFO 0 Status, if fill level is > 0 a frame landed)
        4. Read it out of FIFO 0 and confirm it matches what I sent
    */

    //Tx buffer lives in RAM at SRAMCAN_BASE + 0x278 : this is where we write messages into this peice of memory
    //FDCAN1 Tx buffer 0 in message RAM, volatile because it can change, points to 32-bit words
    volatile uint32_t *txbuf = (volatile uint32_t *)(SRAMCAN_BASE + 0x278);

    
    //NOTE: FDCAN HARDWARE will read the 1st word as ID, 2nd word as DLC, 3+ word as data
    //The ID is in the 0 position of txbuf
    //we left-align the bits because ID slot is 29 bits, our ID field is 11-bits -> insert at bit 18, flags are 29-31(untouched)
    txbuf[0] = (0x123u << 18); //send a message with ID 0x123
    
    //the length = 8 data bytes
    txbuf[1] = (8u << 16); //placed at the 16th bit

    //the data slots, DLC tells hardware HOW FAR TO READ, DLC = 8 = 2 bytes, reads txbuf[2,3]
    txbuf[2] = 0xDEADBEEF; //data bytes 0-3 (4 bytes per word = 32-bit)
    txbuf[3] = 0x12345678; //data bytes 4-7 

    
    //Each bit = one Tx buffer, bit0 = buffer0
    //set only register - after writing 1, the bit auto-clears
    FDCAN1->TXBAR = (1u << 0); //reguest transmission of TX buffer 0

    //confirm the frame arrived
    //RXF0S = Rx FIFO0 status register -> Field F0FL = fill level = how many messages sitting in FIFO0 right now
    while(!(FDCAN1->RXF0S & FDCAN_RXF0S_F0FL_Msk)); //wait until a message comes, F0FL field will be greater than 0

    //Once we know that there IS a message waiting in the buffer we need to knw which of FIFO0s 3 slot hold it
    //the F0GI slot = Get Index, which FIF0 slot 0,1,2 holds the oldest unread message
    // when we do >>F0GI_Po = (>> 8) we are shifting it down to where the F0GI exists which is bit 8
    // shifting it right by 8 turns it into either 0,1,2
    uint32_t getidx = (FDCAN1->RXF0S & FDCAN_RXF0S_F0GI_Msk) >> FDCAN_RXF0S_F0GI_Pos;

    //now we need to compute the slot's address in message RAM
    // 0xB0: Rx FIFO 0s base offset in message RAM
    //getidx * 0x48 = skip to the right slot, each slot is 0x48 so getidx * 0x48 would get you to the right slot
    volatile uint32_t *rxbuf = (volatile uint32_t *)(SRAM_BASE + 0xB0 + getidx * 0x48);
    //R0 = ID word, R1= DLC + Length, R2,3 = data -> same packing as Tx

    //we shift the ID field down out of [28:18], then mask 11-bits 0x7FF = 11 ones, to drop the flags
    uint32_t id = (rxbuf[0] >> 18) & 0x7FFu; 

    //Shift the dlc down from [19:16], mask 4-bits 0xF
    uint32_t dlc = (rxbuf[1] >> 16) & 0xFu; 

    //the data we sent
    uint32_t d0 = rxbuf[2]; //data bytes 0-3
    uint32_t d1 = rxbuf[3]; //data bytes 4-7

    //RXF0A = Rx FIFO 0 Acknowledghe register at FDCAN1 + 0x094
    //Field F0AI: Acknowledge index, write the slot index you just read, FDCAN advances read and frees the slot
    //no need to shift because F0AI is at [2:0] so already at the bottom
    FDCAN1->RXF0A = getidx; //acknowledge and free the slot, F0FL will drop by 1


    return 0;
}

void TIM6_DAC_IRQHandler(void){
    //clears the UPDATE flag
    //SR=status register, reports than an overflow has happened
    TIM6->SR &= ~TIM_SR_UIF;

    //toggle PA5
    GPIOA->ODR ^= GPIO_ODR_OD5;
}
