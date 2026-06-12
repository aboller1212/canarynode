//Part 1: include and extern declarations
#include <stdint.h>

//linker defined variables, extern: declare a name without defining it
extern uint32_t _sdata[]; //start of .data, defined in linker inside SECTIONS
extern uint32_t _edata[]; //end of .data section in RAM
extern uint32_t _sidata[]; //load address of .data section in flash, defined via LOADADDR(.data)
extern uint32_t _sbss[]; //start of .bss in RAM. defined in the .bss SECTIONS in linker
extern uint32_t _ebss[]; //end of .bss in RAM
extern uint32_t _estack[]; //top of RAM, defined at botto of linker

//Part 2: Forward declarations

//reset handler function
void Reset_Handler(void);

//default handler used until we write out other handlers
void Default_Handler(void);

//main function, must be called before called in src/main.c 
int main(void);

//Part 3: Vector table

//Tells compiler 'put this variable or function in th names output section
//forces vector table into .isr_vector
//name in attribute and one in linker must match
//const keeps the table in flash
//vector_table is an array of function(const) pointers
//C decays each Handler function to a pointer to a function pointer
__attribute__((section(".isr_vector")))
void (* const vector_table[])(void) = {
    (void (*)(void))_estack,    // 0x00: initial SP
    Reset_Handler,              // 0x04: reset
    Default_Handler,            // 0x08: NMI
    Default_Handler,            // 0x0C: HardFault
    Default_Handler,            // 0x10: MemManage
    Default_Handler,            // 0x14: BusFault
    Default_Handler,            // 0x18: UsageFault
    0, 0, 0, 0,                 // 0x1C-0x28: reserved
    Default_Handler,            // 0x2C: SVCall
    Default_Handler,            // 0x30: DebugMon
    0,                          // 0x34 reserved
    Default_Handler,            // 0x38: PendSV
    Default_Handler,            // 0x3C: SysTick
    // Peripheral interrupts (0x40+) go here later
};

//Runs first on power-on. sets up C runtime, then calls main
// noreturn = compiler knows this never returns
__attribute__((noreturn))
void Reset_Handler(void)
{
    //copy .data from flash to RAM
    uint32_t *src = _sidata; //flash source: init values stored here
    uint32_t *dst = _sdata;  //RAM destination
    while (dst < _edata) {    //walk until end of .data in ram
        *dst++ = *src++;     //copy 4 bytes and advance both pointers
    }

    //zero .bss - C gauruntees unitizialted globals start at 0
    dst = _sbss;            //reuse dst pointer
    while (dst < _ebss) {   //walk the .bss range in RAM
        *dst++ = 0;         // write zero, advance
    }

    //call main
    main();

    //if main returns hang, defensive
    while (1) { }
}

//catch all unconfigured interrupt vectors
//weak = real handelers (defined elswhere) override this
//early dev spec
__attribute__((weak, noreturn))
void Default_Handler(void) 
{
    while (1) { }
}