; Static Win64 frame: preserve qsort's four arguments and capture the native
; dispatcher's non-argument RSI/R15 before a C++ prologue can claim them.
OPTION CASEMAP:NONE
EXTERN pz_sort_dispatch:QWORD
.code
pz_sort_entry PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog
    mov QWORD PTR [rsp + 20h], rsi
    mov QWORD PTR [rsp + 28h], r15
    mov rax, QWORD PTR [rsp + 38h]
    mov QWORD PTR [rsp + 30h], rax
    call QWORD PTR [pz_sort_dispatch]
    add rsp, 38h
    ret
pz_sort_entry ENDP
END
