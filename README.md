# pico-usb2famikb
Raspberry Pi Pico driven multi device emulator and Keyboard and Mouse Host for Famicom/NES by direct USB input or with an i2c host device

Emulates
* Family Basic Keyboard
* Subor Keyboard and Mouse
* Hori Track
And supports Keyboard and Mouse Host mode, a community developed serialized keyboard and mouse interface.

![](pico-wiring-guide.jpg)
- \* D0 is only needed for subor mouse support. Connecting it will conflict with controller 2, it can be left disconnected. 4016 D1 shares this pin, it is only needed for Hori Track support on Famicom.
- \# D1 and D2 are only needed for Family Basic Keyboard mode, these can be left disconnected (esp. if making for NES player 2 controller port)
- \? USB D+ and D- lines are only needed for direct input. These can be left disconnected if only the i2c Host will be used (i2c Host EN can then be tied to 3.3V to permanently enable it)
![](pico-zero-wiring-guide.jpg)
