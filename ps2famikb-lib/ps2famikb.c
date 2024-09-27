/* Copyright (C) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv2 license, which unfortunately won't be
 * written for another century.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#pragma GCC push_options
#pragma GCC optimize("-O3")

#include "nesin.pio.h"
#include "kbout.pio.h"

#include "ps2famikb.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"


static PIO nesin_pio;
static PIO kbout_pio;
static uint neskben_sm = 2;
static uint neskbadv_sm = 1;
static uint neskbrst_sm = 0;
static uint nesoe_sm = 3; // could share 1 or 2, at least with one OE
static uint kbout_sm = 0;


void ps2famikb_init(uint nesin_gpio, uint nesoe1_gpio, uint nesoe2_gpio, uint kbout_gpio, uint kbmode) {

    // set up the input side second because we may be stuck here
    nesin_pio = pio0;

    // set up the NES output PIO on $4017
    kbout_pio = pio1;

    for (uint8_t i = nesin_gpio; i < nesin_gpio+3; i++) {
        gpio_init(i);
        gpio_pull_up(i);
    }

    // this line is used for all modes
    uint neskbrstos = pio_add_program(nesin_pio, &nesinrst_program);
    pio_sm_set_consecutive_pindirs(nesin_pio, neskbrst_sm, nesin_gpio, 1, false);
    pio_sm_config neskbrstc = nesinrst_program_get_default_config(neskbrstos);
    sm_config_set_in_pins(&neskbrstc, nesin_gpio);
    sm_config_set_in_shift(&neskbrstc, false, false, 32);
    pio_sm_init(nesin_pio, neskbrst_sm, neskbrstos, &neskbrstc);
    pio_sm_set_enabled(nesin_pio, neskbrst_sm, true);

    // for family basic/subor modes
    if (kbmode > 0) { // set up the NES input PIO on $4016
        uint neskbenos = pio_add_program(nesin_pio, &nesinen_program);
        pio_sm_set_consecutive_pindirs(nesin_pio, neskben_sm, nesin_gpio+2, 1, false);
        pio_sm_config neskbenc = nesinen_program_get_default_config(neskbenos);
        sm_config_set_in_pins(&neskbenc, nesin_gpio+2);
        sm_config_set_in_shift(&neskbenc, false, false, 32);
        pio_sm_init(nesin_pio, neskben_sm, neskbenos, &neskbenc);
        pio_sm_set_enabled(nesin_pio, neskben_sm, true);

        uint neskbadvos = pio_add_program(nesin_pio, &nesinadv_program);
        pio_sm_set_consecutive_pindirs(nesin_pio, neskbadv_sm, nesin_gpio+1, 1, false);
        pio_sm_config neskbadvc = nesinadv_program_get_default_config(neskbadvos);
        sm_config_set_in_pins(&neskbadvc, nesin_gpio+1);
        sm_config_set_in_shift(&neskbadvc, false, false, 32);
        pio_sm_init(nesin_pio, neskbadv_sm, neskbadvos, &neskbadvc);
        pio_sm_set_enabled(nesin_pio, neskbadv_sm, true);

        for (uint8_t i = kbout_gpio; i < kbout_gpio+5; i++) {
            pio_gpio_init(kbout_pio, i);
        }
    
        uint kboutos = pio_add_program(kbout_pio, &kbout_program);
        pio_sm_set_consecutive_pindirs(kbout_pio, kbout_sm, kbout_gpio, 5, true);
        pio_sm_config kboutc = kbout_program_get_default_config(kboutos);
        sm_config_set_out_pins(&kboutc, kbout_gpio, 5);
        sm_config_set_out_shift(&kboutc, true, false, 32);
        pio_sm_init(kbout_pio, kbout_sm, kboutos, &kboutc);
        pio_sm_set_enabled(kbout_pio, kbout_sm, true);
        
    }
    // for keyboard/mouse host mode
    if (kbmode == 0) { // enable output on D3/4
        for (uint8_t i = kbout_gpio; i < kbout_gpio+5; i++) {
            pio_gpio_init(kbout_pio, i);
        }
    
        uint kboutos = pio_add_program(kbout_pio, &kbmseout_program);
        pio_sm_set_consecutive_pindirs(kbout_pio, kbout_sm, kbout_gpio+3, 2, true);
        pio_sm_config kboutc = kbout_program_get_default_config(kboutos);
        sm_config_set_out_pins(&kboutc, kbout_gpio+3, 2);
        sm_config_set_out_shift(&kboutc, true, false, 32);
        pio_sm_init(kbout_pio, kbout_sm, kboutos, &kboutc);
        pio_sm_set_enabled(kbout_pio, kbout_sm, true);    
    }

    if (kbmode == 0 || kbmode == 2) { // set up the NES input PIO on OE from $4017
        gpio_init(nesoe2_gpio);
        gpio_pull_up(nesoe2_gpio);

        uint nesoeos = pio_add_program(nesin_pio, &nesoe_program);
        pio_sm_set_consecutive_pindirs(nesin_pio, nesoe_sm, nesoe2_gpio, 1, false);
        pio_sm_config nesoec = nesoe_program_get_default_config(nesoeos);
        sm_config_set_in_pins(&nesoec, nesoe2_gpio);
        sm_config_set_in_shift(&nesoec, false, false, 32);
        pio_sm_init(nesin_pio, nesoe_sm, nesoeos, &nesoec);
        pio_sm_set_enabled(nesin_pio, nesoe_sm, true);
    }

    if (kbmode == 3) { // set up the NES input PIO on OE from $4016
        gpio_init(nesoe1_gpio);
        gpio_pull_up(nesoe1_gpio);

        uint nesoeos = pio_add_program(nesin_pio, &nesoe_program);
        pio_sm_set_consecutive_pindirs(nesin_pio, nesoe_sm, nesoe1_gpio, 1, false);
        pio_sm_config nesoec = nesoe_program_get_default_config(nesoeos);
        sm_config_set_in_pins(&nesoec, nesoe1_gpio);
        sm_config_set_in_shift(&nesoec, false, false, 32);
        pio_sm_init(nesin_pio, nesoe_sm, nesoeos, &nesoec);
        pio_sm_set_enabled(nesin_pio, nesoe_sm, true);
    }

}

// putting output on the data lines
void ps2famikb_putkb(uint32_t kbcode) {
    pio_sm_put(kbout_pio, kbout_sm, kbcode);
    pio_sm_clear_fifos(kbout_pio, kbout_sm);
}

#pragma GCC pop_options