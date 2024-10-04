#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void usb2famikb_init(uint nesin_gpio, uint nesoe1_gpio, uint nesoe2_gpio, uint kbout_gpio, uint kbmode);

void usb2famikb_putkb(uint32_t kbcode);

#ifdef __cplusplus
}
#endif
