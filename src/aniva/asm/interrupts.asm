[bits 64]
[section .text]

extern irq_handler
extern exception_handler
extern processor_enter_interruption
extern processor_exit_interruption

; Macro to define a regulare IRQ interrupt handler
%macro interrupt_asm_entry 1
[global interrupt_asm_entry_%1]
interrupt_asm_entry_%1:

  push 0x00
  push %1

  push r15
  push r14
  push r13
  push r12
  push r11
  push r10
  push r9
  push r8
  push rax
  push rbx
  push rcx
  push rdx
  push rbp
  push rsi
  push rdi

  cld

  mov rdi, rsp
  mov rsi, 0x01
  call processor_enter_interruption

  mov rdi, rsp
  call irq_handler
  mov rsp, rax

  jmp asm_common_irq_exit
%endmacro

; Define a regular exception entry
%macro interrupt_excp_asm_entry 1
[global interrupt_excp_asm_entry_%1]
interrupt_excp_asm_entry_%1:

  push %1

  push r15
  push r14
  push r13
  push r12
  push r11
  push r10
  push r9
  push r8
  push rax
  push rbx
  push rcx
  push rdx
  push rbp
  push rsi
  push rdi

  cld

  mov rdi, rsp
  mov rsi, 0x01
  call processor_enter_interruption

  mov rdi, rsp
  call exception_handler
  mov rsp, rax

  jmp asm_common_irq_exit
%endmacro

; Define a regular exception entry, which does not push it's errorcode onto the stack
%macro interrupt_excp_nocode_asm_entry 1
[global interrupt_excp_asm_entry_%1]
interrupt_excp_asm_entry_%1:

  push 0
  push %1

  push r15
  push r14
  push r13
  push r12
  push r11
  push r10
  push r9
  push r8
  push rax
  push rbx
  push rcx
  push rdx
  push rbp
  push rsi
  push rdi

  cld

  mov rdi, rsp
  mov rsi, 0x01
  call processor_enter_interruption

  mov rdi, rsp
  call exception_handler
  mov rsp, rax

  jmp asm_common_irq_exit
%endmacro

[global asm_common_irq_exit]
asm_common_irq_exit:
  mov rdi, rsp
  call processor_exit_interruption

  pop rdi
  pop rsi
  pop rbp
  pop rdx
  pop rcx
  pop rbx
  pop rax
  pop r8
  pop r9
  pop r10
  pop r11
  pop r12
  pop r13
  pop r14
  pop r15

  add rsp, 16

  iretq

; Devision by zero
interrupt_excp_nocode_asm_entry 0
; Debug trap (User?)
interrupt_excp_nocode_asm_entry 1
; Unknown error (NMI, or non-maskable interrupt)
interrupt_excp_nocode_asm_entry 2
; Breakpoint stuff (User?)
interrupt_excp_nocode_asm_entry 3
; Overflow
interrupt_excp_nocode_asm_entry 4
; Bounds range exceeded
interrupt_excp_nocode_asm_entry 5
; Illegal instruction
interrupt_excp_nocode_asm_entry 6
; FPU error
interrupt_excp_nocode_asm_entry 7
; Double fault
interrupt_excp_asm_entry 8
; Segment overrrun
interrupt_excp_nocode_asm_entry 9
; Invalid TSS
interrupt_excp_asm_entry 10
; Segment not present
interrupt_excp_asm_entry 11
; Stack segment fault
interrupt_excp_asm_entry 12
; GPF
interrupt_excp_asm_entry 13
; Page fault
interrupt_excp_asm_entry 14
; Reserved
interrupt_excp_nocode_asm_entry 15
; x87 FPU exception
interrupt_excp_nocode_asm_entry 16
; Alignment failure
interrupt_excp_asm_entry 17
; Machine check
interrupt_excp_nocode_asm_entry 18
; SIMD fpu exception 
interrupt_excp_nocode_asm_entry 19
; Virtualization
interrupt_excp_nocode_asm_entry 20
; Control protection
interrupt_excp_asm_entry 21
; Reserved
interrupt_excp_nocode_asm_entry 22
interrupt_excp_nocode_asm_entry 23
interrupt_excp_nocode_asm_entry 24
interrupt_excp_nocode_asm_entry 25
interrupt_excp_nocode_asm_entry 26
interrupt_excp_nocode_asm_entry 27
; Hypervisor injection
interrupt_excp_nocode_asm_entry 28
; VMM communication
interrupt_excp_asm_entry 29
; Security
interrupt_excp_asm_entry 30
; Reserved
interrupt_excp_nocode_asm_entry 31

