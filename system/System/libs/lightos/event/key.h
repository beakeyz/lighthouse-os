#ifndef __LIGHTENV_LIBSYS_KEYDEFS__
#define __LIGHTENV_LIBSYS_KEYDEFS__

/*
 * One big enum with (almost) all the USB keyboard scancodes
 * Follows: https://www.usb.org/sites/default/files/documents/hut1_12v2.pdf
 */
enum ANIVA_SCANCODES {
  ANIVA_SCANCODE_UNKNOWN = 0,
  ANIVA_SCANCODE_A = 4,
  ANIVA_SCANCODE_B = 5,
  ANIVA_SCANCODE_C = 6,
  ANIVA_SCANCODE_D = 7,
  ANIVA_SCANCODE_E = 8,
  ANIVA_SCANCODE_F = 9,
  ANIVA_SCANCODE_G = 10,
  ANIVA_SCANCODE_H = 11,
  ANIVA_SCANCODE_I = 12,
  ANIVA_SCANCODE_J = 13,
  ANIVA_SCANCODE_K = 14,
  ANIVA_SCANCODE_L = 15,
  ANIVA_SCANCODE_M = 16,
  ANIVA_SCANCODE_N = 17,
  ANIVA_SCANCODE_O = 18,
  ANIVA_SCANCODE_P = 19,
  ANIVA_SCANCODE_Q = 20,
  ANIVA_SCANCODE_R = 21,
  ANIVA_SCANCODE_S = 22,
  ANIVA_SCANCODE_T = 23,
  ANIVA_SCANCODE_U = 24,
  ANIVA_SCANCODE_V = 25,
  ANIVA_SCANCODE_W = 26,
  ANIVA_SCANCODE_X = 27,
  ANIVA_SCANCODE_Y = 28,
  ANIVA_SCANCODE_Z = 29,

  ANIVA_SCANCODE_1 = 30,
  ANIVA_SCANCODE_2 = 31,
  ANIVA_SCANCODE_3 = 32,
  ANIVA_SCANCODE_4 = 33,
  ANIVA_SCANCODE_5 = 34,
  ANIVA_SCANCODE_6 = 35,
  ANIVA_SCANCODE_7 = 36,
  ANIVA_SCANCODE_8 = 37,
  ANIVA_SCANCODE_9 = 38,
  ANIVA_SCANCODE_0 = 39,

  ANIVA_SCANCODE_RETURN = 40,
  ANIVA_SCANCODE_ESCAPE = 41,
  ANIVA_SCANCODE_BACKSPACE = 42,
  ANIVA_SCANCODE_TAB = 43,
  ANIVA_SCANCODE_SPACE = 44,

  ANIVA_SCANCODE_MINUS = 45,
  ANIVA_SCANCODE_EQUALS = 46,
  ANIVA_SCANCODE_LEFTBRACKET = 47,
  ANIVA_SCANCODE_RIGHTBRACKET = 48,
  ANIVA_SCANCODE_BACKSLASH = 49,

  ANIVA_SCANCODE_NONUSHASH = 50,

  ANIVA_SCANCODE_SEMICOLON = 51,
  ANIVA_SCANCODE_APOSTROPHE = 52,
  ANIVA_SCANCODE_GRAVE = 53,

  ANIVA_SCANCODE_COMMA = 54,
  ANIVA_SCANCODE_PERIOD = 55,
  ANIVA_SCANCODE_SLASH = 56,

  ANIVA_SCANCODE_CAPSLOCK = 57,

  ANIVA_SCANCODE_PRINTSCREEN = 70,
  ANIVA_SCANCODE_SCROLLLOCK = 71,
  ANIVA_SCANCODE_PAUSE = 72,
  ANIVA_SCANCODE_INSERT = 73, 
  ANIVA_SCANCODE_HOME = 74,
  ANIVA_SCANCODE_PAGEUP = 75,
  ANIVA_SCANCODE_DELETE = 76,
  ANIVA_SCANCODE_END = 77,
  ANIVA_SCANCODE_PAGEDOWN = 78,
  ANIVA_SCANCODE_RIGHT = 79,
  ANIVA_SCANCODE_LEFT = 80,
  ANIVA_SCANCODE_DOWN = 81,
  ANIVA_SCANCODE_UP = 82,

