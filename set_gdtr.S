	.global	set_gdtr
	.section .text

set_gdtr:
	sub	$0x10,%esp
	mov	0x18(%esp),%eax
	mov	%ax,0xa(%esp)
	mov	0x14(%esp),%eax
	mov	%eax,0xc(%esp)
	lgdtl	0xa(%esp)
	add	$0x10,%esp
	ljmp	$0x8,$.Lreload_cs	# 0x08 points at the new code selector
.Lreload_cs:
	mov	$0x10,%ax		# 0x10 points at the new data selector
	mov	%eax,%ds
	mov	%eax,%es
	mov	%eax,%fs
	mov	%eax,%gs
	mov	%eax,%ss
	mov	$0x18,%ax
	ltr	%ax
	ret
