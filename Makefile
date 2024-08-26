BUILD_DIR		=	build
SRC_DIR			=	src
OBJ_DIR			=	${BUILD_DIR}/obj
ISO_DIR			=	${BUILD_DIR}/iso

KERNEL		=	${BUILD_DIR}/kfs.bin
ISO			=	${BUILD_DIR}/kfs.iso

BOOT_SRC		=	${SRC_DIR}/kernel/boot.asm

KERNEL_SRC		=	${SRC_DIR}/kernel/kernel.c
KEYBOARD_SRC	=	${SRC_DIR}/kernel/keyboard.c
UTILS_SRC		=	${SRC_DIR}/kernel/utils.c

INCLUDES		=	${SRC_DIR}/include

LINKER_SRC		=	${SRC_DIR}/utils/linker.ld
GRUB_SRC		=	${SRC_DIR}/utils/grub.cfg

FLAGS		=	-fno-builtin -fno-exceptions -fno-stack-protector -nostdlib -nodefaultlibs


all: build

build:
	mkdir -p ${OBJ_DIR}
	nasm -f elf32 ${BOOT_SRC} -o ${OBJ_DIR}/boot.o
	gcc -m32 -ffreestanding ${FLAGS} -I${INCLUDES} -c ${KERNEL_SRC} -o ${OBJ_DIR}/kernel.o
	gcc -m32 -ffreestanding ${FLAGS} -I${INCLUDES} -c ${KEYBOARD_SRC} -o ${OBJ_DIR}/keyboard.o
	gcc -m32 -ffreestanding ${FLAGS} -I${INCLUDES} -c ${UTILS_SRC} -o ${OBJ_DIR}/utils.o
	ld -m elf_i386 -T ${LINKER_SRC} -o ${KERNEL} ${OBJ_DIR}/boot.o ${OBJ_DIR}/kernel.o ${OBJ_DIR}/keyboard.o ${OBJ_DIR}/utils.o

run: build
	qemu-system-i386 -kernel ${KERNEL}

iso: build
	mkdir -p ${ISO_DIR}/boot/grub
	cp ${GRUB_SRC} ${ISO_DIR}/boot/grub
	cp ${KERNEL} ${ISO_DIR}/boot
	grub-mkrescue -o ${ISO} ${ISO_DIR}

run-iso: iso
	qemu-system-i386 -cdrom ${ISO}

clean:
	rm -rf ${KERNEL} ${ISO}

fclean:
	clear
	rm -rf build

re: fclean all

.PHONY: all clean fclean re