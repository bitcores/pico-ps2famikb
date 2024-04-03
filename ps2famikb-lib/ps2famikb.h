#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void ps2famikb_init(uint pio, uint nesin_gpio, uint kbout_gpio);

uint8_t ps2famikb_readnes(void);

void ps2famikb_putkb(uint32_t kbcode);

#ifdef __cplusplus
}
#endif
