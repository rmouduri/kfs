%define ALIGN     (1<<0)
%define MEMINFO   (1<<1)
%define FLAGS     (ALIGN | MEMINFO)
%define MAGIC     0x1BADB002
%define CHECKSUM  -(MAGIC + FLAGS)

section .multiboot
align 4
dd MAGIC
dd FLAGS
dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
bits 32

global _start
global isr_wrapper
extern kmain
extern isr_keyboard

isr_wrapper:
    pushad ; Save current registeries
    cld ; Make string operations from left to right so isr_keyboards works correctly
    call isr_keyboard
    popad ; Restore saved registeries
    iretd ; Load previous state before the interruption

_start:
    mov esp, stack_top
    call kmain
    hlt
