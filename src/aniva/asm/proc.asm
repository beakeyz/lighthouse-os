[bits    64]
[section .text]

global __userproc_entry_wrapper

align 4096
__userproc_entry_wrapper: ; RDI: actual proc function

    ; Check if RDI is actualy considered valid
    cmp rdi, 0
    je  __exit_with_error

    ; Call the function pointed to by RDI
    call rdi

	; At this point we want to preserve RAX
	jmp __do_exit_syscall

	; When RDI is invalid, we land here
__exit_with_error:
	mov rax, -1

__do_exit_syscall:
	; NOTE: preserve the return value inside RAX to RBX and call exit
	mov rbx, rax
    ; Specify the exit syscall
	mov rax, 1
    ; Invoke the exit syscall
	syscall

	times (4096 - ($ - __userproc_entry_wrapper)) db 0
