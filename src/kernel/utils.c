#include <stdarg.h>

#include "kernel.h"
#include "utils.h"


static IDTR_t	idt_register;
static IDT_t	idt[IDT_ENTRIES];
GDTR_t *	gdt_register = (GDTR_t *) 0x00000800;
GDT_t		gdt[GDT_ENTRIES];

inline void outb(const uint16_t port, const uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port) : "memory");
}

inline uint8_t inb(uint16_t port) {
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
	// ICW3: tell Slave PIC its cascade identity (0000 0010)
	outb(PIC2_DATA, 2);
	io_wait();
	// ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
	outb(PIC1_DATA, 4);
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

void load_idt(void) {
    idt_register.size = (sizeof(IDT_t) * IDT_ENTRIES) - 1;
	idt_register.idt = (uint32_t) &idt;

    __asm__ volatile ("lidt %0" : : "m"(idt_register));
}


void set_gdt_entry(const int index, const uint32_t base, const uint32_t limit,
		const uint8_t access, const uint8_t granularity) {
    gdt[index].base_low = (base & 0xFFFF);
    gdt[index].base_middle = (base >> 16) & 0xFF;
    gdt[index].base_high = (base >> 24) & 0xFF;
    gdt[index].limit_low = (limit & 0xFFFF);
    gdt[index].granularity = (limit >> 16) & 0x0F;
    gdt[index].granularity |= (granularity & 0xF0);
    gdt[index].access = access;
}


void init_gdt() {
	const uint32_t	base = 0;
    const uint32_t	limit = 0xFFFFF;


	gdt_register->size = (sizeof(GDT_t) * GDT_ENTRIES) - 1;
    gdt_register->gdt = (uint32_t)&gdt;

    // Segment 0: NULL Segment, mandatory
    set_gdt_entry(0, 0, 0, 0, 0);

    // Segment 1: Kernel code
    set_gdt_entry(1, base, limit, 0x9A, 0xCF);

    // Segment 2: Kernel data
    set_gdt_entry(2, base, limit, 0x92, 0xCF);

	// Segment 3: Kernel stack, same as Kernel data
    set_gdt_entry(3, base, limit, 0x92, 0xCF);

    // Segment 4: User code
    set_gdt_entry(4, base, limit, 0xFA, 0xCF);

    // Segment 5: User data
    set_gdt_entry(5, base, limit, 0xF2, 0xCF);

	// Segment 6: User stack, same as User data
    set_gdt_entry(6, base, limit, 0xF2, 0xCF);

    // Load the new GDT
    __asm__ volatile ("lgdt %0" : : "m"(*gdt_register));
	// Reload the segment registers to the new Kernel Data segment:  index 2 = 0x10
    __asm__ volatile (
        "mov $0x10, %ax\n"
        "mov %ax, %ds\n"
        "mov %ax, %es\n"
        "mov %ax, %fs\n"
        "mov %ax, %gs\n"
        "mov %ax, %ss\n"
		// Jump to provoke a segment change to make sure the segment registers are reloaded properly
        "jmp $0x08, $.1\n"
        ".1:\n"
    );
}

inline void update_cursor(size_t x, size_t y) {
	uint16_t pos = y * VGA_WIDTH + x;

	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

inline void move_cursor_left(void) {
	if (terminal_column[curr_tty] > 0) {
		--terminal_column[curr_tty];
		update_cursor(terminal_column[curr_tty], terminal_row[curr_tty]);
	}
}

inline void move_cursor_right(void) {
	if (terminal_column[curr_tty] < VGA_WIDTH - 1) {
		++terminal_column[curr_tty];
		update_cursor(terminal_column[curr_tty], terminal_row[curr_tty]);
	}
}

size_t terminal_putnbr_base(int n, const char* base, const size_t base_len, size_t pos) {
	long nb = n;

	if (nb < 0) {
		terminal_putentryat('-', DEFAULT_COLOR, pos % VGA_WIDTH, pos / VGA_WIDTH);
		++pos;
		nb *= -1;
	}

	if (nb >= base_len) {
		pos = terminal_putnbr_base((int)(nb / base_len), base, base_len, pos);
	}

	terminal_putentryat(base[(nb % base_len)], DEFAULT_COLOR, pos % VGA_WIDTH, pos / VGA_WIDTH);
	return ++pos;
}

void kmemset(void* ptr, const int8_t value, const size_t num) {
	int8_t* c_ptr = ptr;

	for (size_t i = 0; i < num; ++i) {
		c_ptr[i] = value;
	}
}

void kprintf(const char* format, ...) {
	va_list	va_params;
	size_t	buffer_index = 0;
	char	c, f;
	const char *s;

	va_start(va_params, format);
	for (int i = 0; format[i]; ++i) {
		c = format[i];
		if (c == '%') {
			++i;
			if (!(f = format[i])) {
				break;
			}
			switch (f) {
				case '%':
					terminal_putentryat('%', DEFAULT_COLOR, buffer_index % VGA_WIDTH, buffer_index / VGA_WIDTH);
					++buffer_index;
					break;
				case 'c':
					terminal_putentryat((char) va_arg(va_params, int), DEFAULT_COLOR, buffer_index % VGA_WIDTH, buffer_index / VGA_WIDTH);
					++buffer_index;
					break;
				case 'd':
					buffer_index = terminal_putnbr_base((int) va_arg(va_params, int), "0123456789", 10, buffer_index);
					break;
				case 'x':
					buffer_index = terminal_putnbr_base((int) va_arg(va_params, int), "0123456789abcdef", 16, buffer_index);
					break;
				case 'X':
					buffer_index = terminal_putnbr_base((int) va_arg(va_params, int), "0123456789ABCDEF", 16, buffer_index);
					break;
				case 's':
					s = (char *) va_arg(va_params, char *);
					for (int s_i = 0; s[s_i]; ++s_i) {
						terminal_putentryat(s[s_i], DEFAULT_COLOR, buffer_index % VGA_WIDTH, buffer_index / VGA_WIDTH);
						++buffer_index;
					}
					break;
				default:
					break;
			}
		} else if (c == '\n') {
			buffer_index -= buffer_index % VGA_WIDTH;
			buffer_index += VGA_WIDTH;
		} else {
			terminal_putentryat(c, DEFAULT_COLOR, buffer_index % VGA_WIDTH, buffer_index / VGA_WIDTH);
			++buffer_index;
		}
	}
	va_end(va_params);

	while (buffer_index % VGA_WIDTH) {
		terminal_putentryat(EMPTY, DEFAULT_COLOR, buffer_index % VGA_WIDTH, buffer_index / VGA_WIDTH);
		++buffer_index;
	}
}

void* kmemcpy(void *dest, const void *src, size_t n) {
	size_t				i = -1;
	unsigned char		*destcpy = dest;
	const unsigned char	*srccpy = src;

	while (++i < n) {
		destcpy[i] = srccpy[i];
	}

	return (dest);
}

int	kstrncmp(const uint16_t *s1, const char *s2, const size_t n) {
	size_t	i = 0;

	if (n == 0) {
		return 0;
	}

	while ((unsigned char) (s1[i] & 0x00FF) && (unsigned char) s2[i]
		&& (unsigned char) (s1[i] & 0x00FF) == (unsigned char) s2[i] && i < n - 1)
		++i;

	return ((unsigned char) (s1[i] & 0x00FF) - (unsigned char) s2[i]);
}