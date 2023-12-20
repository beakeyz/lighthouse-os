[bits 64]
[section .text]

;
; Assembly bit of the context switch subsystem under Aniva
;
; The purpose of this file is kind of undetermined, but its goal is to streamline the context-switch as much as possible and
; to bring as much of the inline assembly out of line and into pure x86 NASM asm
;
