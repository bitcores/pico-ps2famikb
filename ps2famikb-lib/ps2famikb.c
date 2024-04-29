/* Copyright (C) 1883 Thomas Edison - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the GPLv2 license, which unfortunately won't be
 * written for another century.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "nesin.pio.h"
#include "kbout.pio.h"

#include "ps2famikb.h"

#include "hardware/clocks.h"
#include "hardware/pio.h"


static PIO nesin_pio;
static PIO nesoe_pio;
static PIO kbout_pio;
static uint neskben_sm;
static uint neskbadv_sm;
static uint neskbrst_sm;
static uint nesoe_sm;
static uint kbout_sm;

static uint8_t kben = 0;
static uint8_t kbadv = 0;
static uint8_t kbrst = 0;

static bool latchstate = false;

void ps2famikb_init(uint pio, uint nesin_gpio, uint nesoe_gpio, uint kbout_gpio, bool kbmode) {

    // set up the input side second because we may be stuck here
    nesin_pio = pio ? pio1 : pio0;

    // set up the NES output PIO on $4017
    kbout_pio = pio ? pio1 : pio0;

    for (uint8_t i = nesin_gpio; i < nesin_gpio+3; i++) {
        gpio_init(i);
        gpio_pull_up(i);
    }

    // this line is used for both modes
    neskbrst_sm = pio_claim_unused_sm(nesin_pio, true);
    uint neskbrstos = pio_add_program(nesin_pio, &nesinprg_program);
    pio_sm_set_consecutive_pindirs(nesin_pio, neskbrst_sm, nesin_gpio, 1, false);
    pio_sm_config neskbrstc = nesinprg_program_get_default_config(neskbrstos);
    sm_config_set_in_pins(&neskbrstc, nesin_gpio);
    sm_config_set_in_shift(&neskbrstc, false, false, 32);
    pio_sm_init(nesin_pio, neskbrst_sm, neskbrstos, &neskbrstc);
    pio_sm_set_enabled(nesin_pio, neskbrst_sm, true);

    if (kbmode) { // set up the NES input PIO on $4016
        neskben_sm = pio_claim_unused_sm(nesin_pio, true);
        uint neskbenos = pio_add_program(nesin_pio, &nesinprg_program);
        pio_sm_set_consecutive_pindirs(nesin_pio, neskben_sm, nesin_gpio+2, 1, false);
        pio_sm_config neskbenc = nesinprg_program_get_default_config(neskbenos);
        sm_config_set_in_pins(&neskbenc, nesin_gpio+2);
        sm_config_set_in_shift(&neskbenc, false, false, 32);
        pio_sm_init(nesin_pio, neskben_sm, neskbenos, &neskbenc);
        pio_sm_set_enabled(nesin_pio, neskben_sm, true);

        neskbadv_sm = pio_claim_unused_sm(nesin_pio, true);
        uint neskbadvos = pio_add_program(nesin_pio, &nesinprg_program);
        pio_sm_set_consecutive_pindirs(nesin_pio, neskbadv_sm, nesin_gpio+1, 1, false);
        pio_sm_config neskbadvc = nesinprg_program_get_default_config(neskbadvos);
        sm_config_set_in_pins(&neskbadvc, nesin_gpio+1);
        sm_config_set_in_shift(&neskbadvc, false, false, 32);
        pio_sm_init(nesin_pio, neskbadv_sm, neskbadvos, &neskbadvc);
        pio_sm_set_enabled(nesin_pio, neskbadv_sm, true);


        for (uint8_t i = kbout_gpio; i < kbout_gpio+4; i++) {
            pio_gpio_init(kbout_pio, i);
        }
    
        kbout_sm = pio_claim_unused_sm(kbout_pio, true);
        uint kboutos = pio_add_program(kbout_pio, &kbout_program);
        pio_sm_set_consecutive_pindirs(kbout_pio, kbout_sm, kbout_gpio, 4, true);
        pio_sm_config kboutc = kbout_program_get_default_config(kboutos);
        sm_config_set_out_pins(&kboutc, kbout_gpio, 4);
        sm_config_set_out_shift(&kboutc, true, false, 32);
        pio_sm_init(kbout_pio, kbout_sm, kboutos, &kboutc);
        pio_sm_set_enabled(kbout_pio, kbout_sm, true);
        
    }
    else { // set up the NES input PIO on OE from $4017
        gpio_init(nesoe_gpio);
        gpio_pull_up(nesoe_gpio);

        nesoe_sm = pio_claim_unused_sm(nesin_pio, true);
        uint nesoeos = pio_add_program(nesin_pio, &nesoe_program);
        pio_sm_set_consecutive_pindirs(nesin_pio, nesoe_sm, nesoe_gpio, 1, false);
        pio_sm_config nesoec = nesoe_program_get_default_config(nesoeos);
        sm_config_set_in_pins(&nesoec, nesoe_gpio);
        sm_config_set_in_shift(&nesoec, false, false, 32);
        pio_sm_init(nesin_pio, nesoe_sm, nesoeos, &nesoec);
        pio_sm_set_enabled(nesin_pio, nesoe_sm, true);


        for (uint8_t i = kbout_gpio; i < kbout_gpio+4; i++) {
            pio_gpio_init(kbout_pio, i);
        }
    
        kbout_sm = pio_claim_unused_sm(kbout_pio, true);
        uint kboutos = pio_add_program(kbout_pio, &kbmseout_program);
        pio_sm_set_consecutive_pindirs(kbout_pio, kbout_sm, kbout_gpio+2, 2, true);
        pio_sm_config kboutc = kbout_program_get_default_config(kboutos);
        sm_config_set_out_pins(&kboutc, kbout_gpio+2, 2);
        sm_config_set_out_shift(&kboutc, true, false, 32);
        pio_sm_init(kbout_pio, kbout_sm, kboutos, &kboutc);
        pio_sm_set_enabled(kbout_pio, kbout_sm, true);    
    }

}

uint8_t __attribute__((noinline)) ps2famikb_readnes(void) {
    uint8_t readin = 0;
    if (!(pio_sm_is_rx_fifo_empty(nesin_pio, neskben_sm))) {
        kben = pio_sm_get(nesin_pio, neskben_sm);
        //pio_sm_clear_fifos(nesin_pio, neskben_sm);
    }
    readin = (readin + kben) << 1;

    if (!(pio_sm_is_rx_fifo_empty(nesin_pio, neskbadv_sm))) {
        kbadv = pio_sm_get(nesin_pio, neskbadv_sm);     
        //pio_sm_clear_fifos(nesin_pio, neskbadv_sm);
    }
    readin = (readin + kbadv) << 1;

    if (!(pio_sm_is_rx_fifo_empty(nesin_pio, neskbrst_sm))) {
        kbrst = pio_sm_get(nesin_pio, neskbrst_sm);
        //pio_sm_clear_fifos(nesin_pio, neskbrst_sm);
    }
    readin += kbrst;
    
    return readin;   
}

void __attribute__((noinline)) ps2famikb_putkb(uint32_t kbcode) {
    pio_sm_put(kbout_pio, kbout_sm, kbcode);
    pio_sm_clear_fifos(kbout_pio, kbout_sm);
}

bool __attribute__((noinline)) ps2famikb_chkstrobe(void) {
    if (!(pio_sm_is_rx_fifo_empty(nesin_pio, nesoe_sm))) {
        pio_sm_get(nesin_pio, nesoe_sm);
        //pio_sm_clear_fifos(nesin_pio, nesoe_sm);
        return true;
    }
    return false;
}

bool __attribute__((noinline)) ps2famikb_chklatch(void) {
    if (!(pio_sm_is_rx_fifo_empty(nesin_pio, neskbrst_sm))) {
        if (pio_sm_get(nesin_pio, neskbrst_sm)) {
            if (!latchstate) {
                latchstate = true;
                return true;
            }
        } else {
            latchstate = false;
        }
        return false;
    }
    return false;
}
