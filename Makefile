# ================== FLAGS ==================

GCCPARAMS = -m32 -Iinclude -fno-use-cxa-atexit -nostdlib -fno-builtin \
            -fno-rtti -fno-exceptions -fno-leading-underscore -Wno-write-strings

ASPARAMS  = --32
LDPARAMS  = -melf_i386


# ================== OBJECT FILES ==================

objects = \
	obj/loader.o \
	obj/gdt.o \
	obj/hardwarecommunication/port.o \
	obj/hardwarecommunication/interruptstubs.o \
	obj/hardwarecommunication/interrupts.o \
	obj/drivers/driver.o \
	obj/drivers/keyboard.o \
	obj/drivers/mouse.o \
	obj/kernel.o


# ================== DEFAULT TARGET ==================

all: mykernel.iso


# ================== BUILD RULES ==================

# Compile C++ → .o
obj/%.o: src/%.cpp
	mkdir -p $(@D)
	gcc $(GCCPARAMS) -c -o $@ $<

# Compile ASM → .o
obj/%.o: src/%.s
	mkdir -p $(@D)
	as $(ASPARAMS) -o $@ $<


# ================== LINK KERNEL ==================

mykernel.bin: linker.ld $(objects)
	ld $(LDPARAMS) -T $< -o $@ $(objects)
	@echo "[OK] Kernel linked: mykernel.bin ($(shell wc -c < mykernel.bin) bytes)"


# ================== CREATE ISO ==================

mykernel.iso: mykernel.bin
	@rm -rf iso
	@mkdir -p iso/boot/grub

	@cp mykernel.bin iso/boot/mykernel.bin

	@printf 'set timeout=0\nset default=0\n\nmenuentry "MyOS" {\n  multiboot /boot/mykernel.bin\n  boot\n}\n' \
		> iso/boot/grub/grub.cfg

	@grub-mkrescue -o mykernel.iso iso 2>/dev/null
	@echo "[OK] ISO created: mykernel.iso"


# ================== RUN (normal) ==================

run: mykernel.iso
	qemu-system-i386 \
		-cdrom mykernel.iso \
		-boot order=d \
		-m 512 \
		-serial stdio


# ================== RUN WITH DEBUG SERIAL LOG ==================
# Prints all serial output to terminal AND saves to serial.log
# Use printf to the serial port (0x3F8) for debug output from the kernel

run-log: mykernel.iso
	qemu-system-i386 \
		-cdrom mykernel.iso \
		-boot order=d \
		-m 512 \
		-serial file:serial.log
	@echo "[LOG] Serial output saved to serial.log"


# ================== GDB DEBUG SESSION ==================
# Terminal 1: make debug
# Terminal 2: make gdb
# Execution is PAUSED at kernel entry until gdb connects.

debug: mykernel.iso
	qemu-system-i386 \
		-cdrom mykernel.iso \
		-boot order=d \
		-m 512 \
		-serial stdio \
		-s -S

gdb:
	gdb \
		-ex "target remote localhost:1234" \
		-ex "symbol-file mykernel.bin" \
		-ex "set architecture i386" \
		-ex "layout src" \
		-ex "break kernelMain" \
		-ex "continue"


# ================== CLEAN ==================

clean:
	@rm -rf obj
	@rm -f mykernel.bin mykernel.iso serial.log
	@rm -rf iso
	@echo "[OK] Cleaned"


# ================== HELP ==================

help:
	@echo ""
	@echo "  make              build everything (default)"
	@echo "  make run          build + run in QEMU"
	@echo "  make run-log      build + run, save serial output to serial.log"
	@echo "  make debug        build + run QEMU paused, waiting for GDB"
	@echo "  make gdb          attach GDB to a running debug session"
	@echo "  make clean        remove all build artifacts"
	@echo ""


.PHONY: all run run-log debug gdb clean help