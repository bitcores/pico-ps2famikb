// == US 104 Keyboard Layout ==============
static uint8_t norkeymap[] = {
    0x76, 0x05, 0x06, 0x04,
    0x0C, 0x03, 0x0B, 0x83,
    0x0A, 0x16, 0x1E, 0x26,
    0x25, 0x2E, 0x36, 0x3D,
    0x3E, 0x46, 0x45, 0x4E,
    0x66, 0x0D, 0x15,
    0x1D, 0x24, 0x2D, 0x2C,
    0x35, 0x3C, 0x43, 0x44,
    0x4D, 0x54, 0x5B,
    0x1C, 0x1B, 0x23, 0x2B,
    0x34, 0x33, 0x3B, 0x42,
    0x4B, 0x4C, 0x52,
    0x5A, 0x12, 0x1A,
    0x22, 0x21, 0x2A, 0x32,
    0x31, 0x3A, 0x41, 0x49,
    0x4A, 0x59, 0x14,
    0x29, 0x0E, 0x5D,
    0x01, 0x09, 0x78, 0x07,
    0x55, 0x4A
};
static uint8_t norkeys[] = {
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
    KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_EQUAL, KEY_SLASH
};
// -- Extended Keys (E0 prefix) -----------------
static uint8_t extkeymap[] = { 
    0x6B, 0x72, 0x74, 0x75,
    0x6C, 0x70, 0x71, 0x69,
    0x7D, 0x7A, 0x14, 0x5A,
    0x4A
};
static uint8_t extkeys[] = {
    KEY_LEFT, KEY_DOWN, KEY_RIGHT, KEY_UP,
    KEY_HOME, KEY_INSERT, KEY_DELETE, KEY_END,
    KEY_PAGEUP, KEY_PAGEDOWN, KEY_RIGHTCTRL, KEY_KPENTER,
    KEY_KPSLASH
};