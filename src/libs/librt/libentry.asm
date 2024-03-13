[section .text]

[global ___app_trampoline]

[global __app_entrypoint]
[global __lib_entrypoints]
[global __lib_entrycount]

[global __real_start]
[global __init_lib]
[global __exit_error]
[global __exit_loop]


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
___app_trampoline:
  ; Save our shit
  push rdi
  push rsi
  push rdx
  push rcx

  ; Move the pointers for the inits into registers
  lea rdi, [rel __app_entrypoint]
  lea rsi, [rel __lib_entrypoints]
  lea rdx, [rel __lib_entrycount]

  ; Now move the actual values at those addresses into the pointers
  mov rdi, [rdi]
  mov rsi, [rsi]
  mov rdx, [rdx]

  mov rcx, rdx

__init_lib:
  ; Check our counter
  cmp rcx, 0
  ; No libraries to init at this point, let's go to the main entry yay
  je __libinit_end

  ; Preserve the sys-v registers
  push rcx
  push rdi
  push rsi
  push rdx

  ; Zero them for library execution
  mov rdi, 0
  mov rsi, 0
  mov rdx, 0
  mov rcx, 0

  ; Call the library entry
  mov rax, [rsp+8]
  call [rax]

  ; Restore the registers
  pop rdx
  pop rsi
  pop rdi
  pop rcx

  ; Check if the library call failed
  cmp rax, 0
  jne __exit_error

  ; Move to the next library entry
  add rsi, 8

  ; Decrement our counter and itterate
  dec rcx
  jmp __init_lib
__libinit_end:

  ; Move the app entry to rax for safekeeping
  mov rax, rdi

  ; Restore the registers which contain the actual
  ; parameters for our userspace application
  pop rcx
  pop rdx
  pop rsi
  pop rdi

  ; Call the app entry
  call rax

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

; (rdi=(DYNAPP_ENTRY_t) app_entry, rsi=(DYNLIB_ENTRY_t*) lib_entries, rdx=(size_t) lib_entry_count)
__app_entrypoint:
  dq 0
__lib_entrypoints:
  dq 0
__lib_entrycount:
  dq 0
