[section .text]

[global _app_trampoline]
[global _app_trampoline_end]

[global __app_entrypoint]
[global __lib_entrypoints]
[global __lib_entrycount]

;
; App entry trampoline
; 
; This stub is a bit of code that gets executed in userspace, after it's been
; relocated by the dynamic loader. This bit of code gets copied/mapped into the
; processes memoryspace.
;
; TODO: Test this bitch lmao
;
align 4096
_app_trampoline:
  jmp __real_start

; (rdi=(DYNAPP_ENTRY_t) app_entry, rsi=(DYNLIB_ENTRY_t*) lib_entries, rdx=(uint32_t) lib_entry_count)
__app_entrypoint:
  dq 0
__lib_entrypoints:
  dq 0
__lib_entrycount:
  dq 0

; Where execution actually starts xD
__real_start:
  push rcx

  mov rcx, rdx

__init_lib:

  ; Preserve the sys-v registers
  push rdi
  push rsi
  push rdx

  ; Zero them for library execution
  mov rdi, 0
  mov rsi, 0
  mov rdx, 0

  ; Call the library entry
  call [rsp+8]

  ; Restore the registers
  pop rdx
  pop rsi
  pop rdi

  ; Check if the library call failed
  cmp rax, 0
  jne __exit_error

  ; Move to the next library entry
  add rsi, 8

  ; Do an itteration
  dec rcx
  cmp rcx, 0
  je __init_lib

  pop rcx

  ; Call the app entrty
  call rdi

__exit_error:
  ; Prepare the SYSID_EXIT syscall

  ; Move the app exit code (from rax) into our arg0 register for a syscall
  mov rbx, rax
  ; Move zero into rax to signal the EXIT syscall
  mov rax, 0

  ; Call the syscall (FIXME: What if we're on a 'legacy' system which does not support the syscall instruciton?)
  syscall

  ; Should not be reached...
  ;ret
__exit_loop:
  jmp __exit_loop;

_app_trampoline_end:

; Fill the rest of the page
times (4096 - (_app_trampoline_end - _app_trampoline)) db 0
