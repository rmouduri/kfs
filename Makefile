NAME						:= kfs
TARGET 					:= i386-elf

###############################################################################
#####   Sources                                                           #####
###############################################################################

INC_DIR					:= include
SRC_DIR		 			:= src
BUILD_DIR				:= build

CXX_SRCS					:=\
	$(SRC_DIR)/kernel/kernel.cpp \
	$(SRC_DIR)/kernel/keyboard.cpp \
	$(SRC_DIR)/kernel/utils.cpp

ASM_SRCS					:=\
	$(SRC_DIR)/kernel/boot.asm

OBJS						:=	\
	$(ASM_SRCS:$(SRC_DIR)/%.asm=$(BUILD_DIR)/%.o) \
	$(CXX_SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o) 

GRUB_CFG					:= grub.cfg

###############################################################################
#####   Instructions                                                      #####
###############################################################################

CXX 						:=\
	$(TARGET)-gcc
CXXFLAGS 				:=\
	-ffreestanding -nostdlib -nostartfiles -nodefaultlibs -fno-exceptions -fno-rtti
DEPFLAGS 				:=\
	-MMD
DEPS						:= $(OBJS:.o=.d)

ASM 						:= nasm
ASMFLAGS 				:= -f elf32

LD 						:=\
	$(TARGET)-ld
LDFLAGS 					:=\
	-n -m elf_i386 -T linker.ld

###############################################################################
#####   Commands                                                          #####
###############################################################################

all: build

$(NAME).iso: $(NAME).bin
	mkdir -p iso/boot/grub
	cp $(NAME).bin iso/boot
	cp $(GRUB_CFG) iso/boot/grub
	grub-mkrescue -o $(NAME).iso iso

$(NAME).bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I $(INC_DIR) $(DEPFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	mkdir -p $(dir $@)
	$(ASM) $(ASMFLAGS) $< -o $@

docker:
	docker build cross_compiler/. -t cross_compiler

build: docker
	docker run -v "${PWD}":/workspace cross_compiler make $(NAME).iso

run: build
	qemu-system-i386 -cdrom $(NAME).iso

clean:
	rm -f $(OBJS) $(DEPS)

fclean: clean
	rm -f $(NAME).bin $(NAME).iso iso/$(NAME).iso iso/boot/$(NAME).bin iso/boot/grub/$(GRUB_CFG)

re: fclean all

.PHONY: all clean fclean re

-include $(DEPS)