interrupt_asm_entry 32
interrupt_asm_entry 33
interrupt_asm_entry 34
interrupt_asm_entry 35
interrupt_asm_entry 36
interrupt_asm_entry 37
interrupt_asm_entry 38
interrupt_asm_entry 39
interrupt_asm_entry 40
interrupt_asm_entry 41
interrupt_asm_entry 42
interrupt_asm_entry 43
interrupt_asm_entry 44
interrupt_asm_entry 45
interrupt_asm_entry 46
interrupt_asm_entry 47
interrupt_asm_entry 48
interrupt_asm_entry 49
interrupt_asm_entry 50
interrupt_asm_entry 51
interrupt_asm_entry 52
interrupt_asm_entry 53
interrupt_asm_entry 54
interrupt_asm_entry 55
interrupt_asm_entry 56
interrupt_asm_entry 57
interrupt_asm_entry 58
interrupt_asm_entry 59
interrupt_asm_entry 60
interrupt_asm_entry 61
interrupt_asm_entry 62
interrupt_asm_entry 63
interrupt_asm_entry 64
interrupt_asm_entry 65
interrupt_asm_entry 66
interrupt_asm_entry 67
interrupt_asm_entry 68
interrupt_asm_entry 69
interrupt_asm_entry 70
interrupt_asm_entry 71
interrupt_asm_entry 72
interrupt_asm_entry 73
interrupt_asm_entry 74
interrupt_asm_entry 75
interrupt_asm_entry 76
interrupt_asm_entry 77
interrupt_asm_entry 78
interrupt_asm_entry 79
interrupt_asm_entry 80
interrupt_asm_entry 81
interrupt_asm_entry 82
interrupt_asm_entry 83
interrupt_asm_entry 84
interrupt_asm_entry 85
interrupt_asm_entry 86
interrupt_asm_entry 87
interrupt_asm_entry 88
interrupt_asm_entry 89
interrupt_asm_entry 90
interrupt_asm_entry 91
interrupt_asm_entry 92
interrupt_asm_entry 93
interrupt_asm_entry 94
interrupt_asm_entry 95
interrupt_asm_entry 96
interrupt_asm_entry 97
interrupt_asm_entry 98
interrupt_asm_entry 99
interrupt_asm_entry 100
interrupt_asm_entry 101
interrupt_asm_entry 102
interrupt_asm_entry 103
interrupt_asm_entry 104
interrupt_asm_entry 105
interrupt_asm_entry 106
interrupt_asm_entry 107
interrupt_asm_entry 108
interrupt_asm_entry 109
interrupt_asm_entry 110
interrupt_asm_entry 111
interrupt_asm_entry 112
interrupt_asm_entry 113
interrupt_asm_entry 114
interrupt_asm_entry 115
interrupt_asm_entry 116
interrupt_asm_entry 117
interrupt_asm_entry 118
interrupt_asm_entry 119
interrupt_asm_entry 120
interrupt_asm_entry 121
interrupt_asm_entry 122
interrupt_asm_entry 123
interrupt_asm_entry 124
interrupt_asm_entry 125
interrupt_asm_entry 126
interrupt_asm_entry 127
interrupt_asm_entry 128
interrupt_asm_entry 129
interrupt_asm_entry 130
interrupt_asm_entry 131
interrupt_asm_entry 132
interrupt_asm_entry 133
interrupt_asm_entry 134
interrupt_asm_entry 135
interrupt_asm_entry 136
interrupt_asm_entry 137
interrupt_asm_entry 138
interrupt_asm_entry 139
interrupt_asm_entry 140
interrupt_asm_entry 141
interrupt_asm_entry 142
interrupt_asm_entry 143
interrupt_asm_entry 144
interrupt_asm_entry 145
interrupt_asm_entry 146
interrupt_asm_entry 147
interrupt_asm_entry 148
interrupt_asm_entry 149
interrupt_asm_entry 150
interrupt_asm_entry 151
interrupt_asm_entry 152
interrupt_asm_entry 153
interrupt_asm_entry 154
interrupt_asm_entry 155
interrupt_asm_entry 156
interrupt_asm_entry 157
interrupt_asm_entry 158
interrupt_asm_entry 159
interrupt_asm_entry 160
interrupt_asm_entry 161
interrupt_asm_entry 162
interrupt_asm_entry 163
interrupt_asm_entry 164
interrupt_asm_entry 165
interrupt_asm_entry 166
interrupt_asm_entry 167
interrupt_asm_entry 168
interrupt_asm_entry 169
interrupt_asm_entry 170
interrupt_asm_entry 171
interrupt_asm_entry 172
interrupt_asm_entry 173
interrupt_asm_entry 174
interrupt_asm_entry 175
interrupt_asm_entry 176
interrupt_asm_entry 177
interrupt_asm_entry 178
interrupt_asm_entry 179
interrupt_asm_entry 180
interrupt_asm_entry 181
interrupt_asm_entry 182
interrupt_asm_entry 183
interrupt_asm_entry 184
interrupt_asm_entry 185
interrupt_asm_entry 186
interrupt_asm_entry 187
interrupt_asm_entry 188
interrupt_asm_entry 189
interrupt_asm_entry 190
interrupt_asm_entry 191
interrupt_asm_entry 192
interrupt_asm_entry 193
interrupt_asm_entry 194
interrupt_asm_entry 195
interrupt_asm_entry 196
interrupt_asm_entry 197
interrupt_asm_entry 198
interrupt_asm_entry 199
interrupt_asm_entry 200
interrupt_asm_entry 201
interrupt_asm_entry 202
interrupt_asm_entry 203
interrupt_asm_entry 204
interrupt_asm_entry 205
interrupt_asm_entry 206
interrupt_asm_entry 207
interrupt_asm_entry 208
interrupt_asm_entry 209
interrupt_asm_entry 210
interrupt_asm_entry 211
interrupt_asm_entry 212
interrupt_asm_entry 213
interrupt_asm_entry 214
interrupt_asm_entry 215
interrupt_asm_entry 216
interrupt_asm_entry 217
interrupt_asm_entry 218
interrupt_asm_entry 219
interrupt_asm_entry 220
interrupt_asm_entry 221
interrupt_asm_entry 222
interrupt_asm_entry 223
interrupt_asm_entry 224
interrupt_asm_entry 225
interrupt_asm_entry 226
interrupt_asm_entry 227
interrupt_asm_entry 228
interrupt_asm_entry 229
interrupt_asm_entry 230
interrupt_asm_entry 231
interrupt_asm_entry 232
interrupt_asm_entry 233
interrupt_asm_entry 234
interrupt_asm_entry 235
interrupt_asm_entry 236
interrupt_asm_entry 237
interrupt_asm_entry 238
interrupt_asm_entry 239
interrupt_asm_entry 240
interrupt_asm_entry 241
interrupt_asm_entry 242
interrupt_asm_entry 243
interrupt_asm_entry 244
interrupt_asm_entry 245
interrupt_asm_entry 246
interrupt_asm_entry 247
interrupt_asm_entry 248
interrupt_asm_entry 249
interrupt_asm_entry 250
interrupt_asm_entry 251
interrupt_asm_entry 252
interrupt_asm_entry 253
interrupt_asm_entry 254
interrupt_asm_entry 255
