	.file	"writeJmp.c"
	.text
	.globl	main
	.type	main, @function
main:
.LFB0:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register 6
	xor	%rax, %rax
l0:
	leaq	l2(%rip), %rcx
	leaq	l0(%rip), %rdx
	sub	%rdx, %rcx
	imul	%rax, %rcx
	leaq	l1(%rip), %rdx
	add	%cl, 1(%rdx)
	inc	%rax
l1:
	jno	l0
l2:
	nop
	.cfi_def_cfa 7, 8
	movq $60, %rax
	movq $0,  %rdi
	syscall
	.cfi_endproc
.LFE0:
	.size	main, .-main
	.ident	"GCC: (Ubuntu/Linaro 4.6.3-1ubuntu5) 4.6.3"
	.section	.note.GNU-stack,"",@progbits
