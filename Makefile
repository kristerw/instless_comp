MOVDBZ_PROG=movdbz-addition.s

CPPFLAGS=-m32 -std=c99 -ffreestanding
CFLAGS=-Wall -Wextra -nostdlib -O2

objects =  loader.o run_movdbz_program.o screen.o set_gdtr.o setup.o

harddisk.img: buildhddimg/buildhddimg.sh kernel.bin
	buildhddimg/buildhddimg.sh

kernel.bin: $(objects) linker.ld
	ld -T linker.ld -m elf_i386  -o $@ $(objects)

generated-prog.c: movdbz-as.py $(MOVDBZ_PROG)
	./movdbz-as.py -o generated-prog.c $(MOVDBZ_PROG)

loader.o: loader.S
run_movdbz_program.o: run_movdbz_program.c screen.h setup.h 
screen.o: screen.c screen.h
set_gdtr.o: set_gdtr.S
setup.o: generated-prog.c screen.h setup.c setup.h

.PHONY : clean
clean:
	rm -f *.o *.bin *.img *.log generated-prog.c
