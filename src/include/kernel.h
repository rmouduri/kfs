#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef _KERNEL_H_
# define _KERNEL_H_

# define EMPTY	' '

# define PIC1			0x20		/* IO base address for master PIC */
# define PIC2			0xA0		/* IO base address for slave PIC */
# define IRQ_START		0x20
# define PIC1_IDT_ENTRY	IRQ_START		/* Default PIC1 offset IRQ0 to IRQ7 (0x20 to 0x27) */
# define PIC2_IDT_ENTRY	IRQ_START + 8	/* Default PIC2 offset IRQ8 to IRQ15 (0x28 to 0x2F) */
# define PIC1_COMMAND	PIC1
# define PIC1_DATA		(PIC1+1)
# define PIC2_COMMAND	PIC2
# define PIC2_DATA		(PIC2+1)

/* reinitialize the PIC controllers, giving them specified vector offsets
   rather than 8h and 70h, as configured by default */
# define ICW1_ICW4		0x01		/* Indicates that ICW4 will be present */
# define ICW1_SINGLE		0x02		/* Single (cascade) mode */
# define ICW1_INTERVAL4	0x04		/* Call address interval 4 (8) */
# define ICW1_LEVEL		0x08		/* Level triggered (edge) mode */
# define ICW1_INIT		0x10		/* Initialization - required! */

# define ICW4_8086		0x01		/* 8086/88 (MCS-80/85) mode */
# define ICW4_AUTO		0x02		/* Auto (normal) EOI */
# define ICW4_BUF_SLAVE	0x08		/* Buffered mode/slave */
# define ICW4_BUF_MASTER	0x0C		/* Buffered mode/master */
# define ICW4_SFNM		0x10		/* Special fully nested (not) */

# define IDT_ENTRIES		256
# define GDT_ENTRIES		7
# define KEYBOARD_INTERRUPT_IRQ	1
# define GDT_CODE_SEGMENT	0x8
# define TYPE_INTERRUPT_GATE 0xE
# define DPL_KERNEL 0x0			// full privileges
# define P_PRESENT 0b10000000	// bit 7 set to 1
# define DEFAULT_FLAG	TYPE_INTERRUPT_GATE | DPL_KERNEL | P_PRESENT

# define VGA_WIDTH		80
# define VGA_HEIGHT		25
# define MAX_TTY		10
# define MAX_HISTORY	32
# define TERMINAL_PROMPT		"kfs> "
# define TERMINAL_PROMPT_LEN	5 // strlen of TERMINAL_PROMPT

typedef struct InterruptDescriptorRegister32 {
	uint16_t	size;
	uint32_t	idt;
} __attribute__((packed)) IDTR_t;

typedef struct InterruptDescriptor32 {
	uint16_t	offset_1;			// offset bits 0..15
	uint16_t	selector;			// a code segment selector in GDT or LDT
	uint8_t		zero;				// unused, set to 0
	uint8_t		type_attributes;	// gate type, dpl, and p fields
	uint16_t	offset_2;			// offset bits 16..31
} __attribute__((packed)) IDT_t;

typedef struct GlobalDescriptorRegister32 {
	uint16_t	size;
	uint32_t	gdt;
} __attribute__((packed)) GDTR_t;

typedef struct GlobalDescriptor32 {
	uint16_t	limit_low;		// Limite (16 bits)
    uint16_t	base_low;		// Base (16 bits)
    uint8_t		base_middle;	// Base (8 bits)
    uint8_t		access;			// Drapeaux d'accès
    uint8_t		granularity;	// Granularité
    uint8_t		base_high;		// Base (8 bits)
} __attribute__((packed)) GDT_t;

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

# define DEFAULT_COLOR	vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK)

extern size_t		terminal_row[MAX_TTY];
extern size_t		terminal_column[MAX_TTY];
extern size_t		terminal_written_column[MAX_TTY];
extern uint8_t		terminal_color[MAX_TTY];
extern uint16_t*	terminal_buffer;
extern uint16_t		tty[MAX_TTY][VGA_WIDTH * VGA_HEIGHT];
extern uint8_t		curr_tty;


extern void isr_wrapper();

void terminal_putentryat(const char c, const uint8_t color, const size_t x, const size_t y);
void terminal_putchar(const char c);
void terminal_insert_char(const char c);
void terminal_writestring(const char* data);
uint8_t vga_entry_color(const enum vga_color fg, const enum vga_color bg);
uint16_t vga_entry(const unsigned char uc, const uint8_t color);

#endif // _KERNEL_H_