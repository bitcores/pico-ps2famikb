/* Copyright (C) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv2 license, which unfortunately won't be
 * written for another century.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma GCC push_options
#pragma GCC optimize("-O3")

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <hardware/i2c.h>
#include <pico/i2c_slave.h>

#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/irq.h"

#include "pio_usb.h"
#include "tusb.h"

#include "neskbdinter.h"
#include "usb2famikb.h"

#include "kblayout.c"

// $4016 "out" from Famicom/NES, three consecutive pins
#define NES_OUT 2
// $4017 OE $4017 strobe read
#define NES_JOY2OE 10
// $4016 OE $4016 strobe read
#define NES_JOY1OE 11
// $4017 "data" lines, five consecutive pins
#define NES_DATA 5

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

// keyboard mode select, two consecutive pins
#define KB_MODE 26

// direct input or usbhost select
#define i2cHOST_ENABLE 28


#define MAX_BUFFER 16
#define WORD_SIZE 4


#define MSERELATIVE 1
#define HORILHAND 1
#define HORILOWSPD 0
#define SENDREPEATS 1


// configuration for PIO USB
// DPDM configuration D+ = pin 14, D- = pin 15
// configure used SMs around currently used SMs
// use pio1, it has available SMs
#define PIO_USB_CONFIG                                                      \
  {                                                                         \
    14, 1, 0, 0,                                                            \
        1, 1, 2,                                                            \
        NULL, PIO_USB_DEBUG_PIN_NONE,                                       \
        PIO_USB_DEBUG_PIN_NONE, false, PIO_USB_PINOUT_DPDM                  \
  }


// keypress matrix for family basic mode & suborkb
static bool keymatrix[] = {
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
    KEY_C, KEY_F, KEY_G, KEY_4,
    KEY_V, KEY_5, KEY_E, KEY_F2,
    KEY_END, KEY_S, KEY_D, KEY_2,
    KEY_X, KEY_3, KEY_W, KEY_F1,
    KEY_RIGHT, KEY_PAGEDOWN, KEY_BACKSPACE, KEY_INSERT, 
    KEY_HOME, KEY_DELETE, KEY_PAGEUP, KEY_F8,
    KEY_COMMA, KEY_L, KEY_I, KEY_9, 
    KEY_DOT, KEY_0, KEY_O, KEY_F5, 
    KEY_LEFT, KEY_UP, KEY_ENTER, KEY_RIGHTBRACE, 
    KEY_DOWN, KEY_BACKSLASH, KEY_LEFTBRACE, KEY_F7, 
    KEY_TAB, KEY_Z, KEY_CAPSLOCK, KEY_Q,  
    KEY_LEFTCTRL, KEY_1, KEY_A, KEY_ESC, 
    KEY_M, KEY_K, KEY_Y, KEY_7, 
    KEY_J, KEY_8, KEY_U, KEY_F4,  
    KEY_SLASH, KEY_APOSTROPHE, KEY_SEMICOLON, KEY_MINUS, 
    KEY_LEFTSHIFT, KEY_EQUAL, KEY_P, KEY_F6, 
    KEY_SPACE, KEY_N, KEY_H, KEY_T,
    KEY_B, KEY_6, KEY_R, KEY_F3, 
    KEY_KP8, KEY_KP4, KEY_KPENTER, KEY_KP6,
    0x00, 0x00, 0x00, KEY_KP2, // this is odd, verify
    KEY_F11, KEY_KP7, KEY_KP4, KEY_LEFTALT, 
    KEY_KP8, KEY_KP2, KEY_KP1, KEY_F12, 
    KEY_KP9, KEY_KPASTERISK, KEY_KPPLUS, KEY_KPMINUS, 
    KEY_NUMLOCK, KEY_KPSLASH, KEY_KP5, KEY_F10,  
    KEY_SPACE, KEY_PAUSE, KEY_KP6, KEY_GRAVE,
    KEY_KP0, KEY_KPDOT, KEY_KP3, KEY_F9 
};


// Just use a 6 byte memory array for storing the latest data for the mouse
static struct
{
    uint8_t mem[6];
    uint8_t mem_address;
    bool mem_address_written;
    bool garbage_message;
} hostmsg;


const uint LED_PIN = PICO_DEFAULT_LED_PIN;

static bool new_input_msg;
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
// oversizing the buffer type to mitigate overflow
static int16_t msebuffer[4];
// we want to store 
static int16_t mseinstbuf[4];
static int8_t mousex;
static int8_t mousey;

static bool NESinlatch = false;

static uint8_t ps2kbmode;
static bool i2chostmode = false;
static uint8_t kblayout;

static uint32_t output = 0;
static uint8_t select = 0;
static uint8_t enable = 0;
static uint8_t toggle = 0;
static uint8_t setoe2 = 0;
static bool instrobe = false;

static uint32_t kbword = 0;
static uint32_t mseword = 0;

static uint8_t subormouse[4]; // three byte holder for subor mouse data (one byte pad)
static uint8_t sbmouseindex = 0;
static uint8_t sbmouselength = 0; // should be 1 or 3 each report

static uint32_t horitrack = 0;

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
            else { // put thew values into buffer
                hostmsg.mem[hostmsg.mem_address] = i2c_read_byte_raw(i2c);
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
            // parse the value from mem[1] if not 0x00
            if (hostmsg.mem[1] != 0x00) {
                if (ps2kbmode == 1 || ps2kbmode == 3) {
                    // famikey modes
                    bool state = (hostmsg.mem[1] >> 7) ? 0: 1;
                    uint8_t ascii = hostmsg.mem[1] & 127;
                    for (uint i = 0; i < sizeof(famikey); i++) {
                        if (famikey[i] == ascii) {
                            keymatrix[i] = state;
                            break;
                        }
                    }
                } else if (ps2kbmode == 2) {
                    // subor mode
                    bool state = (hostmsg.mem[1] >> 7) ? 0: 1;
                    uint8_t ascii = hostmsg.mem[1] & 127;
                    for (uint i = 0; i < sizeof(suborkey); i++) {
                        if (suborkey[i] == ascii) {
                            keymatrix[i] = state;
                            // some keys appear more than once in the matrix
                            //break;
                        }
                    }
                } else { // keyboard mouse host mode
                    // shouldn't need to check if keyboard is "present" for this
                    kbbackbuffer[kbbbindex] = hostmsg.mem[1];
                    kbbbindex = (kbbbindex + 1) % MAX_BUFFER;
                }
            }
            // only update mouse buffer if mouse is "present"
            if ((hostmsg.mem[2] & 32) == 32) {
                // and the first byte with the current value
                // this means if a button is pressed, it will stay "pressed"
                // until the NES polls it (probably next frame)
                msebuffer[0] |= hostmsg.mem[2];
                // if true, we are in relative mode
                if ((msebuffer[0] & 8) == 8 && new_input_msg) {
                    msebuffer[1] += (int8_t)hostmsg.mem[3];
                    msebuffer[2] += (int8_t)hostmsg.mem[4];
                } else {
                    msebuffer[1] = (int8_t)hostmsg.mem[3];
                    msebuffer[2] = (int8_t)hostmsg.mem[4];   
                }
                // some wheel movements or middle button events could
                // be missed. target for improvement later
                msebuffer[3] |= hostmsg.mem[5];
            }
            new_input_msg = true;
        }
        
        hostmsg.mem_address_written = false;
        
        break;
    default:
        break;
    }
}

// when mouse data is sent to the NES, update relevant buffer data
static void update_mouse_data() {
    // if there is new data from the host
    if (new_input_msg) {
        if (i2chostmode) {
            // actually update button values
            msebuffer[0] = hostmsg.mem[2];
            msebuffer[3] = hostmsg.mem[5];
        } else {
            msebuffer[0] = mseinstbuf[0];
            msebuffer[1] = mseinstbuf[1];
            msebuffer[2] = mseinstbuf[2];
            msebuffer[3] = mseinstbuf[3] & 0x87;
        }
        new_input_msg = false;
    }
}

// IRQ handler for OE lines to shift data
void pio_IRQ_handler() {

    if (pio_interrupt_get(pio0, 3)) {
        if (ps2kbmode == 2) {
            // move the subor mouse data along
            subormouse[sbmouseindex] = subormouse[sbmouseindex] << 1;
        }
        else if (ps2kbmode == 0) {
            mseword = mseword << 1;
            kbword = kbword << 1;
        }
        else if (ps2kbmode == 3) {
            horitrack = horitrack << 1;
        }

        pio_interrupt_clear(pio0, 3);
    }
}


void nes_handler_thread() {

    ps2famikb_init(NES_OUT, NES_JOY1OE, NES_JOY2OE, NES_DATA, ps2kbmode);

    
    if (ps2kbmode > 0) {

        if (ps2kbmode >= 2) {
            pio_set_irq0_source_enabled(pio0, pis_interrupt3, true);
            irq_set_exclusive_handler(PIO0_IRQ_0, pio_IRQ_handler);
        }
        irq_set_enabled(PIO0_IRQ_0, true);

        for (;;) {
            //  read the current $4016 ouput
            uint8_t nesread = (pio0->intr >> 8) & 0x0F;
            
            // fami/subor keyboard enable
            enable = nesread & 4;

            // read the strobe value, if was previously in strobe
            // exit strobe if no longer in strobe
            uint8_t strobe = nesread & 1;
            if (!strobe && instrobe) {
                instrobe = false;
            }
            
            // only reset/prepare data if beginning of strobe
            if (strobe && !instrobe) {  //  reset keyboard row/strobe mouse
                instrobe = true;
                // if the keyboard is enabled, reset it to prepare for reading 
                select = 0;
                toggle = 0;

                // if there is new data, load it
                // otherwise, give a 0
                if (new_input_msg) {
                    if (msebuffer[1] < -128) {
                        mousex = -128;
                    } else if (msebuffer[1] > 127) {
                        mousex = 127;
                    } else {
                        mousex = msebuffer[1];
                    }
                    if (msebuffer[2] < -128) {
                        mousey = -128;
                    } else if (msebuffer[2] > 127) {
                        mousey = 127;
                    } else {
                        mousey = msebuffer[2];
                    }
                } else {
                    mousex = mousey = 0x00;
                }

                // if subor or famikb+horitrack modes, prepare mouse data
                if (ps2kbmode == 2) {
                    // progress index if less than length
                    if (sbmouseindex < sbmouselength) {
                        sbmouseindex++;
                    }

                    // if index is equal to length, time to make a new report
                    if (sbmouseindex == sbmouselength) {
                        // reset index
                        sbmouseindex = 0;
                        sbmouselength = 0;
                        
                        if (!enable) {
                            // it is a single byte report
                            if ((mousex >= -1 && mousex <= 1) && (mousey >= -1 && mousey <= 1)) {
                                sbmouselength = 1;
                                subormouse[0] = subormouse[1] = subormouse[2] = 0x00;

                                // set mouse left/right buttons
                                subormouse[0] |= (uint8_t) msebuffer[0] & 0xC0;
                                // mouse X/Y reports
                                subormouse[0] |= (mousex & 0x03) << 4;
                                subormouse[0] |= (mousey & 0x03) << 2;
                                // byte identifier
                                //subormouse[0] += 0x00; 
                            } else {
                                sbmouselength = 3;
                                uint8_t xdir = 0;
                                uint8_t ydir = 0;

                                // clamp movement to maximum "31" and set dir
                                xdir = (mousex & 0x80) >> 7;
                                ydir = (mousey & 0x80) >> 7;
                                if (xdir) {
                                    mousex = ~mousex;
                                }
                                if (mousex > 31) {
                                    mousex = 31;
                                }
                                if (ydir) {
                                    mousey = ~mousey;
                                }
                                if (mousey > 31) {
                                    mousey = 31;
                                }

                                // first byte
                                subormouse[0] = 0x00;
                                subormouse[0] |= msebuffer[0] & 0xC0;
                                subormouse[0] |= xdir << 5;
                                subormouse[0] |= mousex & 0x10;
                                subormouse[0] |= ydir << 3;
                                subormouse[0] |= (mousey & 0x10) >> 2;
                                subormouse[0] |= 0x01;

                                // second byte
                                subormouse[1] = 0x00;
                                subormouse[1] |= (mousex & 0x0F) << 2;
                                subormouse[1] |= 0x02;

                                // third byte
                                subormouse[2] = 0x00;
                                subormouse[2] |= (mousey & 0x0F) << 2;
                                subormouse[2] |= 0x03;

                            }

                            update_mouse_data();
 
                        }

                    }
                } else if (ps2kbmode == 3) {
                    horitrack = 0x00;
                    horitrack |= msebuffer[0] & 0xC0;
                    horitrack <<= 4;
                    if (mousex < -8) {
                        mousex = -8;
                    } else if (mousex > 7) {
                        mousex = 7;
                    }
                    if (mousey < -8) {
                        mousey = -8;
                    } else if (mousey > 7) {
                        mousey = 7;
                    }
                    horitrack |= (~mousey) & 0x0F;
                    horitrack <<= 4;
                    horitrack |= (~mousex) & 0x0F;
                    horitrack <<= 4;
                    horitrack |= ((msebuffer[3] & 0x03) << 2) + 1;
                    horitrack <<= 12;

                    update_mouse_data();

                }
            } else if ((nesread & 2) != toggle) {   // increment keyboard row
                toggle = nesread & 2;
                if (ps2kbmode == 2) {   //  wrap back to first row
                    select = (select + 1) % 26; // 26 blocks for subor
                } else {
                    select = (select + 1) % 18; // 18 blocks for famikb
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

            if (ps2kbmode == 2) {
                // append subor mouse data
                output += (subormouse[sbmouseindex] >> 7) ? 0: 1;
            } else if (ps2kbmode == 3) {
                output += (horitrack >> 31) ? 0: 1;
            }

            ps2famikb_putkb(output);
        }

    } else {

        pio_set_irq0_source_enabled(pio0, pis_interrupt3, true);
        irq_set_exclusive_handler(PIO0_IRQ_0, pio_IRQ_handler);
        irq_set_enabled(PIO0_IRQ_0, true);

        for (;;) {

            uint8_t nesread = (pio0->intr >> 8) & 0x0F;
            
            // read the strobe value, if was previously in strobe
            // exit strobe if no longer in strobe
            uint8_t strobe = nesread & 1;
            if (!strobe && instrobe) {
                instrobe = false;
            }

            // check for strobe signal and latch the buffers
            if (strobe && !instrobe) {  //  reset keyboard row/strobe mouse
                instrobe = true;
                // check if latched for buffer context
                NESinlatch = true;

                kbword = 0x00000000;
                mseword = 0x00000000;
                // load the four oldest buffered values
                for (int i = 0; i < WORD_SIZE; i++) {
                    kbword = kbword << 8;
                    kbword += keybuffer[i];
                    // mouse doesn't actually have a history
                    // just get the latest values
                    mseword = mseword << 8;
                    mseword += (uint8_t) msebuffer[i];
                }
                int c = WORD_SIZE;
                if (bufferindex < WORD_SIZE) {
                    c = bufferindex;
                }
                for (int i = c; i < MAX_BUFFER; i++) {
                    keybuffer[i-c] = keybuffer[i];
                }
                bufferindex = bufferindex - c;

                update_mouse_data();

                NESinlatch = false;
            }

            uint32_t serialout = 0;
            // push next mouse bit in
            serialout += (~mseword & 0x80000000) >> 27;
            // push the next keyboard bit in
            serialout += (~kbword & 0x80000000) >> 28;
        
            ps2famikb_putkb(serialout);
        }
    }

}

int main() {
    // need a clock speed that is a multiple of 12,000
    //set_sys_clock_khz(264000, true);
    set_sys_clock_khz(264000, true);

    //  configure pico-ps2kb based on gpio
    gpio_init(KB_MODE); gpio_init(KB_MODE+1);
    gpio_set_dir(KB_MODE, GPIO_IN); gpio_set_dir(KB_MODE+1, GPIO_IN);
    gpio_pull_down(KB_MODE); gpio_pull_down(KB_MODE+1);

    //  0: serialized mode
    //  1: famikb mode
    //  2: subor mode
    //  3: famikb+hori trackball
    ps2kbmode |= gpio_get(KB_MODE+1) << 1;
    ps2kbmode |= gpio_get(KB_MODE);

    gpio_init(i2cHOST_ENABLE);
    gpio_set_dir(i2cHOST_ENABLE, GPIO_IN);
    gpio_pull_down(i2cHOST_ENABLE);
    
    i2chostmode = gpio_get(i2cHOST_ENABLE);

    // prepare buffers
    for (int i = 0; i < MAX_BUFFER; i++) {
        keybuffer[i] = 0x00;
    }
    for (int i = 1; i < 4; i++) {
        msebuffer[i] = 0x00;
        subormouse[i-1] = 0x00;
        mseinstbuf[i] = 0x00;
    }
    // set only the device id in the buffer so if the NES
    // strobes for update before data received it will know
    // the interface is present
    msebuffer[0] = mseinstbuf[0] = 0x06;

    multicore_reset_core1();
    //  run the NES handler on seperate core
    multicore_launch_core1(nes_handler_thread);

    //  everything should have initialized, turn on LED
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    if (i2chostmode) {
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
        mseinstbuf[0] |= MSERELATIVE << 3;
        sleep_ms(10);

        // Use tuh_configure() to pass pio configuration to the host stack
        // Note: tuh_configure() must be called before
        pio_usb_configuration_t pio_cfg = PIO_USB_CONFIG;
        tuh_configure(1, TUH_CFGID_RPI_PIO_USB_CONFIGURATION, &pio_cfg);

        // To run USB SOF interrupt in core0, init host stack for pio_usb (roothub
        // port1) on core0
        tuh_init(1);

        /*if (tuh_inited()) {
            sleep_ms(500);
            gpio_put(LED_PIN, 0);
            sleep_ms(500);
            gpio_put(LED_PIN, 1);
        }*/

        while (true) {
            tuh_task(); // tinyusb host task
        }
    }

}

static void keycode_handler(uint8_t ascii, uint8_t release) {

    if (ps2kbmode == 1 || ps2kbmode == 3) { // famikey modes
        // update the status of the key
        for (uint8_t i = 0; i < sizeof(famikey); i++) {
            if (famikey[i] == ascii) {
                keymatrix[i] = release;
                break;
            }
        }
    } else if (ps2kbmode == 2) { // subor mode
        // update the status of the key
        for (uint8_t i = 0; i < sizeof(suborkey); i++) {
            if (suborkey[i] == ascii) {
                keymatrix[i] = release;
                // some keys appear more than once in the matrix
                //break;
            }
        }
    } else { // keyboard mouse host mode
        // if the key is being released, add 0x80
        if (!release && ascii > 0x00) {
            ascii += 0x80;
        }

        // bitwise NOT the ascii values into the buffer
        if ((bufferindex+1) == MAX_BUFFER) {
            for (int i = 1; i < MAX_BUFFER; i++) {
                keybuffer[i-1] = keybuffer[i];
            }
            keybuffer[bufferindex] = ascii;
        }
        else {
            keybuffer[bufferindex] = ascii;
            bufferindex++;
        }
    }

}


//--------------------------------------------------------------------+
// Host HID
//--------------------------------------------------------------------+

