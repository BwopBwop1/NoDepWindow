
.text

.extern main

.global _start
.type _start, @function
_start:
  call main
  movq %rax, %rdi
  movq $60, %rax
  syscall
  ret

.global syscall
.type syscall, @function
syscall:
  movq %rdi, %rax
  movq %rsi, %rdi
  movq %rdx, %rsi
  movq %rcx, %rdx
  movq %r8, %r10
  movq %r9, %r8
  movq 8(%rsp), %r9
  syscall
  ret
