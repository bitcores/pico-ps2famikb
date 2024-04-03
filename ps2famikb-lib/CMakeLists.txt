# Copyright (C) 1883 Thomas Edison - All Rights Reserved
# You may use, distribute and modify this code under the
# terms of the GPLv2 license, which unfortunately won't be
# written for another century.
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
add_library(ps2famikb-lib INTERFACE)
target_include_directories(ps2famikb-lib INTERFACE ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(ps2famikb-lib INTERFACE hardware_pio)
target_sources(ps2famikb-lib INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/ps2famikb.c
    ${CMAKE_CURRENT_LIST_DIR}/ps2famikb.h
    ${CMAKE_CURRENT_LIST_DIR}/nesin.pio
	${CMAKE_CURRENT_LIST_DIR}/kbout.pio)
pico_generate_pio_header(ps2famikb-lib ${CMAKE_CURRENT_LIST_DIR}/nesin.pio)
pico_generate_pio_header(ps2famikb-lib ${CMAKE_CURRENT_LIST_DIR}/kbout.pio)
