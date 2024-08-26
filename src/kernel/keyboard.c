#include <stdbool.h>

#include "kernel.h"
#include "keyboard.h"
#include "utils.h"


static char	qwerty_keyboard_table[128][2] = QWERTY_KEYBOARD_TABLE;
static bool	lshift = false;
static bool	rshift = false;
static bool	shift = false;
static bool	maj = false;
static bool	rdy_to_disable_maj = false;


static inline void delete_last_char() {
	if (terminal_column[current_tty] == TERMINAL_PROMPT_SIZE) {
		return;
	}

	size_t index = terminal_row[current_tty] * VGA_WIDTH + terminal_column[current_tty];
	size_t x = terminal_column[current_tty];

	for (; x < VGA_WIDTH; ++x, ++index) {
		terminal_putentryat(terminal_buffer[index], terminal_color[current_tty], x - 1, terminal_row[current_tty]);
	}
	terminal_putentryat(EMPTY, terminal_color[current_tty], x - 1, terminal_row[current_tty]);
	--terminal_written_column[current_tty];
	move_cursor_left();
}

static inline void delete_next_char() {
	if (terminal_column[current_tty] >= terminal_written_column[current_tty]) {
		return;
	}

	size_t index = terminal_row[current_tty] * VGA_WIDTH + terminal_column[current_tty];
	size_t x = terminal_column[current_tty];

	for (; x < VGA_WIDTH - 1; ++x, ++index) {
		terminal_putentryat(terminal_buffer[index + 1], terminal_color[current_tty], x, terminal_row[current_tty]);
	}
	terminal_putentryat(EMPTY, terminal_color[current_tty], x, terminal_row[current_tty]);
	--terminal_written_column[current_tty];
}

void terminal_prompt(void) {
	terminal_color[current_tty] = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
	terminal_column[current_tty] = 0;
	terminal_row[current_tty] = VGA_HEIGHT - 1;

	terminal_writestring(TERMINAL_PROMPT);
	terminal_written_column[current_tty] = terminal_column[current_tty];
	terminal_color[current_tty] = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

	for (int i = 0; i < VGA_WIDTH - TERMINAL_PROMPT_SIZE; ++i) {
		terminal_putentryat(EMPTY, terminal_color[current_tty], terminal_column[current_tty] + i, terminal_row[current_tty]);
	}
}

static inline void handle_extended_byte(const uint8_t scan_code) {
	switch (scan_code) {
		case EXTENDED_ENTER_PRESS:
			terminal_prompt();
			break;
		case DELETE_PRESS:
			delete_next_char();
			break;
		case CURSOR_RIGHT_PRESS:
			if (terminal_written_column[current_tty] > terminal_column[current_tty]) {
				move_cursor_right();
			}
			break;
		case CURSOR_LEFT_PRESS:
			move_cursor_left();
			break;
		case CURSOR_UP_PRESS:
			// handle_up_press();
			break;
		case CURSOR_DOWN_PRESS:
			// handle_down_press();
			break;
		default:
			break;
	}
}

inline void swap_tty(const uint8_t new_tty) {
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;

			tty[current_tty][index] = terminal_buffer[index];
			terminal_buffer[index] = tty[new_tty][index];
		}
	}

	current_tty = new_tty;
	update_cursor(terminal_column[current_tty], terminal_row[current_tty]);
}

void isr_keyboard(void) {
    uint8_t scan_code = inb(0x60);
	uint8_t c = qwerty_keyboard_table[scan_code][shift];

	if (c) {
		if (maj && ((c >= 'a' && c <= 'z') || c >= 'A' && c <= 'Z')) {
			c = qwerty_keyboard_table[scan_code][!shift];
		}

		terminal_insert_char(c);
	} else {
		switch (scan_code) {
			case ENTER_PRESS:
				terminal_prompt();
				break;
			case EXTENDED_BYTE:
				scan_code = inb(0x60);
				handle_extended_byte(scan_code);
				break;
			case BACKSPACE_PRESS:
				delete_last_char();
				break;
			case LSHIFT_PRESS:
				lshift = true;
				shift = lshift | rshift;
				break;
			case RSHIFT_PRESS:
				rshift = true;
				shift = lshift | rshift;
				break;
			case LSHIFT_RELEASE:
				lshift = false;
				shift = lshift | rshift;
				break;
			case RSHIFT_RELEASE:
				rshift = false;
				shift = lshift | rshift;
				break;
			case CAPSLOCK_PRESS:
				if (rdy_to_disable_maj == false) {
					maj = true;
					rdy_to_disable_maj = false;
				}
				break;
			case CAPSLOCK_RELEASE:
				if (rdy_to_disable_maj == false) {
					rdy_to_disable_maj = true;
				} else {
					maj = false;
					rdy_to_disable_maj = false;
				}
			case F1_PRESSED:
			case F2_PRESSED:
			case F3_PRESSED:
			case F4_PRESSED:
			case F5_PRESSED:
			case F6_PRESSED:
			case F7_PRESSED:
			case F8_PRESSED:
			case F9_PRESSED:
			case F10_PRESSED:
				const uint8_t new_tty = scan_code - F1_PRESSED;
				if (current_tty != new_tty) {
					swap_tty(new_tty);
				}
				break;
			default:
				break;
		}
	}

    outb(PIC1_COMMAND, 0x20);
}