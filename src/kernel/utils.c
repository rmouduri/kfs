#include "kernel.h"
#include "utils.h"


static IDTR_t	idt_register;
static IDT_t	idt[IDT_ENTRIES];

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

inline void terminal_putnbr_base(int n, char* base, size_t base_len) {
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

void kmemset(void* ptr, const int8_t value, const size_t num) {
	int8_t* c_ptr = ptr;

	for (size_t i = 0; i < num; ++i) {
		c_ptr[i] = value;
	}
}