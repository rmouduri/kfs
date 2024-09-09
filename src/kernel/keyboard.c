#include <stdbool.h>

#include "kernel.h"
#include "keyboard.h"
#include "utils.h"


static const uint8_t default_colors[MAX_TTY][2] = TERMINAL_PROMPT_COLORS;
static uint8_t	prompts_colors[MAX_TTY];
static char		qwerty_keyboard_table[128][2] = QWERTY_KEYBOARD_TABLE;
static size_t	history_total_index[MAX_TTY];
static int		history_current_index[MAX_TTY];
static uint16_t	history[MAX_TTY][MAX_HISTORY][VGA_WIDTH];
static bool		lshift = false;
static bool		rshift = false;
static bool		shift = false;
static bool		maj = false;
static bool		rdy_to_disable_maj = false;


void init_colors(void) {
	for (int i = 0; i < MAX_TTY; ++i) {
		prompts_colors[i] = vga_entry_color(default_colors[i][0], default_colors[i][1]);
	}
}

void init_history(void) {
	kmemset(history_current_index, 0, sizeof(uint8_t) * MAX_TTY);
	kmemset(history_total_index, 0, sizeof(uint8_t) * MAX_TTY);
	kmemset(history, 0, sizeof(uint16_t) * (MAX_TTY * MAX_HISTORY * VGA_WIDTH));
}

static inline void delete_last_char(void) {
	if (terminal_column[curr_tty] == TERMINAL_PROMPT_LEN) {
		return;
	}

	size_t index = terminal_row[curr_tty] * VGA_WIDTH + terminal_column[curr_tty];
	size_t x = terminal_column[curr_tty];

	for (; x < VGA_WIDTH; ++x, ++index) {
		terminal_putentryat(terminal_buffer[index], terminal_color[curr_tty], x - 1, terminal_row[curr_tty]);
	}
	terminal_putentryat(EMPTY, terminal_color[curr_tty], x - 1, terminal_row[curr_tty]);
	--terminal_written_column[curr_tty];
	move_cursor_left();
}

static inline void delete_next_char(void) {
	if (terminal_column[curr_tty] >= terminal_written_column[curr_tty]) {
		return;
	}

	size_t index = terminal_row[curr_tty] * VGA_WIDTH + terminal_column[curr_tty];
	size_t x = terminal_column[curr_tty];

	for (; x < VGA_WIDTH - 1; ++x, ++index) {
		terminal_putentryat(terminal_buffer[index + 1], terminal_color[curr_tty], x, terminal_row[curr_tty]);
	}
	terminal_putentryat(EMPTY, terminal_color[curr_tty], x, terminal_row[curr_tty]);
	--terminal_written_column[curr_tty];
}

void save_to_history(void) {
	long input_end = terminal_written_column[curr_tty];

	for (; input_end >= TERMINAL_PROMPT_LEN; --input_end) {
		if ((terminal_buffer[VGA_WIDTH * (VGA_HEIGHT - 1) + input_end] & 0x00FF) != EMPTY) {
			break ;
		}
	}

	if (input_end < TERMINAL_PROMPT_LEN) {
		return ;
	}

	if (input_end < VGA_WIDTH - 1) {
		history[curr_tty][history_total_index[curr_tty] % MAX_HISTORY][input_end + 1] = 0;
	}

	for (; input_end >= 0; --input_end) {
		history[curr_tty][history_total_index[curr_tty] % MAX_HISTORY][input_end] =
			terminal_buffer[VGA_WIDTH * (VGA_HEIGHT - 1) + input_end];
	}

	++history_total_index[curr_tty];
}

void terminal_prompt(void) {
	const uint16_t og_color = terminal_color[curr_tty];

	terminal_color[curr_tty] = prompts_colors[curr_tty];
	terminal_column[curr_tty] = 0;
	terminal_row[curr_tty] = VGA_HEIGHT - 1;

	terminal_writestring(TERMINAL_PROMPT);
	terminal_written_column[curr_tty] = terminal_column[curr_tty];
	terminal_color[curr_tty] = og_color;

	for (int i = 0; i < VGA_WIDTH - TERMINAL_PROMPT_LEN; ++i) {
		terminal_putentryat(EMPTY, terminal_color[curr_tty], terminal_column[curr_tty] + i, terminal_row[curr_tty]);
	}
}

