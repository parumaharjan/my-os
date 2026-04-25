GCCPARAMS = -m32 -Iinclude -fno-use-cxa-atexit -nostdlib -fno-builtin \
-fno-rtti -fno-exceptions -fno-leading-underscore -Wno-write-strings

ASPARAMS = --32
LDPARAMS = -melf_i386

objects = \
	obj/loader.o \
	obj/gdt.o \
	obj/hardwarecommunication/port.o \
	obj/hardwarecommunication/interruptstubs.o \
	obj/hardwarecommunication/interrupts.o \
	obj/drivers/driver.o \
	obj/drivers/keyboard.o \
	obj/init.o \
	obj/vga.o \
	obj/memorymanagement.o \
	obj/scheduler.o \
	obj/processes.o \
	obj/shell.o \
	obj/paging.o \
	obj/kernel.o

all: mykernel.iso

obj/%.o: src/%.cpp
	mkdir -p $(@D)
	gcc $(GCCPARAMS) -c -o $@ $<

obj/%.o: src/%.s
	mkdir -p $(@D)
	as $(ASPARAMS) -o $@ $<

mykernel.bin: linker.ld $(objects)
	ld $(LDPARAMS) -T $< -o $@ $(objects)

mykernel.iso: mykernel.bin
	rm -rf iso
	mkdir -p iso/boot/grub
	cp mykernel.bin iso/boot/mykernel.bin
	printf 'set timeout=0\nset default=0\n\nmenuentry "MyOS" {\n multiboot /boot/mykernel.bin\n boot\n}\n' > iso/boot/grub/grub.cfg
	grub-mkrescue -o mykernel.iso iso 2>/dev/null

run: mykernel.iso
	qemu-system-i386 -cdrom mykernel.iso -m 512 -serial stdio

run-log: mykernel.iso
	qemu-system-i386 -cdrom mykernel.iso -m 512 -serial file:serial.log
	@echo "[LOG] Serial output saved to serial.log"

clean:
	rm -rf obj mykernel.bin mykernel.iso iso serial.log

.PHONY: all run run-log clean