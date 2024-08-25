#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define EMPTY	' '

#define PIC1			0x20		/* IO base address for master PIC */
#define PIC2			0xA0		/* IO base address for slave PIC */
#define IRQ_START		0x20
#define PIC1_IDT_ENTRY	IRQ_START		/* Default PIC1 offset IRQ0 to IRQ7 (0x20 to 0x27) */
#define PIC2_IDT_ENTRY	IRQ_START + 8	/* Default PIC2 offset IRQ8 to IRQ15 (0x28 to 0x2F) */
#define PIC1_COMMAND	PIC1
#define PIC1_DATA		(PIC1+1)
#define PIC2_COMMAND	PIC2
#define PIC2_DATA		(PIC2+1)

/* reinitialize the PIC controllers, giving them specified vector offsets
   rather than 8h and 70h, as configured by default */
#define ICW1_ICW4		0x01		/* Indicates that ICW4 will be present */
#define ICW1_SINGLE		0x02		/* Single (cascade) mode */
#define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
#define ICW1_LEVEL		0x08		/* Level triggered (edge) mode */
#define ICW1_INIT		0x10		/* Initialization - required! */

#define ICW4_8086		0x01		/* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO		0x02		/* Auto (normal) EOI */
#define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
#define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
#define ICW4_SFNM		0x10		/* Special fully nested (not) */

#define IDT_ENTRIES		256
#define KEYBOARD_INTERRUPT_IRQ	1
#define GDT_CODE_SEGMENT	0x8
#define TYPE_INTERRUPT_GATE 0xE
#define DPL_KERNEL 0x0			// full privileges
#define P_PRESENT 0b10000000	// bit 7 set to 1
#define DEFAULT_FLAG	TYPE_INTERRUPT_GATE | DPL_KERNEL | P_PRESENT

#define TERMINAL_PROMPT			"kfs> "
#define TERMINAL_PROMPT_SIZE	strlen(TERMINAL_PROMPT)


typedef struct InterruptDescriptorRegister32 {
	uint16_t	size;
	uint32_t	idt;
} __attribute__((packed)) IDTR_t;

typedef struct InterruptDescriptor32 {
	uint16_t	offset_1;        // offset bits 0..15
	uint16_t	selector;        // a code segment selector in GDT or LDT
	uint8_t		zero;            // unused, set to 0
	uint8_t		type_attributes; // gate type, dpl, and p fields
	uint16_t	offset_2;        // offset bits 16..31
} __attribute__((packed)) IDT_t;

/* Hardware text mode color constants. */
enum vga_color {
	VGA_COLOR_BLACK = 0,
	VGA_COLOR_BLUE = 1,
	VGA_COLOR_GREEN = 2,
	VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4,
	VGA_COLOR_MAGENTA = 5,
	VGA_COLOR_BROWN = 6,
	VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8,
	VGA_COLOR_LIGHT_BLUE = 9,
	VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11,
	VGA_COLOR_LIGHT_RED = 12,
	VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_LIGHT_BROWN = 14,
	VGA_COLOR_WHITE = 15,
};

// void terminal_writestring(const char* data);
extern void isr_wrapper();

static const size_t	VGA_WIDTH = 80;
static const size_t	VGA_HEIGHT = 25;

size_t		terminal_row;
size_t		terminal_column;
size_t		terminal_written_column;
uint8_t		terminal_color;
uint16_t*	terminal_buffer;
bool		lshift = false;
bool		rshift = false;
bool		shift = false;
bool		maj = false;
bool		rdy_to_disable_maj = false;

IDTR_t	idt_register;
IDT_t	idt[IDT_ENTRIES];


static inline void outb(const uint16_t port, const uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %w1, %b0"
                   : "=a"(ret)
                   : "Nd"(port)
                   : "memory");
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

void PIC_remap(void) {
	// starts the initialization sequence (in cascade mode)
	outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();
	outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);
	io_wait();

	// ICW2: Master PIC vector offset
	outb(PIC1_DATA, PIC1_IDT_ENTRY);
	io_wait();
	// ICW2: Slave PIC vector offset
	outb(PIC2_DATA, PIC2_IDT_ENTRY);
	io_wait();

	// IRQ2 is only used to cascade, so we connect slave (PIC2) to irq2 and tell master (PIC1)
	// ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	outb(PIC1_DATA, 4);
	io_wait();
	// ICW3: tell Slave PIC its cascade identity (0000 0010)
	outb(PIC2_DATA, 2);
	io_wait();

	// ICW4: have the PICs use 8086 mode (and not 8080 mode)
	outb(PIC1_DATA, ICW4_8086);
	io_wait();
	outb(PIC2_DATA, ICW4_8086);
	io_wait();

	// Mask all interrupts except keyboard
	outb(PIC1_DATA, 0xFD);
	io_wait();
	outb(PIC2_DATA, 0xFF);
	io_wait();
}

void initialize_idt(void) {
	idt[IRQ_START + KEYBOARD_INTERRUPT_IRQ].offset_1 = (uintptr_t) isr_wrapper & 0xFFFF;
	idt[IRQ_START + KEYBOARD_INTERRUPT_IRQ].selector = GDT_CODE_SEGMENT;
	idt[IRQ_START + KEYBOARD_INTERRUPT_IRQ].zero = 0;
	idt[IRQ_START + KEYBOARD_INTERRUPT_IRQ].type_attributes = DEFAULT_FLAG;
	idt[IRQ_START + KEYBOARD_INTERRUPT_IRQ].offset_2 = ((uintptr_t) isr_wrapper >> 16) & 0xFFFF;
}

void load_idt() {
    idt_register.size = (sizeof(IDT_t) * IDT_ENTRIES) - 1;
	idt_register.idt = (uint32_t) &idt;

    __asm__ volatile ("lidt %0" : : "m"(idt_register));
}

static inline
uint8_t vga_entry_color(const enum vga_color fg, const enum vga_color bg) {
	return fg | bg << 4;
}

static inline
uint16_t vga_entry(const unsigned char uc, const uint8_t color) {
	return (uint16_t) uc | (uint16_t) color << 8;
}

size_t strlen(const char* str) {
	size_t len = 0;

	while (str[len])
		len++;
	return len;
}

void terminal_initialize(void) {
	terminal_row = 0;
	terminal_column = 0;
	terminal_written_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = (uint16_t*) 0xB8000; // Reserved address of VGA to store text to display

	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;

			terminal_buffer[index] = vga_entry(EMPTY, terminal_color);
		}
	}
}

static inline void terminal_setcolor(const uint8_t color) {
	terminal_color = color;
}

static inline void terminal_putentryat(const char c, const uint8_t color, const size_t x, const size_t y) {
	const size_t index = y * VGA_WIDTH + x;

	terminal_buffer[index] = vga_entry(c, color);
}

static inline void update_cursor(size_t x, size_t y) {
	uint16_t pos = y * VGA_WIDTH + x;

	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

static inline void move_cursor_left() {
	if (terminal_column > TERMINAL_PROMPT_SIZE) {
		--terminal_column;
		update_cursor(terminal_column, terminal_row);
	}
}

static inline void move_cursor_right() {
	if (terminal_column < VGA_WIDTH - 1) {
		++terminal_column;
		update_cursor(terminal_column, terminal_row);
	}
}

static inline void terminal_putchar(const char c) {
	terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	move_cursor_right();
}

static inline void terminal_insert_char(const char c) {
	if (terminal_written_column < VGA_WIDTH) {
		if (terminal_column < terminal_written_column) {
			size_t written_index = terminal_row * VGA_WIDTH + terminal_written_column;
			size_t x = terminal_written_column;

			for (; x > terminal_column; --x, --written_index) {
				terminal_putentryat(terminal_buffer[written_index - 1], terminal_color, x, terminal_row);
			}
		}
		terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
		++terminal_written_column;
		move_cursor_right();
	}
}

static inline void delete_last_char() {
	if (terminal_column == TERMINAL_PROMPT_SIZE) {
		return;
	}

	size_t index = terminal_row * VGA_WIDTH + terminal_column;
	size_t x = terminal_column;

	for (; x < VGA_WIDTH; ++x, ++index) {
		terminal_putentryat(terminal_buffer[index], terminal_color, x - 1, terminal_row);
	}
	terminal_putentryat(EMPTY, terminal_color, x - 1, terminal_row);
	--terminal_written_column;
	move_cursor_left();
}

static inline void delete_next_char() {
	if (terminal_column >= terminal_written_column) {
		return;
	}

	size_t index = terminal_row * VGA_WIDTH + terminal_column;
	size_t x = terminal_column;

	for (; x < VGA_WIDTH - 1; ++x, ++index) {
		terminal_putentryat(terminal_buffer[index + 1], terminal_color, x, terminal_row);
	}
	terminal_putentryat(EMPTY, terminal_color, x, terminal_row);
	--terminal_written_column;
}

static inline void terminal_write(const char* data, const size_t size) {
	for (size_t i = 0; i < size; i++) {
		terminal_putchar(data[i]);
	}
}

static inline void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}

static inline void terminal_putnbr_base(int n, char* base, size_t base_len) {
	long nb = n;

	if (nb < 0) {
		terminal_putchar('-');
		nb *= -1;
	}

	if (nb >= base_len) {
		terminal_putnbr_base((int)(nb / base_len), base, base_len);
	}

	terminal_putchar(base[(nb % base_len)]);
}

void terminal_prompt(void) {
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
	terminal_column = 0;
	terminal_row = VGA_HEIGHT - 1;

	terminal_writestring("kfs> ");

	terminal_written_column = terminal_column;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

// char qwerty_keyboard_table[128][2] = {
#define QWERTY_KEYBOARD_TABLE { \
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

char qwerty_keyboard_table[128][2] = QWERTY_KEYBOARD_TABLE;

#define LSHIFT_PRESS	0x2A
#define LSHIFT_RELEASE	0xAA
#define RSHIFT_PRESS	0x36
#define RSHIFT_RELEASE	0xB6

#define CAPSLOCK_PRESS		0x3A
#define CAPSLOCK_RELEASE	0xBA

#define BACKSPACE_PRESS	0x0E


#define EXTENDED_BYTE	0xE0
/* Extended Bytes sent after 0xE0 */
#define DELETE_PRESS 	0x53

#define CURSOR_RIGHT_PRESS	0x4d
#define CURSOR_LEFT_PRESS	0x4B
#define CURSOR_UP_PRESS		0x48
#define CURSOR_DOWN_PRESS	0x50


static inline void handle_extended_byte(const uint8_t scan_code) {
	switch (scan_code) {
		case DELETE_PRESS:
			delete_next_char();
			break;
		case CURSOR_RIGHT_PRESS:
			if (terminal_written_column > terminal_column) {
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
			default:
				break;
		}
	}

    outb(PIC1_COMMAND, 0x20);
}

void kmain(void) {
	// Deactivate interruptions while kernel starts
	__asm__ volatile ("cli");

	/* Initialize terminal interface */
	terminal_initialize();

	initialize_idt();
	load_idt();
	PIC_remap();

	terminal_prompt();

	// Restore interruptions
	__asm__ volatile ("sti");

	for (;;);
}