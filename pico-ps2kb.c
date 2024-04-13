/* Copyright (C) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv2 license, which unfortunately won't be
 * written for another century.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "ps2kbd.h"
#include "stdinit.h"
#include "neskbdinter.h"
#include "ps2famikb.h"

// $4016 "out" from Famicom/NES, three consecutive pins
#define NES_OUT 0
// $4017 "data" lines, five consecutive pins
#define NES_DATA 3

// KBD data and clock inputs must be consecutive with
// data in the lower position.
#define DAT_GPIO 14 // PS/2 data
#define CLK_GPIO 15 // PS/2 clock

static uint8_t keymatrix[] = {
    0, 0, 0, 0, 0, 0, 0, 0, // 0 0, 0 1
    0, 0, 0, 0, 0, 0, 0, 0, // 1 0, 1 1
    0, 0, 0, 0, 0, 0, 0, 0, // 2 0, 2 1
    0, 0, 0, 0, 0, 0, 0, 0, // 3 0, 3 1
    0, 0, 0, 0, 0, 0, 0, 0, // 4 0, 4 1
    0, 0, 0, 0, 0, 0, 0, 0, // 5 0, 5 1
    0, 0, 0, 0, 0, 0, 0, 0, // 6 0, 6 1
    0, 0, 0, 0, 0, 0, 0, 0, // 7 0, 7 1
    0, 0, 0, 0, 0, 0, 0, 0  // 8 0, 8 1
};

static const uint8_t famikey[] = {
    KEY_RIGHTBRACE, KEY_LEFTBRACE, KEY_ENTER, KEY_F8, 
    KEY_END, KEY_YEN, KEY_RIGHTSHIFT, KEY_KATAKANAHIRAGANA,
    KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_F7,
    KEY_GRAVE, KEY_MINUS, KEY_SLASH, KEY_BACKSLASH,
    KEY_K, KEY_L, KEY_O, KEY_F6,
    KEY_0, KEY_P, KEY_COMMA, KEY_DOT,
    KEY_J, KEY_U, KEY_I, KEY_F5,
    KEY_8, KEY_9, KEY_N, KEY_M,
    KEY_H, KEY_G, KEY_Y, KEY_F4,
    KEY_6, KEY_7, KEY_V, KEY_B,
    KEY_D, KEY_R, KEY_T, KEY_F3,
    KEY_4, KEY_5, KEY_C, KEY_F,
    KEY_A, KEY_S, KEY_W, KEY_F2,
    KEY_3, KEY_E, KEY_Z, KEY_X,
    KEY_LEFTCTRL, KEY_Q, KEY_ESC, KEY_F1,
    KEY_2, KEY_1, KEY_TAB, KEY_LEFTSHIFT,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_HOME,
    KEY_INSERT, KEY_BACKSPACE, KEY_SPACE, KEY_DOWN
};

static const uint8_t norkeymap[] = {
    0x76, 0x05, 0x06, 0x04,
    0x0C, 0x03, 0x0B, 0x83,
    0x0A, 0x16, 0x1E, 0x26,
    0x25, 0x2E, 0x36, 0x3D,
    0x3E, 0x46, 0x45, 0x4E,
    0x66, 0x0D, 0x15,
    0x1D, 0x24, 0x2D, 0x2C,
    0x35, 0x3C, 0x43, 0x44,
    0x4D, 0x5B, 0x5D,
    0x1C, 0x1B, 0x23, 0x2B,
    0x34, 0x33, 0x3B, 0x42,
    0x4B, 0x4C, 0x52,
    0x5A, 0x12, 0x1A,
    0x22, 0x21, 0x2A, 0x32,
    0x31, 0x3A, 0x41, 0x49,
    0x4A, 0x59, 0x14,
    0x29, 0x0E, 0x5D,
    0x13, 0x6A
};
static const uint8_t norkeys[] = {
    KEY_ESC, KEY_F1, KEY_F2, KEY_F3, 
    KEY_F4, KEY_F5, KEY_F6, KEY_F7,
    KEY_F8, KEY_1, KEY_2, KEY_3,
    KEY_4, KEY_5, KEY_6, KEY_7,
    KEY_8, KEY_9, KEY_0, KEY_MINUS,
    KEY_BACKSPACE, KEY_TAB, KEY_Q,
    KEY_W, KEY_E, KEY_R, KEY_T, 
    KEY_Y, KEY_U, KEY_I, KEY_O, 
    KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE,
    KEY_A, KEY_S, KEY_D, KEY_F, 
    KEY_G, KEY_H, KEY_J, KEY_K, 
    KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE,
    KEY_ENTER, KEY_LEFTSHIFT, KEY_Z,
    KEY_X, KEY_C, KEY_V, KEY_B, 
    KEY_N, KEY_M, KEY_COMMA, KEY_DOT,
    KEY_SLASH, KEY_RIGHTSHIFT, KEY_LEFTCTRL,
    KEY_SPACE, KEY_GRAVE, KEY_BACKSLASH,
    KEY_KATAKANAHIRAGANA, KEY_YEN
};

static const uint8_t extkeymap[] = { 
    0x68, 0x72, 0x74, 0x75,
    0x6C, 0x70, 0x71, 0x69 
};
static const uint8_t extkeys[] = {
    KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_UP,
    KEY_HOME, KEY_INSERT, KEY_DELETE, KEY_END 
};

static uint8_t extended;
static uint8_t release = 1;  // use opposite state
static uint8_t ascii;
static uint32_t output = 0;
static uint8_t select = 0;
static uint8_t enable = 0;
static uint8_t toggle = 0;


void nes_handler_thread() {

    ps2famikb_init(0, NES_OUT, NES_DATA);

    for (;;) {
        //  read the current $4016 ouput
        uint8_t nesread = ps2famikb_readnes();
        
        enable = nesread & 4;

        if ((nesread & 1) == 1) {  //  reset keyboard row 
            select = 0;
            toggle = 0;
        } else if ((nesread & 2) != toggle) {   // increment keyboard row
            toggle = nesread & 2;
            select += 1;
            if (select > 17) {  //  wrap back to first row
                select = 0;
            }
        } 

        //  set current output value on $4017
        output = 0; //  if keyboard is not enabled return 0
        if (enable > 0) {
            for (int i = (4 * select); i < (4 * select)+4; i++) {
                output += keymatrix[i];
                output = output << 1;
            }
        } 
        ps2famikb_putkb(output);
    }
}

int main() {
    set_sys_clock_khz(270000, true);

    //stdio_init();

    //  run the NES handler on seperate core
    multicore_launch_core1(nes_handler_thread);

    //printf("PS/2 KDB example\n");
    kbd_init(1, DAT_GPIO);

    //  everything should have initialized, turn on LED
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);
    
    for (;;) {
        uint8_t code = kbd_getraw();

        //printf("%02d\n", code);

        switch (code) {
            case 0xE0:
                extended = 1;        // extended-ascii code 0xE0 detected
                break;
            case 0xF0:               // key-release code 0xF0 detected
                release = 0;         // set release
                break;               // go back to start
            default:
                if (extended) {  // if extended key, check the extkeymap
                    for (uint8_t i = 0; i < sizeof(extkeymap); i++) {
                        if (extkeymap[i] == code) {
                            ascii = extkeys[i];
                            break;
                        }
                    }
                } else {    // if not extended, check the norkeymap
                    for (uint8_t i = 0; i < sizeof(norkeymap); i++) {
                        if (norkeymap[i] == code) {
                            ascii = norkeys[i];
                            break;
                        }
                    }
                }   // update the status of the key
                for (uint8_t i = 0; i < sizeof(famikey); i++) {
                    if (famikey[i] == ascii) {
                        keymatrix[i] = release;
                        break;
                    }
                }
                extended = 0;
                release = 1;
                break;
        }

    }

}