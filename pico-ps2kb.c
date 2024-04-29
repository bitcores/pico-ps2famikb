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
#include "hardware/gpio.h"

#include "ps2kbd.h"
#include "stdinit.h"
#include "neskbdinter.h"
#include "ps2famikb.h"

// $4016 "out" from Famicom/NES, three consecutive pins
#define NES_OUT 0
// $4017 OE $4017 strobe read
#define NES_JOY2OE 3
// $4017 "data" lines, four consecutive pins
#define NES_DATA 4

// KBD data and clock inputs must be consecutive with
// data in the lower position.
#define DAT_GPIO 10 // PS/2 data
#define CLK_GPIO 11 // PS/2 clock

// keyboard mode select
#define KB_MODE 12 

#define MAX_BUFFER 16
#define WORD_SIZE 4

// keypress matrix for family basic mode
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

// == Japanese 106/109 Keyboard Layout ==============
uint8_t jpn106nkm[] = {
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
static uint8_t jpn106nk[] = {
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
// -- Extended Keys (E0 prefix) -----------------
static uint8_t jpn106ekm[] = { 
    0x68, 0x72, 0x74, 0x75,
    0x6C, 0x70, 0x71, 0x69 
};
static uint8_t jpn106ek[] = {
    KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_UP,
    KEY_HOME, KEY_INSERT, KEY_DELETE, KEY_END 
};


// FIFO buffer for keypresses for standard mode
// buffer length will be relatively small because under standard operation
// the NES is likely to be reading from the buffer very frequently
static uint8_t bufferindex = 0;
static uint8_t keybuffer[MAX_BUFFER];

static bool isfamikbmode = true;

static uint8_t extended;
static uint8_t release = 1;  // use opposite state
static uint8_t ascii;
static uint32_t output = 0;
static uint8_t select = 0;
static uint8_t enable = 0;
static uint8_t toggle = 0;

static uint32_t kbword = 0;
static uint32_t mseword = 0;


void nes_handler_thread() {

    ps2famikb_init(0, NES_OUT, NES_JOY2OE, NES_DATA, isfamikbmode);

    
    if (isfamikbmode) {
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
                    output = output << 1;
                    output += keymatrix[i];
                }
            } 
            ps2famikb_putkb(output);
        }

    } else {

        for (;;) {
            // check for latch signal and latch the buffers
            if (ps2famikb_chklatch()) {
                kbword = 0xFFFFFFFF;
                mseword = 0xFFFFFFFF;
                // load the four oldest buffered values
                for (int i = 0; i < WORD_SIZE; i++) {
                    kbword = kbword << 8;
                    kbword += keybuffer[i];
                    //mseword = mseword << 8;
                    //mseword += msebuffer[i];
                }
                int c = WORD_SIZE;
                if (bufferindex < WORD_SIZE) {
                    c = bufferindex;
                }
                for (int i = c; i < MAX_BUFFER; i++) {
                    keybuffer[i-c] = keybuffer[i];
                }
                
                bufferindex = bufferindex - c;
            }

            if (ps2famikb_chkstrobe()) {

                //mseword = mseword << 1;
                kbword = kbword << 1;
                //joypad = joypad << 1;
            }

            uint32_t serialout = 0;
            // push next mouse bit in
            serialout += (mseword & 0x80000000) >> 30;
            // push the next keyboard bit in
            serialout += (kbword & 0x80000000) >> 31;

            ps2famikb_putkb(serialout);

        }
    }

}

int main() {
    set_sys_clock_khz(270000, true);

    //stdio_init();
    //printf("PS/2 KDB example\n");
    kbd_init(1, DAT_GPIO);

    //  configure pico-ps2kb based on gpio
    gpio_init(KB_MODE);
    gpio_set_dir(KB_MODE, GPIO_IN);
    gpio_pull_down(KB_MODE);
    
    //  set famikb mode if high
    isfamikbmode = gpio_get(KB_MODE);

    // prepare buffers
    for (int i = 0; i < MAX_BUFFER; i++) {
        keybuffer[i] = 0xFF;
    }

    //  run the NES handler on seperate core
    multicore_launch_core1(nes_handler_thread);

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
                    for (uint8_t i = 0; i < sizeof(jpn106ekm); i++) {
                        if (jpn106ekm[i] == code) {
                            ascii = jpn106ek[i];
                            break;
                        }
                    }
                } else {    // if not extended, check the norkeymap
                    for (uint8_t i = 0; i < sizeof(jpn106nkm); i++) {
                        if (jpn106nkm[i] == code) {
                            ascii = jpn106nk[i];
                            break;
                        }
                    }
                }

                if (isfamikbmode) {
                    // update the status of the key
                    for (uint8_t i = 0; i < sizeof(famikey); i++) {
                        if (famikey[i] == ascii) {
                            keymatrix[i] = release;
                            break;
                        }
                    }
                } else {
                    // if the key is being released, add 0x80
                    if (!release) {
                        ascii += 0x80;
                    }

                    // bitwise NOT the ascii values into the buffer
                    if ((bufferindex+1) == MAX_BUFFER) {
                        for (int i = 1; i < MAX_BUFFER; i++) {
                            keybuffer[i-1] = keybuffer[i];
                        }
                        keybuffer[bufferindex] = ~ascii;
                    }
                    else {
                        keybuffer[bufferindex] = ~ascii;
                        bufferindex++;
                    }
                }

                extended = 0;
                release = 1;
                break;
        }

        if (bufferindex >= MAX_BUFFER) {
            bufferindex = MAX_BUFFER-1;
        }

    }

}