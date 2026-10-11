; Win64 ABI adapter for the selected shared image-emitter entry. Do not inspect
; RSI from a C++ function: its prologue may already have claimed that register.
; No native register/state snapshot or runtime-generated callback stub is needed.
OPTION CASEMAP:NONE
EXTERN pz_list_draw_dispatch:QWORD

.code
pz_list_draw_entry PROC FRAME
    sub rsp, 48h
    .allocstack 48h
    .endprolog

    ; Preserve raw native arguments5..7 and add RSI/return address as8..9.
    mov eax, DWORD PTR [rsp + 70h]
    mov DWORD PTR [rsp + 20h], eax
    mov eax, DWORD PTR [rsp + 78h]
    mov DWORD PTR [rsp + 28h], eax
    mov eax, DWORD PTR [rsp + 80h]
    mov DWORD PTR [rsp + 30h], eax
    mov QWORD PTR [rsp + 38h], rsi
    mov rax, QWORD PTR [rsp + 48h]
    mov QWORD PTR [rsp + 40h], rax
    call QWORD PTR [pz_list_draw_dispatch]
    add rsp, 48h
    ret
pz_list_draw_entry ENDP
END
