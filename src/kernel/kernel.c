#include <stdbool.h>

#include "kernel.h"
#include "keyboard.h"
#include "utils.h"


size_t		terminal_row[MAX_TTY];
size_t		terminal_column[MAX_TTY];
size_t		terminal_written_column[MAX_TTY];
uint8_t		terminal_color[MAX_TTY];
uint16_t*	terminal_buffer;
uint16_t	tty[MAX_TTY][VGA_WIDTH * VGA_HEIGHT];
uint8_t		curr_tty;


inline uint8_t vga_entry_color(const enum vga_color fg, const enum vga_color bg) {
	return fg | bg << 4;
}

inline uint16_t vga_entry(const unsigned char uc, const uint8_t color) {
	return (uint16_t) uc | (uint16_t) color << 8;
}

size_t strlen(const char* str) {
	size_t len = 0;

	while (str[len])
		len++;
	return len;
}

void terminal_initialize(void) {
	for (int i = 0; i < MAX_TTY; ++i) {
		terminal_row[i] = 0;
		terminal_column[i] = 0;
		terminal_written_column[i] = 0;
		terminal_color[i] = DEFAULT_COLOR;
	}

	curr_tty = 0;
	terminal_buffer = (uint16_t*) 0xB8000; // Reserved address of VGA to store text to display

	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;

			terminal_buffer[index] = vga_entry(EMPTY, terminal_color[curr_tty]);

			for (int i = 0; i < MAX_TTY; ++i) {
				tty[i][index] = vga_entry(EMPTY, terminal_color[curr_tty]);
			}
		}
	}

	init_history();
	init_colors();
	for (int8_t tmp_tty = MAX_TTY - 1; tmp_tty >= 0; --tmp_tty) {
		terminal_prompt();
		swap_tty(tmp_tty);
	}
}

static inline void terminal_setcolor(const uint8_t color) {
	terminal_color[curr_tty] = color;
}

inline void terminal_putentryat(const char c, const uint8_t color, const size_t x, const size_t y) {
	const size_t index = y * VGA_WIDTH + x;

	terminal_buffer[index] = vga_entry(c, color);
}

inline void terminal_putchar(const char c) {
	terminal_putentryat(c, terminal_color[curr_tty], terminal_column[curr_tty], terminal_row[curr_tty]);
	move_cursor_right();
}

inline void terminal_insert_char(const char c) {
	if (terminal_written_column[curr_tty] < VGA_WIDTH) {
		if (terminal_column[curr_tty] < terminal_written_column[curr_tty]) {
			size_t written_index = terminal_row[curr_tty] * VGA_WIDTH + terminal_written_column[curr_tty];
			size_t x = terminal_written_column[curr_tty];

			for (; x > terminal_column[curr_tty]; --x, --written_index) {
				terminal_putentryat(terminal_buffer[written_index - 1], terminal_color[curr_tty], x, terminal_row[curr_tty]);
			}
		}
		terminal_putentryat(c, terminal_color[curr_tty], terminal_column[curr_tty], terminal_row[curr_tty]);
		++terminal_written_column[curr_tty];
		move_cursor_right();
	}
}

static inline void terminal_write(const char* data, const size_t size) {
	for (size_t i = 0; i < size; i++) {
		terminal_putchar(data[i]);
	}
}

inline void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}

void kmain(void) {
	// Deactivate interruptions while kernel starts
	__asm__ volatile ("cli");

	/* Initialize terminal interface */
	terminal_initialize();

	initialize_idt();
	load_idt();
	PIC_remap();

	// Restore interruptions
	__asm__ volatile ("sti");

	for (;;);
}