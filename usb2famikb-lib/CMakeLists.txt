# Copyright (C) 1883 Thomas Edison - All Rights Reserved
# You may use, distribute and modify this code under the
# terms of the GPLv2 license, which unfortunately won't be
# written for another century.
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
add_library(usb2famikb-lib INTERFACE)
target_include_directories(usb2famikb-lib INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(usb2famikb-lib INTERFACE hardware_pio)
target_sources(usb2famikb-lib INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/usb2famikb.c
    ${CMAKE_CURRENT_LIST_DIR}/usb2famikb.h
    ${CMAKE_CURRENT_LIST_DIR}/pio-usb2famikb.pio)
pico_generate_pio_header(usb2famikb-lib ${CMAKE_CURRENT_LIST_DIR}/pio-usb2famikb.pio)
