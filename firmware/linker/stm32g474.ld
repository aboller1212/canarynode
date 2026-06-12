/* ============================================================
 * STM32G474RET6 Linker Script
 * Target: 512 KB Flash, 128 KB SRAM, Cortex-M4F
 * ============================================================ */

/* ---- Physical memory regions on the chip ---- */
/* Addresses are hardwired in silicon; we just describe them. */
MEMORY
{
    /* Flash: non-volatile. Holds code + constants + .data's initial values. */
    /* (rx) = readable + executable. Starts at 0x08000000 per Cortex-M convention. */
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K

    /* SRAM: volatile. Holds stack, globals, and copied-in .data at runtime. */
    /* (rwx) = readable + writable + executable. Base 0x20000000 is Cortex-M architectural. */
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* ---- Where each part of the program lives ---- */
SECTIONS
{
    /* Vector table — MUST be first in flash so it lands at 0x08000000. */
    /* CPU reads initial SP and reset vector from here on power-on. */
    /* KEEP() exempts it from --gc-sections (nothing in C references it; */
    /* the hardware jumps to it directly, so the linker can't see the "use"). */
    .isr_vector : {
        KEEP(*(.isr_vector))
    } > FLASH

    /* Code — all functions from all object files. */
    /* .text* wildcard catches per-function sections from -ffunction-sections. */
    .text : {
        *(.text)
        *(.text*)
    } > FLASH

    /* Read-only data: string literals, const globals. Lives in flash, never copied. */
    .rodata : {
        *(.rodata)
        *(.rodata*)
    } > FLASH

    /* Initialized globals (e.g. int x = 42). */
    /* These RUN from RAM but their initial values are STORED in flash. */
    /* AT> FLASH puts the load image in flash; > RAM sets the runtime address. */
    /* Startup code copies bytes from _sidata (flash) to _sdata..._edata (RAM) at boot. */
    .data : {
        _sdata = .;        /* RAM address: start of .data destination */
        *(.data)
        *(.data*)
        _edata = .;        /* RAM address: end of .data destination */
    } > RAM AT> FLASH

    /* Flash source address for the .data copy. LOADADDR gives the LMA, */
    /* which is different from the VMA (RAM address) we just defined. */
    _sidata = LOADADDR(.data);

    /* Uninitialized globals (e.g. int counter;). C guarantees they start at 0. */
    /* No flash storage — startup code just zeros the RAM range _sbss.._ebss. */
    /* This is why .bss costs 0 bytes of flash regardless of how big it is. */
    .bss : {
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)          /* tentative definitions - rare, but include for correctness */
        _ebss = .;
    } > RAM
}

/* ---- Stack pointer initial value ---- */
/* Stack lives at the top of RAM and grows DOWNWARD on Cortex-M. */
/* The vector table's first entry will reference this symbol, */
/* and the CPU loads it into the SP register automatically on reset. */
/* Evaluates to 0x20000000 + 0x20000 = 0x20020000 (top of 128 KB RAM). */
_estack = ORIGIN(RAM) + LENGTH(RAM);