/* Copyright (C) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv2 license, which unfortunately won't be
 * written for another century.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma GCC push_options
#pragma GCC optimize("-O3")

#include "pio-usb2famikb.pio.h"

#include "usb2famikb.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"

// we are going to use pio0 for all our stuff
// pico_pio_usb uses pio1 to the full
static const PIO picofamikb_pio = pio0;
static const uint neskben_sm = 2;
static const uint neskbadv_sm = 1;
static const uint neskbrst_sm = 0;
static const uint nesoe_sm = 3;

// we need the offset for output enable for other things
static uint nesoeos;


void usb2famikb_init(uint nesin_gpio, uint nesoe1_gpio, uint nesoe2_gpio, uint kbout_gpio, uint usb2kbmode) {

    for (uint8_t i = nesin_gpio; i < nesin_gpio+3; i++) {
        gpio_init(i);
    }

    for (uint8_t i = kbout_gpio; i < kbout_gpio+5; i++) {
        pio_gpio_init(picofamikb_pio, i);
    }

    // this line is used for all modes
    uint neskbrstos = pio_add_program(picofamikb_pio, &nesinrst_program);
    pio_sm_config neskbrstc = nesinrst_program_get_default_config(neskbrstos);

    pio_sm_set_consecutive_pindirs(picofamikb_pio, neskbrst_sm, nesin_gpio, 1, false);
    sm_config_set_in_pins(&neskbrstc, nesin_gpio);
    sm_config_set_in_shift(&neskbrstc, false, false, 32);
    sm_config_set_jmp_pin(&neskbrstc, nesin_gpio);

    pio_sm_init(picofamikb_pio, neskbrst_sm, neskbrstos, &neskbrstc);
    pio_sm_set_enabled(picofamikb_pio, neskbrst_sm, true);

    // for family basic/subor modes
    if (usb2kbmode > 0) { // set up the NES input PIO on $4016
        uint neskbenos = pio_add_program(picofamikb_pio, &nesinen_program);
        pio_sm_config neskbenc = nesinen_program_get_default_config(neskbenos);

        pio_sm_set_consecutive_pindirs(picofamikb_pio, neskben_sm, nesin_gpio+2, 1, false);
        sm_config_set_in_pins(&neskbenc, nesin_gpio+2);
        sm_config_set_in_shift(&neskbenc, false, false, 32);
        sm_config_set_jmp_pin(&neskbenc, nesin_gpio+2);
        
        pio_sm_init(picofamikb_pio, neskben_sm, neskbenos, &neskbenc);
        pio_sm_set_enabled(picofamikb_pio, neskben_sm, true);

        uint neskbadvos = pio_add_program(picofamikb_pio, &nesinadv_program);
        pio_sm_config neskbadvc = nesinadv_program_get_default_config(neskbadvos);

        pio_sm_set_consecutive_pindirs(picofamikb_pio, neskbadv_sm, nesin_gpio+1, 1, false);
        sm_config_set_in_pins(&neskbadvc, nesin_gpio+1);
        sm_config_set_in_shift(&neskbadvc, false, false, 32);
        sm_config_set_jmp_pin(&neskbadvc, nesin_gpio+1);

        pio_sm_init(picofamikb_pio, neskbadv_sm, neskbadvos, &neskbadvc);
        pio_sm_set_enabled(picofamikb_pio, neskbadv_sm, true);
        
    }

    if (usb2kbmode < 3) { // set up the NES input PIO on OE from $4017
        gpio_init(nesoe2_gpio);

        nesoeos = pio_add_program(picofamikb_pio, &nesoe_program);
        pio_sm_config nesoec = nesoe_program_get_default_config(nesoeos);

        pio_sm_set_consecutive_pindirs(picofamikb_pio, nesoe_sm, nesoe2_gpio, 1, false);
        sm_config_set_in_pins(&nesoec, nesoe2_gpio);
        sm_config_set_in_shift(&nesoec, false, false, 32);
        sm_config_set_jmp_pin(&nesoec, nesoe2_gpio);

        pio_sm_set_consecutive_pindirs(picofamikb_pio, nesoe_sm, kbout_gpio, 5, true);
        sm_config_set_out_pins(&nesoec, kbout_gpio, 5);
        sm_config_set_out_shift(&nesoec, true, false, 32);

        pio_sm_init(picofamikb_pio, nesoe_sm, nesoeos, &nesoec);
        pio_sm_set_enabled(picofamikb_pio, nesoe_sm, true);

    } else { // set up the NES input PIO on OE from $4016
        gpio_init(nesoe1_gpio);

        nesoeos = pio_add_program(picofamikb_pio, &nesoe_program);
        pio_sm_config nesoec = nesoe_program_get_default_config(nesoeos);

        pio_sm_set_consecutive_pindirs(picofamikb_pio, nesoe_sm, nesoe1_gpio, 1, false);
        sm_config_set_in_pins(&nesoec, nesoe1_gpio);
        sm_config_set_in_shift(&nesoec, false, false, 32);
        sm_config_set_jmp_pin(&nesoec, nesoe1_gpio);

        pio_sm_set_consecutive_pindirs(picofamikb_pio, nesoe_sm, kbout_gpio, 5, true);
        sm_config_set_out_pins(&nesoec, kbout_gpio, 5);
        sm_config_set_out_shift(&nesoec, true, false, 32);

        pio_sm_init(picofamikb_pio, nesoe_sm, nesoeos, &nesoec);
        pio_sm_set_enabled(picofamikb_pio, nesoe_sm, true);
    }

}

void usb2famikb_putkb(const uint32_t nesout) {
    // we basically just force the SM to pull this data now, no matter
    // what it is doing, and put it on the output
    // then it returns back to where it was
    pio_sm_put(picofamikb_pio, nesoe_sm, nesout);
    pio_sm_exec(picofamikb_pio, nesoe_sm, pio_encode_pull(false, false));
    pio_sm_exec(picofamikb_pio, nesoe_sm, pio_encode_out(pio_pins, 5));
}

#pragma GCC pop_options