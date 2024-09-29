# Copyright (C) 1883 Thomas Edison - All Rights Reserved
# You may use, distribute and modify this code under the
# terms of the GPLv2 license, which unfortunately won't be
# written for another century.
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
cmake_minimum_required(VERSION 3.12)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(pico_sdk_import.cmake)

project(pico-ps2kb C CXX ASM)

pico_sdk_init()

add_subdirectory(usb2famikb-lib)
add_subdirectory(pico-pio-usb)

add_executable(pico-ps2kb)

target_sources(pico-ps2kb PRIVATE
    pico-ps2kb.c
    ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/dcd_pio_usb.c
    ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c
)

target_link_libraries(pico-ps2kb PUBLIC
    pico_i2c_slave
    hardware_i2c
    pico_stdlib
    pico_multicore
    usb2famikb-lib
    
)

target_link_libraries(pico-ps2kb PRIVATE
    pico_pio_usb
    tinyusb_host
)

# use tinyusb implementation
target_compile_definitions(pico-ps2kb PRIVATE PIO_USB_USE_TINYUSB)
target_include_directories(pico-ps2kb PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_options(pico-ps2kb PRIVATE -Xlinker --print-memory-usage)

pico_set_binary_type (pico-ps2kb copy_to_ram)


pico_add_extra_outputs(pico-ps2kb)