  ANIVA_SCANCODE_KP_DIVIDE = 84,
  ANIVA_SCANCODE_KP_MULTIPLY = 85,
  ANIVA_SCANCODE_KP_MINUS = 86,
  ANIVA_SCANCODE_KP_PLUS = 87,
  ANIVA_SCANCODE_KP_ENTER = 88,
  ANIVA_SCANCODE_KP_1 = 89,
  ANIVA_SCANCODE_KP_2 = 90,
  ANIVA_SCANCODE_KP_3 = 91,
  ANIVA_SCANCODE_KP_4 = 92,
  ANIVA_SCANCODE_KP_5 = 93,
  ANIVA_SCANCODE_KP_6 = 94,
  ANIVA_SCANCODE_KP_7 = 95,
  ANIVA_SCANCODE_KP_8 = 96,
  ANIVA_SCANCODE_KP_9 = 97,
  ANIVA_SCANCODE_KP_0 = 98,
  ANIVA_SCANCODE_KP_PERIOD = 99,

  ANIVA_SCANCODE_LCTRL = 224,
  ANIVA_SCANCODE_LSHIFT = 225,
  ANIVA_SCANCODE_LALT = 226,
  ANIVA_SCANCODE_LGUI = 227,
  ANIVA_SCANCODE_RCTRL = 228,
  ANIVA_SCANCODE_RSHIFT = 229,
  ANIVA_SCANCODE_RALT = 230,
  ANIVA_SCANCODE_RGUI = 231,
};

/*
 * Maps index to scancode
 *
 * It's the job of any keyboard driver to translate it's own native scancodes into
 * something like this (Or at least a table that is pointed to by the SCANCODE_TRNSLT_TBL variable)
 */
static enum ANIVA_SCANCODES aniva_scancode_table[512] __attribute__((used)) = {
  ANIVA_SCANCODE_UNKNOWN,
  ANIVA_SCANCODE_ESCAPE,
  ANIVA_SCANCODE_1,
  ANIVA_SCANCODE_2, 
  ANIVA_SCANCODE_3, 
  ANIVA_SCANCODE_4, 
  ANIVA_SCANCODE_5, 
  ANIVA_SCANCODE_6, 
  ANIVA_SCANCODE_7, 
  ANIVA_SCANCODE_8, 
  ANIVA_SCANCODE_9, 
  ANIVA_SCANCODE_0, 
  ANIVA_SCANCODE_MINUS,      
  ANIVA_SCANCODE_EQUALS,     
  ANIVA_SCANCODE_BACKSPACE,  
  ANIVA_SCANCODE_TAB,        
  ANIVA_SCANCODE_Q,          
  ANIVA_SCANCODE_W,          
  ANIVA_SCANCODE_E,          
  ANIVA_SCANCODE_R,          
  ANIVA_SCANCODE_T,          
  ANIVA_SCANCODE_Y,          
  ANIVA_SCANCODE_U,          
  ANIVA_SCANCODE_I,          
  ANIVA_SCANCODE_O,          
  ANIVA_SCANCODE_P,          
  ANIVA_SCANCODE_LEFTBRACKET,
  ANIVA_SCANCODE_RIGHTBRACKET,
  ANIVA_SCANCODE_RETURN,     
  ANIVA_SCANCODE_LCTRL,      
  ANIVA_SCANCODE_A,          
  ANIVA_SCANCODE_S,          
  ANIVA_SCANCODE_D,          
  ANIVA_SCANCODE_F,          
  ANIVA_SCANCODE_G,          
  ANIVA_SCANCODE_H,          
  ANIVA_SCANCODE_J,          
  ANIVA_SCANCODE_K,          
  ANIVA_SCANCODE_L,          
  ANIVA_SCANCODE_SEMICOLON,  
  ANIVA_SCANCODE_APOSTROPHE,
  ANIVA_SCANCODE_GRAVE,      
  ANIVA_SCANCODE_LSHIFT,     
  ANIVA_SCANCODE_BACKSLASH,  
  ANIVA_SCANCODE_Z,          
  ANIVA_SCANCODE_X,          
  ANIVA_SCANCODE_C,          
  ANIVA_SCANCODE_V,          
  ANIVA_SCANCODE_B,          
  ANIVA_SCANCODE_N,          
  ANIVA_SCANCODE_M,          
  ANIVA_SCANCODE_COMMA,      
  ANIVA_SCANCODE_PERIOD,     
  ANIVA_SCANCODE_SLASH,      
  ANIVA_SCANCODE_RSHIFT,     
  ANIVA_SCANCODE_KP_MULTIPLY,
  ANIVA_SCANCODE_LALT,       
  ANIVA_SCANCODE_SPACE,      
  ANIVA_SCANCODE_CAPSLOCK,
  [0x7B] = ANIVA_SCANCODE_HOME,
  [104] = ANIVA_SCANCODE_UP,
  [107] = ANIVA_SCANCODE_LEFT,
  [109] = ANIVA_SCANCODE_RIGHT,
  [112] = ANIVA_SCANCODE_DOWN,
  0
};

#endif // !__LIGHTENV_LIBSYS_KEYDEFS__
