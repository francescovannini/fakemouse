cmake_minimum_required(VERSION 3.10)
project(fakemouse C)

enable_language(C ASM)
include_directories(.)
include_directories(usbdrv)

SET(AVRPROG avrdude)
SET(PROGDEVICE usbtiny)
SET(MCU "attiny85")
SET(F_CPU "16500000")
SET(CMAKE_C_COMPILER /usr/bin/avr-gcc)
SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmcu=${MCU} -DF_CPU=${F_CPU} -Wall -Wno-unknown-pragmas -Os -DDEBUG_LEVEL=0")
SET(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}")
SET(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS}")

add_executable(fakemouse
        usbdrv/usbdrvasm.S
        usbdrv/usbdrv.h
        usbdrv/usbdrv.c
        usbconfig.h
        main.c main.h)

add_custom_target(hexfile
        COMMAND avr-objcopy -j .text -j .data -O ihex fakemouse
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        DEPENDS fakemouse)

add_custom_target(flash
        COMMAND "${AVRPROG}" -c "${PROGDEVICE}" -p "${MCU}" -U flash:w:fakemouse.hex:i
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        DEPENDS hexfile)

add_custom_target(fuses
        COMMAND "${AVRPROG}" -c "${PROGDEVICE}" -p "${MCU}" -U hfuse:w:0xdd:m -U lfuse:w:0xe1:m)
