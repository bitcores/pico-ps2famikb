/* Copyright (C) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv2 license, which unfortunately won't be
 * written for another century.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "ps2kbd.pio.h"

#include "ps2kbd.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"

static PIO kbd_pio;         // pio0 or pio1
static uint kbd_sm;         // pio state machine index
static uint base_gpio;      // data signal gpio #

void kbd_init(uint pio, uint gpio) {
    kbd_pio = pio ? pio1 : pio0;
    base_gpio = gpio; // base_gpio is data signal, base_gpio+1 is clock signal
    // init KBD pins to input
    gpio_init(base_gpio);
    gpio_init(base_gpio + 1);
    // with pull up
    gpio_pull_up(base_gpio);
    gpio_pull_up(base_gpio + 1);
    // get a state machine
    kbd_sm = pio_claim_unused_sm(kbd_pio, true);
    // reserve program space in SM memory
    uint offset = pio_add_program(kbd_pio, &ps2kbd_program);
    // Set pin directions base
    pio_sm_set_consecutive_pindirs(kbd_pio, kbd_sm, base_gpio, 2, false);
    // program the start and wrap SM registers
    pio_sm_config c = ps2kbd_program_get_default_config(offset);
    // Set the base input pin. pin index 0 is DAT, index 1 is CLK
    sm_config_set_in_pins(&c, base_gpio);
    // Shift 8 bits to the right, autopush enabled
    sm_config_set_in_shift(&c, true, true, 8);
    // Deeper FIFO as we're not doing any TX
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);
    // We don't expect clock faster than 16.7KHz and want no less
    // than 8 SM cycles per keyboard clock.
    float div = (float)clock_get_hz(clk_sys) / (8 * 16700);
    sm_config_set_clkdiv(&c, div);
    // Ready to go
    pio_sm_init(kbd_pio, kbd_sm, offset, &c);
    pio_sm_set_enabled(kbd_pio, kbd_sm, true);
}

// clang-format off

#define BS 0x8
#define TAB 0x9
#define LF 0xA
#define ESC 0x1B

// Upper-Case ASCII codes by keyboard-code index, 16 elements per row
static const uint8_t lower[] = {
    0,  0,   0,   0,   0,   0,   0,   0,  0,  0,   0,   0,   0,   TAB, '`', 0,
    0,  0,   0,   0,   0,   'q', '1', 0,  0,  0,   'z', 's', 'a', 'w', '2', 0,
    0,  'c', 'x', 'd', 'e', '4', '3', 0,  0,  ' ', 'v', 'f', 't', 'r', '5', 0,
    0,  'n', 'b', 'h', 'g', 'y', '6', 0,  0,  0,   'm', 'j', 'u', '7', '8', 0,
    0,  ',', 'k', 'i', 'o', '0', '9', 0,  0,  '.', '/', 'l', ';', 'p', '-', 0,
    0,  0,   '\'',0,   '[', '=', 0,   0,  0,  0,   LF,  ']', 0,   '\\',0,   0,
    0,  0,   0,   0,   0,   0,   BS,  0,  0,  0,   0,   0,   0,   0,   0,   0,
    0,  0,   0,   0,   0,   0,   ESC, 0,  0,  0,   0,   0,   0,   0,   0,   0};

// Upper-Case ASCII codes by keyboard-code index
static const uint8_t upper[] = {
    0,  0,   0,   0,   0,   0,   0,   0,  0,  0,   0,   0,   0,   TAB, '~', 0,
    0,  0,   0,   0,   0,   'Q', '!', 0,  0,  0,   'Z', 'S', 'A', 'W', '@', 0,
    0,  'C', 'X', 'D', 'E', '$', '#', 0,  0,  ' ', 'V', 'F', 'T', 'R', '%', 0,
    0,  'N', 'B', 'H', 'G', 'Y', '^', 0,  0,  0,   'M', 'J', 'U', '&', '*', 0,
    0,  '<', 'K', 'I', 'O', ')', '(', 0,  0,  '>', '?', 'L', ':', 'P', '_', 0,
    0,  0,   '"', 0,   '{', '+', 0,   0,  0,  0,   LF,  '}', 0,   '|', 0,   0,
    0,  0,   0,   0,   0,   0,   BS,  0,  0,  0,   0,   0,   0,   0,   0,   0,
    0,  0,   0,   0,   0,   0,   ESC, 0,  0,  0,   0,   0,   0,   0,   0,   0};

// Extended ASCII codes by keyboard-code index
static const uint8_t extkeys[] = {
    0,  0,   0,   0,   0,   0,   0,   0,  0,  0,   0,   0,   0,   0, 0, 0,
    0,  0,   0,   0,   0,   0,   0,   0,  0,  0,   0,   0,   0,   0, 0, 0,
    0,  0,   0,   0,   0,   0,   0,   0,  0,  0,   0,   0,   0,   0, 0, 0,
    0,  0,   0,   0,   0,   0,   0,   0,  0,  0,   0,   0,   0,   0, 0, 0,
    0,  0,   0,   0,   0,   0,   0,   0,  0,  0,   0,   0,   0,   0, 0, 0,
    0,  0,   0,   0,   0,   0,   0,   0,  0,  0,   0,   0,   0,   0, 0, 0,
    0,  0,   0,   0,   0,   0,   0,   0,  0,  0,   0, '<',   0,   0, 0, 0,
    0,  0, 'v',   0, '>', '^',   0,   0,  0,  0,   0,   0,   0,   0, 0, 0};
// clang-format on

static uint8_t release; // Flag indicates the release of a key
static uint8_t shift;   // Shift indication
static uint8_t ascii;   // Translated to ASCII
static uint8_t extended; // flag extended char keys
static uint8_t rawcode; 

int __attribute__((noinline)) kbd_ready(void) {
    if (ascii) // We might already have a character
        return ascii;
    if (pio_sm_is_rx_fifo_empty(kbd_pio, kbd_sm))
        return 0; // no new codes in the fifo
    // pull a scan code from the PIO SM fifo
    uint8_t code = *((io_rw_8*)&kbd_pio->rxf[kbd_sm] + 3);
    switch (code) {
   // case 0xE0:
   //     extended = 1;
    //    break;
    case 0xF0:               // key-release code 0xF0 detected
        release = 1;         // set release
        break;               // go back to start
    case 0x12:               // Left-side SHIFT key detected
    case 0x59:               // Right-side SHIFT key detected
        if (release) {       // L or R Shift detected, test release
            shift = 0;       // Key released preceded  this Shift, so clear shift
            release = 0;     // Clear key-release flag
        } else
            shift = 1; // No previous Shift detected before now, so set Shift_Key flag now
        break;
    default:    
        // no case applies
        if (!release) {      // If no key-release detected yet
            //if (extended) {     // If the key is an extended code
            //    ascii = extkeys[code];
           //     extended = 0;
           // }
           // else                         
                ascii = (shift ? upper : lower)[code]; // Get ASCII value by case
        }
        release = 0;
        break;
    }
    return ascii;
}

int __attribute__((noinline)) kbd_readraw(void) {
    if (rawcode) // We might already have a code
        return rawcode;
    if (pio_sm_is_rx_fifo_empty(kbd_pio, kbd_sm))
        return 0; // no new codes in the fifo
    // pull a scan code from the PIO SM fifo
    uint8_t rawcode = *((io_rw_8*)&kbd_pio->rxf[kbd_sm] + 3);

    return rawcode;
}

char kbd_getc(void) {
    char c;
    while (!(c = kbd_ready()))
        tight_loop_contents();
    ascii = 0;
    return c;
}

uint8_t kbd_getraw(void) {
    uint8_t c;
    while (!(c = kbd_readraw()))
        tight_loop_contents();
    rawcode = 0;
    return c;
}
