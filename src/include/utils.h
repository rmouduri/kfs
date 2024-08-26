#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifndef _UTILS_H_
# define _UTILS_H_

void update_cursor(size_t x, size_t y);
void move_cursor_left();
void move_cursor_right();

void outb(const uint16_t port, const uint8_t val);
uint8_t inb(uint16_t port);
void PIC_remap(void);
void initialize_idt(void);
void load_idt();
void kmemset(void* ptr, const int8_t value, const size_t num);

#endif // _UTILS_H_