// Invoked when device with hid interface is mounted
// Report descriptor is also available for use. tuh_hid_parse_report_descriptor()
// can be used to parse common/simple enough descriptor.
// Note: if report descriptor length > CFG_TUH_ENUMERATION_BUFSIZE, it will be skipped
// therefore report_desc = NULL, desc_len = 0
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len)
{
    (void)desc_report;
    (void)desc_len;

    // Interface protocol (hid_interface_protocol_enum_t)
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    //uint16_t vid, pid;
    //tuh_vid_pid_get(dev_addr, &vid, &pid);

    char tempbuf[256];
    //int count = sprintf(tempbuf, "[%04x:%04x][%u] HID Interface%u, Protocol = %s\r\n", vid, pid, dev_addr, instance, protocol_str[itf_protocol]);

    //tud_cdc_write(tempbuf, count);
    //tud_cdc_write_flush();

    // Receive report from boot keyboard & mouse only
    // tuh_hid_report_received_cb() will be invoked when report is available
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD || itf_protocol == HID_ITF_PROTOCOL_MOUSE) {
        switch (itf_protocol) {
            case (HID_ITF_PROTOCOL_KEYBOARD):
                mseinstbuf[0] |= 0x10;
                break;
            case (HID_ITF_PROTOCOL_MOUSE):
                mseinstbuf[0] |= 0x20;
                break;
        }
        new_input_msg = true;

        //  set up report receiving
        tuh_hid_receive_report(dev_addr, instance);

    }
}

