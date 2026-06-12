#Declares minimum CMake version of this file
#Use 3.20 as minimum so it is accesible
cmake_minimum_required(VERSION 3.20)

#Declares project name and which languages CMake should configure it for
project(canary_firmware LANGUAGES C CXX ASM)

#Complies these source fules -> combines them into one executable target_name
#fill in the correct .c source files once written
#THIS CREATES TARGET canary_firmware
add_executable(canary_firmware
    src/main.c 
    startup/startup_stm32g474.s
)

#Tells the compiler where to look when it sees "#include something.h"
#Remember to add in vendor/cmsis/ once CMSIS headers are dropped
target_include_directories(canary_firmware PRIVATE 
    inc/
)

#Per-target compiler flags, on top of the CPU flags from toolchain
#-Wall: enable common warnings, -Wextra: additional warnings, 
#-O0: no optimization, -g3: max debug info
#-ffunction-sections: put each function into its own linke section
#-fdata-sections: put each global var in its own section
target_compile_options(canary_firmware PRIVATE 
    -Wall 
    -Wextra 
    -O0 
    -g3 
    -ffunction-sections 
    -fdata-sections
)

#Per-target linker flags: toolchain file adds CPU flags to the linker, this is for project specific things
#-Wl means you pass an argument THROUGH GCC to the linker
#-T${...}: tells the linker which linker script to use
#-Wl,--gc-sections: discard unused sections
#-nostartfiles: Dont pull the host system default startup code
#-Wl, -Map...: tells the linker to produce a MAP fule showinf where every functiona and varibale is in memory. 
target_link_options(canary_firmware PRIVATE 
    -T${CMAKE_SOURCE_DIR}/linker/stm32g474.ld 
    -Wl,--gc-sections 
    -nostartfiles
    -Wl,-Map=${PROJECT_NAME}.map
)

#These run AFTER BUILD - converts output and reports size
#POST_BUILD: runs after linking
#COMMENT: prints human readable line during build

#convert ELF to BIN 
#-O binary, output format is raw binary
#$<TARGET_FILE:canary_firmware> -> input file (ELF)
#${PROJECT_NAME}.bin -> output file, canary_firmware.bin
add_custom_command(TARGET canary_firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:canary_firmware> ${PROJECT_NAME}.bin
    COMMENT "Generating canary_firmware.bin from ELF"
)

#prints the size of the bin
add_custom_command(TARGET canary_firmware POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:canary_firmware>
    COMMENT "Binary size:"
)