static inline void handle_down_press(void) {
	if (!history_total_index[curr_tty] || history_current_index[curr_tty] == -1) {
		return;
	} else if (history_current_index[curr_tty] == (history_total_index[curr_tty] - 1) % MAX_HISTORY) {
		history_current_index[curr_tty] = -1;
		terminal_prompt();
		return;
	}

	if (history_total_index[curr_tty] <= MAX_HISTORY && history_current_index[curr_tty] < history_total_index[curr_tty]) {
		++history_current_index[curr_tty];
	} else if (history_total_index[curr_tty] > MAX_HISTORY) {
		if (history_current_index[curr_tty] == MAX_HISTORY - 1) {
			history_current_index[curr_tty] = 0;
		} else {
			++history_current_index[curr_tty];
		}
	}

	uint16_t c = history[curr_tty][history_current_index[curr_tty]][TERMINAL_PROMPT_LEN];
	terminal_column[curr_tty] = TERMINAL_PROMPT_LEN;
	while ((terminal_column[curr_tty] < VGA_WIDTH) && (c & 0x00FF)) {
		terminal_buffer[VGA_WIDTH * (VGA_HEIGHT - 1) + terminal_column[curr_tty]] = c;

		++terminal_column[curr_tty];
		c = history[curr_tty][history_current_index[curr_tty]][terminal_column[curr_tty]];
	}

	for (int i = terminal_column[curr_tty]; i < VGA_WIDTH; ++i) {
		terminal_buffer[VGA_WIDTH * (VGA_HEIGHT - 1) + i] = vga_entry(EMPTY, DEFAULT_COLOR);
	}

	terminal_written_column[curr_tty] = terminal_column[curr_tty];
	update_cursor(terminal_column[curr_tty], terminal_row[curr_tty]);
}

static inline void handle_up_press(void) {
	if (!history_total_index[curr_tty]) {
		return;
	}

	if (history_current_index[curr_tty] == -1) {
		history_current_index[curr_tty] = (history_total_index[curr_tty] - 1) % MAX_HISTORY;
	} else {
		if (history_total_index[curr_tty] <= MAX_HISTORY && history_current_index[curr_tty]) {
			--history_current_index[curr_tty];
		} else if (history_total_index[curr_tty] > MAX_HISTORY
				&& (history_total_index[curr_tty] % MAX_HISTORY) != history_current_index[curr_tty]) {
			if (!history_current_index[curr_tty]) {
				history_current_index[curr_tty] = MAX_HISTORY - 1;
			} else {
				--history_current_index[curr_tty];
			}
		}
	}

	uint16_t c = history[curr_tty][history_current_index[curr_tty]][TERMINAL_PROMPT_LEN];

	terminal_column[curr_tty] = TERMINAL_PROMPT_LEN;
	while ((terminal_column[curr_tty] < VGA_WIDTH) && (c & 0x00FF)) {
		terminal_buffer[VGA_WIDTH * (VGA_HEIGHT - 1) + terminal_column[curr_tty]] = c;

		++terminal_column[curr_tty];
		c = history[curr_tty][history_current_index[curr_tty]][terminal_column[curr_tty]];
	}

	for (int i = terminal_column[curr_tty]; i < VGA_WIDTH; ++i) {
		terminal_buffer[VGA_WIDTH * (VGA_HEIGHT - 1) + i] = vga_entry(' ', DEFAULT_COLOR);
	}

	terminal_written_column[curr_tty] = terminal_column[curr_tty];
	update_cursor(terminal_column[curr_tty], terminal_row[curr_tty]);
}

inline void swap_tty(const uint8_t new_tty) {
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;

			tty[curr_tty][index] = terminal_buffer[index];
			terminal_buffer[index] = tty[new_tty][index];
		}
	}

	curr_tty = new_tty;
	update_cursor(terminal_column[curr_tty], terminal_row[curr_tty]);
}

static inline void display_full_history(const int gap) {
	for (int i = 0; i < VGA_HEIGHT - gap; ++i) {
		for (int j = 0; j < VGA_WIDTH; ++j) {
			terminal_buffer[i * VGA_WIDTH + j] = terminal_buffer[(i + gap) * VGA_WIDTH + j];
		}
	}
}

