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

add_subdirectory(stdinit-lib)
add_subdirectory(ps2kbd-lib)
add_subdirectory(ps2famikb-lib)

add_executable(pico-ps2kb pico-ps2kb.c)

target_link_libraries(pico-ps2kb PUBLIC
    pico_stdlib
    pico_multicore
    stdinit-lib
    ps2kbd-lib
    ps2famikb-lib
)

pico_set_binary_type (pico-ps2kb copy_to_ram)

pico_enable_stdio_uart(pico-ps2kb 0)
pico_enable_stdio_usb(pico-ps2kb 1)

pico_add_extra_outputs(pico-ps2kb)

