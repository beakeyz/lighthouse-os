[bits 64]


global thread_entry_stub
global thread_entry_stub_end

;
; thread_entry_stub(FuncPtr realentry);
; rdi: realentry
;

thread_entry_stub:

  cmp rdi, 0
  jz .te_no_rdi

  call rdi

  ; Called entry successfully, time to cleanly exit
  jmp .te_exit

  ; No param, let's check the stack :clown:
  .te_no_rdi:
    pop rdi
    cmp rdi, 0
    jz .te_exit_no_rdi

    call rdi

  ; Restore the stack, like a good boi
  .te_exit_no_rdi:
    pop rdi

  .te_exit:
    ret

thread_entry_stub_end:
