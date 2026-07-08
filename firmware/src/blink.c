#include "stm32g474xx.h"
#include "blink.h"

void TIM6_DAC_IRQHandler(void){
    //clears the UPDATE flag
    //SR=status register, reports than an overflow has happened
    TIM6->SR &= ~TIM_SR_UIF;

    //toggle PA5
    GPIOA->ODR ^= GPIO_ODR_OD5;
}

//drives the LED ON
void led_on(void) {
    GPIOA->ODR |= GPIO_ODR_OD5;
}

void led_init(void) {
    // --- GPIOA Enable ---
    //Enable the GPIOA clock, sets the GPIOA bit high without changing other bit configurations
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;

    //Clear PA5:
    // MODER comes up nonzero at reset, so clear PA5's two bits to 00 first, can't just OR 01 onto whatever's there
    // ~ flips the MODE5 mask so we AND 0s onto PA5 (forces them low) and 1s everywhere else (leaves other pins alone)
    GPIOA->MODER &= ~GPIO_MODER_MODE5;

    // PA5 field is 00 now; OR in the low bit to make it 01 = output mode
    GPIOA->MODER |= GPIO_MODER_MODE5_0;
}

void tim6_init(void) {
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
}