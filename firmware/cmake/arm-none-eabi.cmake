#Block 1: NOT a Mac build
#Generic == No OS, disables host-OS assumption
set(CMAKE_SYSTEM_NAME Generic) 
set(CMAKE_SYSTEM_PROCESSOR arm) 

#Block 2: Disable compile-and-run test: Mac can't run the test file (ARM binaries)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

#Block 3: Point at the ARM GCC Binaries: override each compiler
set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc) #can also assemble .s files
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy)
set(CMAKE_SIZE         arm-none-eabi-size)

#Block 4:2 CPU Flags
# cortex-m4(exact CPU), mthumb(only use thumb), -mfpu...(emit FPU for floats), -mfloat...(pass float arguments in FPU registers)
set(CPU_FLAGS "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard")

#Block 5: Apply the Flags everywhere
#_INIT means to use these as the starting flags
set(CMAKE_C_FLAGS_INIT   "${CPU_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${CPU_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${CPU_FLAGS}")
