/* Copyright (C) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv2 license, which unfortunately won't be
 * written for another century.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdio.h>
#include <hardware/i2c.h>
#include <pico/i2c_slave.h>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/gpio.h"

#include "ps2kbd.h"
#include "stdinit.h"
#include "neskbdinter.h"
#include "ps2famikb.h"

#include "kblayout.c"

// $4016 "out" from Famicom/NES, three consecutive pins
#define NES_OUT 2
// $4017 OE $4017 strobe read
#define NES_JOY2OE 10
// $4017 "data" lines, four consecutive pins (five for suborkb+mse, 5)
#define NES_DATA 6

// KBD data and clock inputs must be consecutive with
// data in the lower position.
#define DAT_GPIO 12 // PS/2 data
#define CLK_GPIO 13 // PS/2 clock

// mouse data and clock inputs must be consecutive with
// data in the lower position.
#define MSE_DAT_GPIO 14 // PS/2 data
#define MSE_CLK_GPIO 15 // PS/2 clock

// I2C communication pins
#define I2C_SDA_PIN 0
#define I2C_SCL_PIN 1

// keyboard mode select
#define KB_MODE 26

// direct input or usbhost select
#define USBHOST_ENABLE 28


#define MAX_BUFFER 16
#define WORD_SIZE 4

// keypress matrix for family basic mode & suborkb
static uint8_t keymatrix[] = {
    0, 0, 0, 0, 0, 0, 0, 0, // 0 0, 0 1
    0, 0, 0, 0, 0, 0, 0, 0, // 1 0, 1 1
    0, 0, 0, 0, 0, 0, 0, 0, // 2 0, 2 1
    0, 0, 0, 0, 0, 0, 0, 0, // 3 0, 3 1
    0, 0, 0, 0, 0, 0, 0, 0, // 4 0, 4 1
    0, 0, 0, 0, 0, 0, 0, 0, // 5 0, 5 1
    0, 0, 0, 0, 0, 0, 0, 0, // 6 0, 6 1
    0, 0, 0, 0, 0, 0, 0, 0, // 7 0, 7 1
    0, 0, 0, 0, 0, 0, 0, 0, // 8 0, 8 1 famikb stops here
    0, 0, 0, 0, 0, 0, 0, 0, // 9 0, 9 1
    0, 0, 0, 0, 0, 0, 0, 0, // 10 0, 10 1
    0, 0, 0, 0, 0, 0, 0, 0, // 11 0, 11 1
    0, 0, 0, 0, 0, 0, 0, 0  // 12 0, 12 1
};
static const uint8_t famikey[] = {
    KEY_RIGHTBRACE, KEY_LEFTBRACE, KEY_ENTER, KEY_F8, 
    KEY_END, KEY_YEN, KEY_RIGHTSHIFT, KEY_KATAKANAHIRAGANA,
    KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_F7, // APOSTROPHE maps to COLON
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
static const uint8_t suborkey[] = {
    KEY_4, KEY_G, KEY_F, KEY_C, 
    KEY_F2, KEY_E, KEY_5, KEY_V,
    KEY_2, KEY_D, KEY_S, KEY_END,
    KEY_F1, KEY_W, KEY_3, KEY_X,
    KEY_INSERT, KEY_BACKSPACE, KEY_PAGEDOWN, KEY_RIGHT, 
    KEY_F8, KEY_PAGEUP, KEY_DELETE, KEY_HOME,
    KEY_9, KEY_I, KEY_L, KEY_COMMA,
    KEY_F5, KEY_O, KEY_0, KEY_DOT,
    KEY_RIGHTBRACE, KEY_ENTER, KEY_UP, KEY_LEFT,
    KEY_F7, KEY_LEFTBRACE, KEY_BACKSLASH, KEY_DOWN,
    KEY_Q, KEY_CAPSLOCK, KEY_Z, KEY_TAB,
    KEY_ESC, KEY_A, KEY_1, KEY_LEFTCTRL,
    KEY_7, KEY_Y, KEY_K, KEY_M,
    KEY_F4, KEY_U, KEY_8, KEY_J,
    KEY_MINUS, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_SLASH,
    KEY_F6, KEY_P, KEY_EQUAL, KEY_LEFTSHIFT,
    KEY_T, KEY_H, KEY_N, KEY_SPACE,
    KEY_F3, KEY_R, KEY_6, KEY_B,
    KEY_KP6, KEY_KPENTER, KEY_KP4, KEY_KP8,
    KEY_KP2, 0x00, 0x00, 0x00,
    KEY_LEFTALT, KEY_KP4, KEY_KP7, KEY_F11,
    KEY_F12, KEY_KP1, KEY_KP2, KEY_KP8,
    KEY_KPMINUS, KEY_KPPLUS, KEY_KPASTERISK, KEY_KP9,
    KEY_F10, KEY_KP5, KEY_KPSLASH, KEY_NUMLOCK,
    KEY_GRAVE, KEY_KP6, KEY_PAUSE, KEY_SPACE,
    KEY_F9, KEY_KP3, KEY_KPDOT, KEY_KP0
};


// Just use a 6 byte memory array for storing the latest data for the mouse
static struct
{
    uint8_t mem[6];
    uint8_t mem_address;
    bool mem_address_written;
    bool new_msg;
    bool garbage_message;
} hostmsg;

// FIFO buffer for keypresses for standard mode
// buffer length will be relatively small because under standard operation
// the NES is likely to be reading from the buffer very frequently
static uint8_t bufferindex = 0;
static uint8_t keybuffer[MAX_BUFFER];
static uint8_t kbbbindex = 0;
static uint8_t kbbackbuffer[MAX_BUFFER];
// transport buffer index
static uint8_t transbbindex = 0;
// mouse updates won't be buffered like the keyboard, if multiple updates come
// inbetween a frame, we want to put them together instead of stack them up
static uint8_t msebuffer[4];

static bool NESinlatch = false;

static uint8_t ps2kbmode;
static bool usbhostmode = false;
static uint8_t kblayout;

static uint8_t extended;
static uint8_t release = 1;  // use opposite state
static uint8_t ascii;
static uint32_t output = 0;
static uint8_t select = 0;
static uint8_t enable = 0;
static uint8_t toggle = 0;

static uint32_t kbword = 0;
static uint32_t mseword = 0;

// I2C configuration
static const uint I2C_ADDRESS = 0x17;
static const uint I2C_BAUDRATE = 100000; // 100 kHz
// Our handler is called from the I2C ISR, so it must complete quickly. Blocking calls /
// printing to stdio may interfere with interrupt handling.
static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    switch (event) {
    case I2C_SLAVE_RECEIVE: // master has written some data
        if (!hostmsg.mem_address_written) {
            // writes always start with the memory address
            // the first value here is a len, ignore
            hostmsg.mem_address = i2c_read_byte_raw(i2c);
            // host should always address addr 0 in the buffer
            if (hostmsg.mem_address != 0){
                hostmsg.garbage_message = true;
            } else {
                hostmsg.garbage_message = false;
            }
            hostmsg.mem_address_written = true;
        } else {
            // if it is garbage we just read the values
            // but don't put them in memory
            if (hostmsg.garbage_message) {
                i2c_read_byte_raw(i2c);
            }
            else { // bitwise NOT the values into buffer
                hostmsg.mem[hostmsg.mem_address] = ~i2c_read_byte_raw(i2c);
                hostmsg.mem_address = (hostmsg.mem_address + 1) % 6;
            }
        }
        break;
    case I2C_SLAVE_REQUEST: // master is requesting data
        // load from memory
        i2c_write_byte_raw(i2c, hostmsg.mem[hostmsg.mem_address]);
        hostmsg.mem_address = (hostmsg.mem_address + 1) % 6;
        break;
    case I2C_SLAVE_FINISH: // master has signalled Stop / Restart
        if (!hostmsg.garbage_message) {
            if (ps2kbmode == 1) {
                // do famikbmode
                int state = hostmsg.mem[1] >> 7;
                int ascii = (~hostmsg.mem[1]) & 127;
                for (uint8_t i = 0; i < sizeof(famikey); i++) {
                    if (famikey[i] == ascii) {
                        keymatrix[i] = state;
                        break;
                    }
                }
            } else if (ps2kbmode == 0) {
                // read the value from mem[1] for keyboard to buffer if not ~0x00
                // shouldn't need to check if keyboard is "present" for this
                if (hostmsg.mem[1] != 0xFF) {
                    kbbackbuffer[kbbbindex] = hostmsg.mem[1];
                    kbbbindex = (kbbbindex + 1) % MAX_BUFFER;
                }
                // only update mouse buffer if mouse is "present"
                if ((hostmsg.mem[2] & 32) == 0) {
                    // and the first byte with the current value
                    // this means if a button is pressed, it will stay "pressed"
                    // until the NES polls it (probably next frame)
                    msebuffer[0] = msebuffer[0] & hostmsg.mem[2];
                    // if true, we are in relative mode
                    if ((hostmsg.mem[2] & 8) == 0) {
                        msebuffer[1] += hostmsg.mem[3];
                        msebuffer[2] += hostmsg.mem[4];
                    } else {
                        msebuffer[1] = hostmsg.mem[3];
                        msebuffer[2] = hostmsg.mem[4];   
                    }
                    // some wheel movements or middle button events could
                    // be missed. target for improvement later
                    msebuffer[3] = hostmsg.mem[5];
                }
            }
            hostmsg.new_msg = true;
        }
        
        hostmsg.mem_address_written = false;
        
        break;
    default:
        break;
    }
}

void nes_handler_thread() {

    ps2famikb_init(0, NES_OUT, NES_JOY2OE, NES_DATA, ps2kbmode);

    
    if (ps2kbmode == 1) {
        for (;;) {
            //  read the current $4016 ouput
            uint8_t nesread = ps2famikb_readnes();
            
            enable = nesread & 4;

            if ((nesread & 1) == 1) {  //  reset keyboard row 
                select = 0;
                toggle = 0;
            } else if ((nesread & 2) != toggle) {   // increment keyboard row
                toggle = nesread & 2;
                select = (select + 1) % 18; //  wrap back to first row
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

    } else if (ps2kbmode == 0) {

        for (;;) {

            // check for latch signal and latch the buffers
            if (ps2famikb_chklatch()) {
                // check if latched for buffer context
                NESinlatch = true;

                kbword = 0xFFFFFFFF;
                mseword = 0xFFFFFFFF;
                // load the four oldest buffered values
                for (int i = 0; i < WORD_SIZE; i++) {
                    kbword = kbword << 8;
                    kbword += keybuffer[i];
                    // mouse doesn't actually have a history
                    // just get the latest values
                    mseword = mseword << 8;
                    mseword += msebuffer[i];
                }
                int c = WORD_SIZE;
                if (bufferindex < WORD_SIZE) {
                    c = bufferindex;
                }
                for (int i = c; i < MAX_BUFFER; i++) {
                    keybuffer[i-c] = keybuffer[i];
                }
                bufferindex = bufferindex - c;

                // if there is new data from the host
                if (hostmsg.new_msg) {
                    // actually update button values
                    msebuffer[0] = hostmsg.mem[2];
                    hostmsg.new_msg = false;
                }

                // reset relative values in buffer
                if ((msebuffer[0] & 8) == 0) {
                    msebuffer[1] = msebuffer[2] = 0xFF;
                }
                msebuffer[3] = msebuffer[3] | 127; 

                NESinlatch = false;
            }

            if (ps2famikb_chkstrobe()) {
                mseword = mseword << 1;
                kbword = kbword << 1;
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

    //  configure pico-ps2kb based on gpio
    gpio_init(KB_MODE); gpio_init(KB_MODE+1);
    gpio_set_dir(KB_MODE, GPIO_IN); gpio_set_dir(KB_MODE+1, GPIO_IN);
    gpio_pull_down(KB_MODE); gpio_pull_down(KB_MODE+1);

    //  0: serialized mode
    //  1: famikb mode
    //  2: subor mode
    //  3: ??
    ps2kbmode |= gpio_get(KB_MODE+1) << 1;
    ps2kbmode |= gpio_get(KB_MODE);

    gpio_init(USBHOST_ENABLE);
    gpio_set_dir(USBHOST_ENABLE, GPIO_IN);
    gpio_pull_down(USBHOST_ENABLE);
    
    usbhostmode = gpio_get(USBHOST_ENABLE);

    // prepare buffers
    for (int i = 0; i < MAX_BUFFER; i++) {
        keybuffer[i] = 0xFF;
    }
    for (int i = 1; i < 4; i++) {
        msebuffer[i] = 0xFF;
    }
    // set only the device id in the buffer so if the NES
    // strobes for update before data received it will know
    // the interface is present
    msebuffer[0] = 0xF9;

    //  run the NES handler on seperate core
    multicore_launch_core1(nes_handler_thread);

    //  everything should have initialized, turn on LED
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    if (usbhostmode) {
        gpio_init(I2C_SDA_PIN);
        gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
        gpio_pull_up(I2C_SDA_PIN);

        gpio_init(I2C_SCL_PIN);
        gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
        gpio_pull_up(I2C_SCL_PIN);

        
        i2c_init(i2c0, I2C_BAUDRATE);
        // configure I2C0 for slave mode
        i2c_slave_init(i2c0, I2C_ADDRESS, &i2c_slave_handler);

        // loop forever now
        for (;;) {
            if ((transbbindex != kbbbindex) && !NESinlatch){
                // if the buffer is full, don't do anything yet
                if ((bufferindex+1) < MAX_BUFFER) {
                    keybuffer[bufferindex] = kbbackbuffer[transbbindex];
                    bufferindex++;

                    transbbindex = (transbbindex + 1) % MAX_BUFFER;
                } 
            }
            // gets a little ansy without a little sleep here
            sleep_ms(1);
        }
        
    }
    else {
        kbd_init(1, DAT_GPIO);
    
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
                    }

                    if (ps2kbmode == 1) {
                        // update the status of the key
                        for (uint8_t i = 0; i < sizeof(famikey); i++) {
                            if (famikey[i] == ascii) {
                                keymatrix[i] = release;
                                break;
                            }
                        }
                    } else if (ps2kbmode == 0) {
                        // if the key is being released, add 0x80
                        if (!release && ascii > 0x00) {
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

            // is this even necessary?
            if (bufferindex >= MAX_BUFFER) {
                bufferindex = MAX_BUFFER-1;
            }

        }
    }

}