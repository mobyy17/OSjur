# Compiler & linker
ASM           = nasm
LIN           = ld
CC            = gcc

# Directory
SOURCE_FOLDER = src
OUTPUT_FOLDER = bin
ISO_NAME      = OS2025
DISK_NAME     = storage

# Flags
WARNING_CFLAG = -Wall -Wextra -Werror
DEBUG_CFLAG   = -fshort-wchar -g
STRIP_CFLAG   = -nostdlib -fno-stack-protector -nostartfiles -nodefaultlibs -ffreestanding
CFLAGS        = $(DEBUG_CFLAG) $(WARNING_CFLAG) $(STRIP_CFLAG) -m32 -c -I$(SOURCE_FOLDER)
AFLAGS        = -f elf32 -g -F dwarf
LFLAGS        = -T $(SOURCE_FOLDER)/linker.ld -melf_i386

.PHONY: run start all build disk clean kernel iso inserter user-shell insert-shell user-hello user-clock insert-apps \
	run-audio user-badapple badapple-data insert-badapple

run: all
	@qemu-system-i386 -s -S -drive file=$(OUTPUT_FOLDER)/$(DISK_NAME).bin,format=raw,if=ide,index=0,media=disk -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso

start: all
	@qemu-system-i386 -drive file=$(OUTPUT_FOLDER)/$(DISK_NAME).bin,format=raw,if=ide,index=0,media=disk -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso

# Sama seperti start, ditambah suara PC speaker (bonus). Ganti pa dengan alsa/sdl kalau backend pa tidak ada.
AUDIO_BACKEND ?= pa
run-audio: all
	@qemu-system-i386 -drive file=$(OUTPUT_FOLDER)/$(DISK_NAME).bin,format=raw,if=ide,index=0,media=disk -cdrom $(OUTPUT_FOLDER)/$(ISO_NAME).iso \
		-audiodev $(AUDIO_BACKEND),id=spk -machine pcspk-audiodev=spk

all: build

build: iso

disk:
	@qemu-img create -f raw $(OUTPUT_FOLDER)/$(DISK_NAME).bin 4M

