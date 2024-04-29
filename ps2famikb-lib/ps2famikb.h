#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ps2famikb_init(uint pio, uint nesin_gpio, uint nesoe_gpio, uint kbout_gpio, bool kbmode);

uint8_t ps2famikb_readnes(void);

void ps2famikb_putkb(uint32_t kbcode);

bool ps2famikb_chkstrobe(void);

bool ps2famikb_chklatch(void);

#ifdef __cplusplus
}
#endif
