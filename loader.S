STACKSIZE = 0x4000

# Constants for GRUB Multiboot header.
MODULEALIGN = 1<<0
MEMINFO = 1<<1
FLAGS = MODULEALIGN | MEMINFO
MAGIC = 0x1BADB002
CHECKSUM = -(MAGIC + FLAGS)

	.global	loader
	.section .loader

	.long	MAGIC
	.long	FLAGS
	.long	CHECKSUM
loader:
	mov	$stack+STACKSIZE,%esp
	call	kmain
	cli
.Lhang:
	hlt
	jmp	.Lhang

	.section .bss
stack:	.zero 	STACKSIZE