#define COLOR_COMMAND		"color "
#define COLOR_COMMAND_LEN	6
#define COLOR_MSG	"You are now writing in "

#define GDT_COMMAND			"gdt "
#define GDT_COMMAND_LEN		4
#define GDTR_COMMAND		"gdtr "
#define GDTR_COMMAND_LEN	5

static void write_color_msg(const char * color_str, const uint16_t color) {
	terminal_column[curr_tty] = 0;
	terminal_row[curr_tty] = VGA_HEIGHT - 2;

	terminal_writestring(COLOR_MSG);
	terminal_color[curr_tty] = color;
	terminal_writestring(color_str);

	while (terminal_column[curr_tty] < VGA_WIDTH - 1) {
		terminal_putchar(' ');
	}
}

static void change_color(uint16_t* arg_ptr) {
	const char color_palet[16][14] = COLOR_PALET;

	while (((*arg_ptr) & 0x00FF) == EMPTY && (uint32_t) arg_ptr < TERMINAL_LIMIT) {
		++arg_ptr;
	}

	if ((uint32_t) arg_ptr < TERMINAL_LIMIT) {
		for (int color_index = 0; color_index < 16; ++color_index) {
			const char *	color_str = color_palet[color_index];
			const size_t	color_len = kstrlen(color_str);

			if (((uint32_t) (arg_ptr + color_len) <= TERMINAL_LIMIT) && (kstrncmp(arg_ptr, color_str, color_len) == 0)) {
				const uint16_t color = vga_entry_color(color_index, VGA_COLOR_BLACK);

				display_full_history(2);
				write_color_msg(color_str, color);
				return ;
			}
		}
	}

	display_full_history(2);
	terminal_column[curr_tty] = 0;
	terminal_row[curr_tty] = VGA_HEIGHT - 2;
	terminal_writestring("Invalid color");
	while (terminal_column[curr_tty] < VGA_WIDTH - 1) {
		terminal_putchar(' ');
	}
}

static void print_gdt(void) {
	uint8_t * gdt_ptr = (uint8_t *) &gdt;
	const char hexa_base[] = "0123456789abcdef";
	const uint8_t prev_color = terminal_color[curr_tty];

	terminal_color[curr_tty] = DEFAULT_COLOR;
	display_full_history(1 + (sizeof(GDT_t) * GDT_ENTRIES / 16) + 1);
	terminal_column[curr_tty] = 12;
	terminal_row[curr_tty] = VGA_HEIGHT - 1 - (sizeof(GDT_t) * GDT_ENTRIES / 16) - 1;

	for (int i = 0; i < (sizeof(GDT_t) * GDT_ENTRIES / 16) + 1; ++i) {
		terminal_column[curr_tty] = 0;

		terminal_writestring("0x00");
		terminal_putnbr_base(
			((uint32_t) gdt) + (i * 16), hexa_base, 16,
			((VGA_HEIGHT - 1 - (sizeof(GDT_t) * GDT_ENTRIES / 16) - 1) + i) * VGA_WIDTH + 4) % 16;

		terminal_column[curr_tty] += 6;
		terminal_writestring("  ");
		++terminal_row[curr_tty];
	}

	terminal_column[curr_tty] = 12;
	terminal_row[curr_tty] = VGA_HEIGHT - 1 - (sizeof(GDT_t) * GDT_ENTRIES / 16) - 1;

	for (int i = 0; i < sizeof(GDT_t) * (GDT_ENTRIES + 1); ++i) {
		if ((*(gdt_ptr + i)) < 10) {
			terminal_putchar('0');
		}
		terminal_column[curr_tty] = terminal_putnbr_base(
			(*(gdt_ptr + i)), hexa_base, 16, terminal_row[curr_tty] * VGA_WIDTH + terminal_column[curr_tty]) % VGA_WIDTH;

		if ((*(gdt_ptr + i)) % 16 == 1) {
			terminal_putchar('0');
		}
		if (i && (i + 1) % 8 == 0) {
			terminal_putchar(' ');
		}
		terminal_putchar(' ');

		if ((i + 1) % 16 == 0) {
			++terminal_row[curr_tty];
			terminal_column[curr_tty] = 12;
		}
	}

	terminal_color[curr_tty] = prev_color;
}

