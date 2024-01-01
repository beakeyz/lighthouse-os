[section .text]

; int lib_entry(void);
[extern lib_entry]

; lib_entry must be valid, otherwise we could not have been linked
[global _start]
_start:
  ; Make sure rax is zeroed
  xor rax, rax
  ; Call entry
  call lib_entry
  ; Return from the library start function
  ; at this point, the library must have its memory initialized,
  ; variables set and it is ready to be used by its child app
  ; NOTE: keep the return value from lib_entry in rax
  ret