// Invoked when device with hid interface is un-mounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance)
{
    char tempbuf[256];
    //int count = sprintf(tempbuf, "[%u] HID Interface%u is unmounted\r\n", dev_addr, instance);
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
    
    switch (itf_protocol) {
        case (HID_ITF_PROTOCOL_KEYBOARD):
            mseinstbuf[0] &= 0xEF;
            break;
        case (HID_ITF_PROTOCOL_MOUSE):
            mseinstbuf[0] &= 0xDF;
            break;
    }
    new_input_msg = true;
    
    //tud_cdc_write(tempbuf, count);
    //tud_cdc_write_flush();
}

// look up new key in previous keys
static inline bool find_key_in_report(hid_keyboard_report_t const *report, uint8_t keycode)
{
    for(uint8_t i=0; i<6; i++)
    {
        if (report->keycode[i] == keycode)  return true;
    }

    return false;
}

// look up new key in previous keys
static inline uint8_t find_release_in_report(hid_keyboard_report_t const *report, uint8_t index)
{
    if (report->keycode[index] > 0x00) return report->keycode[index];
    return 0x00;
}

// convert hid keycode to ascii and print via usb device CDC (ignore non-printable)
static void process_kbd_report(hid_keyboard_report_t const *report)
{
    static hid_keyboard_report_t prev_report = { 0, 0, {0} }; // previous report to check key released

    for(uint8_t i=0; i<6; i++)
    {
        uint8_t keycode = report->keycode[i];
        if ( keycode )
        {
            if ( find_key_in_report(&prev_report, keycode)) {
                // ignore for now would like a repeat thing
            } else {
                keycode_handler(keycode, 0x01);
            }
        }
        else {
            keycode = find_release_in_report(&prev_report, i);
            if (keycode > 0x00) {
                keycode_handler(keycode, 0x00);
            }
        }
    }

    prev_report = *report;
}

// send mouse report to usb device CDC
static void process_mouse_report(hid_mouse_report_t const * report)
{

    //------------- button state  -------------//
    //uint8_t button_changed_mask = report->buttons ^ prev_report.buttons;
    uint8_t temp = mseinstbuf[0] & 0x3F;
    temp |= (report->buttons & MOUSE_BUTTON_LEFT) << 7;
    temp |= (report->buttons & MOUSE_BUTTON_RIGHT) << 5;
    mseinstbuf[0] = temp;
    msebuffer[0] |= mseinstbuf[0];

    temp = 0x00;
    temp |= (report->buttons & MOUSE_BUTTON_MIDDLE) << 5;
    temp |= (report->wheel & 0x0F) << 3;
    temp |= HORILHAND << 1;
    temp |= HORILOWSPD;
    mseinstbuf[3] = temp;
    msebuffer[3] |= mseinstbuf[3];

    if (MSERELATIVE) {
        if (new_input_msg) {
            msebuffer[1] += report->x;
            msebuffer[2] += report->y;
            mseinstbuf[1] = report->x;
            mseinstbuf[2] = report->y;
        } else {
            msebuffer[1] = mseinstbuf[1] = report->x;
            msebuffer[2] = mseinstbuf[2] = report->y;
        }
    } else {
        mseinstbuf[1] += report->x;
        if (mseinstbuf[1] < 0) mseinstbuf[1] = 0;
        if (mseinstbuf[1] > 255) mseinstbuf[1] = 255;

        mseinstbuf[2] += report->y;
        if (mseinstbuf[2] < 0) mseinstbuf[2] = 0;
        if (mseinstbuf[2] > 255) mseinstbuf[2] = 255;
    }

    new_input_msg = true;

}

// Invoked when received report from device via interrupt endpoint
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len)
{
    (void) len;
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    switch(itf_protocol)
    {
        case HID_ITF_PROTOCOL_KEYBOARD:
        process_kbd_report((hid_keyboard_report_t const*) report );
        break;

        case HID_ITF_PROTOCOL_MOUSE:
        process_mouse_report((hid_mouse_report_t const*) report );
        break;

        default: break;
    }

    // continue to request to receive report
    tuh_hid_receive_report(dev_addr, instance);
}


#pragma GCC pop_options