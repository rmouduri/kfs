#ifndef _KEYBOARD_H_
# define _KEYBOARD_H_

# define QWERTY_KEYBOARD_TABLE { \
    {0, 0}, {0, 0}, {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'}, {'5', '%'}, {'6', '^'}, \
    {'7', '&'}, {'8', '*'}, {'9', '('}, {'0', ')'}, {'-', '_'}, {'=', '+'}, {0, 0}, {0, 0}, \
    {'q', 'Q'}, {'w', 'W'}, {'e', 'E'}, {'r', 'R'}, {'t', 'T'}, {'y', 'Y'}, {'u', 'U'}, {'i', 'I'}, \
    {'o', 'O'}, {'p', 'P'}, {'[', '{'}, {']', '}'}, {0, 0}, {0, 0}, {'a', 'A'}, {'s', 'S'}, \
    {'d', 'D'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'}, {';', ':'}, \
    {'\'', '"'}, {'`', '~'}, {0, 0}, {'\\', '|'}, {'z', 'Z'}, {'x', 'X'}, {'c', 'C'}, {'v', 'V'}, \
    {'b', 'B'}, {'n', 'N'}, {'m', 'M'}, {',', '<'}, {'.', '>'}, {'/', '?'}, {0, 0}, {'*', '*'}, \
    {0, 0}, {' ', ' '}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, \
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, \
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, \
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, \
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, \
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, \
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, \
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, \
    {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} \
};

# define TERMINAL_PROMPT_COLORS { \
    { VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK }, \
    { VGA_COLOR_BLUE, VGA_COLOR_BLACK }, \
    { VGA_COLOR_RED, VGA_COLOR_BLACK }, \
    { VGA_COLOR_ORANGE, VGA_COLOR_BLACK }, \
    { VGA_COLOR_CYAN, VGA_COLOR_BLACK }, \
    { VGA_COLOR_MAGENTA, VGA_COLOR_BLACK }, \
    { VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK }, \
    { VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK }, \
    { VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK }, \
    { VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK } \
};

# define COLOR_PALET { \
    "black", /* VGA_COLOR_BLACK */ \
    "blue", /* VGA_COLOR_BLUE */ \
    "green", /* VGA_COLOR_GREEN */ \
    "cyan", /* VGA_COLOR_CYAN */ \
    "red", /* VGA_COLOR_RED */ \
    "magenta", /* VGA_COLOR_MAGENTA */ \
    "orange", /* VGA_COLOR_ORANGE */ \
    "light_grey", /* VGA_COLOR_LIGHT_GREY */ \
    "dark_grey", /* VGA_COLOR_DARK_GREY */ \
    "light_blue", /* VGA_COLOR_LIGHT_BLUE */ \
    "light_green", /* VGA_COLOR_LIGHT_GREEN */ \
    "light_cyan", /* VGA_COLOR_LIGHT_CYAN */ \
    "light_red", /* VGA_COLOR_LIGHT_RED */ \
    "light_magenta", /* VGA_COLOR_LIGHT_MAGENTA */ \
    "yellow", /* VGA_COLOR_YELLOW */ \
    "white" /* VGA_COLOR_WHITE */ \
}

/* KEYBOARD PRESSES */
# define LSHIFT_PRESS	0x2A
# define LSHIFT_RELEASE	0xAA
# define RSHIFT_PRESS	0x36
# define RSHIFT_RELEASE	0xB6

# define CAPSLOCK_PRESS		0x3A
# define CAPSLOCK_RELEASE	0xBA

# define BACKSPACE_PRESS	0x0E
# define ENTER_PRESS		0x1C

# define F1_PRESSED     0x3B
# define F2_PRESSED     0x3C
# define F3_PRESSED     0x3D
# define F4_PRESSED     0x3E
# define F5_PRESSED     0x3F
# define F6_PRESSED     0x40
# define F7_PRESSED     0x41
# define F8_PRESSED     0x42
# define F9_PRESSED     0x43
# define F10_PRESSED    0x44



# define EXTENDED_BYTE	0xE0
/* Extended Bytes sent after 0xE0 */
# define EXTENDED_ENTER_PRESS	0x1C
# define DELETE_PRESS 	0x53

# define CURSOR_RIGHT_PRESS	0x4d
# define CURSOR_LEFT_PRESS	0x4B
# define CURSOR_UP_PRESS	0x48
# define CURSOR_DOWN_PRESS	0x50


void terminal_prompt(void);
void swap_tty(const uint8_t new_tty);
void init_colors(void);
void init_history(void);

#endif // _KEYBOARD_H_