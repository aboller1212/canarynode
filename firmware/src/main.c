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

    while(1) {
        GPIOA->ODR ^= GPIO_ODR_OD5;   // flip PA5: on→off or off→on each loop
        //use volatile so the computer doesn't 'optimize' and not run for loop because of empty body
        for (volatile uint32_t i = 0; i < 200000; i++);
    }
}