static void print_gdtr(void) {
	uint8_t * gdtr_ptr = (uint8_t *) 0x00000800;
	const char hexa_base[] = "0123456789abcdef";
	const uint8_t prev_color = terminal_color[curr_tty];

	terminal_color[curr_tty] = DEFAULT_COLOR;
	display_full_history(2);
	terminal_column[curr_tty] = 0;
	terminal_row[curr_tty] = VGA_HEIGHT - 2;
	terminal_writestring("0x00000800  ");

	for (int i = 0; i < 16; ++i) {
		terminal_column[curr_tty] = terminal_putnbr_base(
			(*(gdtr_ptr + i)), hexa_base, 16, terminal_row[curr_tty] * VGA_WIDTH + terminal_column[curr_tty]) % VGA_WIDTH;
		if ((*(gdtr_ptr + i)) % 16 == 1 || (*(gdtr_ptr + i)) == 0) {
			terminal_putchar('0');
		}
		if (i && (i + 1) % 8 == 0) {
			terminal_putchar(' ');
		}
		terminal_putchar(' ');
	}

	terminal_color[curr_tty] = prev_color;
}

static int check_command(void) {
	size_t index = TERMINAL_PROMPT_LEN;

	while ((terminal_buffer[VGA_WIDTH * (VGA_HEIGHT - 1) + index] & 0x00FF) == EMPTY && index < VGA_WIDTH) {
		++index;
	}

	if (index == VGA_WIDTH) {
		return 0;
	}

	uint16_t * curr_buff = &terminal_buffer[VGA_WIDTH * (VGA_HEIGHT - 1) + index];

	if (index + COLOR_COMMAND_LEN < VGA_WIDTH + 1 && kstrncmp(curr_buff, COLOR_COMMAND, COLOR_COMMAND_LEN) == 0) {
		change_color(curr_buff + COLOR_COMMAND_LEN);
		return 1;
	} else if (index + GDT_COMMAND_LEN < VGA_WIDTH && kstrncmp(curr_buff, GDT_COMMAND, GDT_COMMAND_LEN) == 0) {
		print_gdt();
		return 1;
	} else if (index + GDTR_COMMAND_LEN < VGA_WIDTH && kstrncmp(curr_buff, GDTR_COMMAND, GDTR_COMMAND_LEN) == 0) {
		print_gdtr();
		return 1;
	}

	return 0;
}

static inline void handle_extended_byte(const uint8_t scan_code) {
	switch (scan_code) {
		case EXTENDED_ENTER_PRESS:
			save_to_history();
			if (!check_command()) {
				display_full_history(1);
			}
			terminal_prompt();
			history_current_index[curr_tty] = -1;
			break;
		case DELETE_PRESS:
			delete_next_char();
			break;
		case CURSOR_RIGHT_PRESS:
			if (terminal_written_column[curr_tty] > terminal_column[curr_tty]) {
				move_cursor_right();
			}
			break;
		case CURSOR_LEFT_PRESS:
			if (terminal_column[curr_tty] > TERMINAL_PROMPT_LEN) {
				move_cursor_left();
			}
			break;
		case CURSOR_UP_PRESS:
			handle_up_press();
			break;
		case CURSOR_DOWN_PRESS:
			handle_down_press();
			break;
		default:
			break;
	}
}

void isr_keyboard(void) {
    uint8_t scan_code = inb(0x60);
	uint8_t c = qwerty_keyboard_table[scan_code][shift];
	uint8_t new_tty;

	#ifdef DEBUG 
		kprintf("scan_code: 0x%x/%d\nchar: %c", scan_code, scan_code, c);
	#endif

	if (c) {
		if (maj && ((c >= 'a' && c <= 'z') || c >= 'A' && c <= 'Z')) {
			c = qwerty_keyboard_table[scan_code][!shift];
		}

		terminal_insert_char(c);
	} else {
		switch (scan_code) {
			case ENTER_PRESS:
				save_to_history();
				if (!check_command()) {
					display_full_history(1);
				}
				terminal_prompt();
				history_current_index[curr_tty] = -1;
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
				new_tty = scan_code - F1_PRESSED;
				if (curr_tty != new_tty) {
					swap_tty(new_tty);
				}
				break;
			default:
				break;
		}
	}

    outb(PIC1_COMMAND, 0x20);
}