clean:
	rm -rf $(OUTPUT_FOLDER)/*.o $(OUTPUT_FOLDER)/*.iso $(OUTPUT_FOLDER)/kernel
	rm -f $(OUTPUT_FOLDER)/inserter $(OUTPUT_FOLDER)/shell $(OUTPUT_FOLDER)/hello $(OUTPUT_FOLDER)/clock
	rm -f $(OUTPUT_FOLDER)/badapple

kernel:
	@mkdir -p $(OUTPUT_FOLDER)
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/kernel-entrypoint.s -o $(OUTPUT_FOLDER)/kernel-entrypoint.o
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/intsetup.s -o $(OUTPUT_FOLDER)/intsetup.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/cpu/gdt.c -o $(OUTPUT_FOLDER)/gdt.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/cpu/idt.c -o $(OUTPUT_FOLDER)/idt.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/cpu/interrupt.c -o $(OUTPUT_FOLDER)/interrupt.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/cpu/portio.c -o $(OUTPUT_FOLDER)/portio.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/driver/keyboard.c -o $(OUTPUT_FOLDER)/keyboard.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/driver/disk.c -o $(OUTPUT_FOLDER)/disk.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/filesystem/ext2.c -o $(OUTPUT_FOLDER)/ext2.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/text/framebuffer.c -o $(OUTPUT_FOLDER)/framebuffer.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/stdlib/string.c -o $(OUTPUT_FOLDER)/string.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/memory/paging.c -o $(OUTPUT_FOLDER)/paging.o
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/context-switch.s -o $(OUTPUT_FOLDER)/context-switch.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/driver/cmos.c -o $(OUTPUT_FOLDER)/cmos.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/process/process.c -o $(OUTPUT_FOLDER)/process.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/scheduler/scheduler.c -o $(OUTPUT_FOLDER)/scheduler.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/cpu/syscall-ext.c -o $(OUTPUT_FOLDER)/syscall-ext.o
	@$(CC) $(CFLAGS) $(SOURCE_FOLDER)/kernel.c -o $(OUTPUT_FOLDER)/kernel.o
	@$(LIN) $(LFLAGS) $(OUTPUT_FOLDER)/*.o -o $(OUTPUT_FOLDER)/kernel
	@echo Linking object files and generate elf32...

iso: kernel
	@mkdir -p $(OUTPUT_FOLDER)/iso/boot/grub
	@cp $(OUTPUT_FOLDER)/kernel     $(OUTPUT_FOLDER)/iso/boot/
	@cp other/grub1                 $(OUTPUT_FOLDER)/iso/boot/grub/
	@cp $(SOURCE_FOLDER)/menu.lst   $(OUTPUT_FOLDER)/iso/boot/grub/
	@genisoimage -R -b boot/grub/grub1 -no-emul-boot -boot-load-size 4 -A os -input-charset utf8 -quiet -boot-info-table -o $(OUTPUT_FOLDER)/$(ISO_NAME).iso $(OUTPUT_FOLDER)/iso
	@echo Generated iso file...
	@rm -r $(OUTPUT_FOLDER)/iso/

inserter:
	@$(CC) -Wno-builtin-declaration-mismatch -g -I$(SOURCE_FOLDER) \
		$(SOURCE_FOLDER)/stdlib/string.c \
		$(SOURCE_FOLDER)/filesystem/ext2.c \
		$(SOURCE_FOLDER)/external/external-inserter.c \
		-o $(OUTPUT_FOLDER)/inserter
	@echo Inserter ready: $(OUTPUT_FOLDER)/inserter

user-shell:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-shell.c $(SOURCE_FOLDER)/shell/*.c \
		$(SOURCE_FOLDER)/stdlib/string.c
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-shell.o shell-*.o cmd-*.o string.o -o $(OUTPUT_FOLDER)/shell
	@echo Linking object shell object files and generate flat binary...
	@size --target=binary $(OUTPUT_FOLDER)/shell
	@rm -f *.o

insert-shell: inserter user-shell
	@echo Inserting shell into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter -f shell 1 $(DISK_NAME).bin

# Aplikasi user Ch.4: tiap aplikasi = crt0 + satu file C, tanpa library shell
user-hello:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-hello.c -o user-hello.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-hello.o -o $(OUTPUT_FOLDER)/hello
	@size --target=binary $(OUTPUT_FOLDER)/hello
	@rm -f crt0.o user-hello.o

user-clock:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-clock.c -o user-clock.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-clock.o -o $(OUTPUT_FOLDER)/clock
	@size --target=binary $(OUTPUT_FOLDER)/clock
	@rm -f crt0.o user-clock.o

insert-apps: inserter user-hello user-clock
	@echo Inserting apps into root directory...
	@cd $(OUTPUT_FOLDER); ./inserter -f hello 1 $(DISK_NAME).bin
	@cd $(OUTPUT_FOLDER); ./inserter -f clock 1 $(DISK_NAME).bin

# ===== Aplikasi bonus =====
user-badapple:
	@$(ASM) $(AFLAGS) $(SOURCE_FOLDER)/crt0.s -o crt0.o
	@$(CC)  $(CFLAGS) -fno-pie $(SOURCE_FOLDER)/user-badapple.c -o user-badapple.o
	@$(LIN) -T $(SOURCE_FOLDER)/user-linker.ld -melf_i386 --oformat=binary \
		crt0.o user-badapple.o -o $(OUTPUT_FOLDER)/badapple
	@size --target=binary $(OUTPUT_FOLDER)/badapple
	@rm -f crt0.o user-badapple.o

# Bad Apple: ubah video jadi frame 80x24 1 bit (butuh ffmpeg + python3).
# Pemakaian: make badapple-data VIDEO=path/ke/badapple.mp4
badapple-data:
	@python3 tools/badapple-pack.py "$(VIDEO)" $(OUTPUT_FOLDER)/badapple-data

insert-badapple: inserter
	@if [ ! -f $(OUTPUT_FOLDER)/badapple-data/ba00 ]; then \
		echo "Data Bad Apple belum ada, jalankan dulu: make badapple-data VIDEO=<file video>"; exit 1; \
	fi
	@for f in $(OUTPUT_FOLDER)/badapple-data/ba[0-9][0-9]; do \
		name=$$(basename $$f); cp $$f $(OUTPUT_FOLDER)/$$name; \
		(cd $(OUTPUT_FOLDER); ./inserter -f $$name 1 $(DISK_NAME).bin > /dev/null) || exit 1; \
		rm -f $(OUTPUT_FOLDER)/$$name; echo "  $$name"; \
	done
