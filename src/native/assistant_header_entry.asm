option casemap:none

EXTERN pz_assistant_header_dispatch:QWORD
PUBLIC pz_assistant_header_entry

.code
pz_assistant_header_entry PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog
    mov r9, rdi
    mov qword ptr [rsp+20h], rbp
    mov rax, qword ptr [rsp+38h]
    mov qword ptr [rsp+28h], rax
    call qword ptr [pz_assistant_header_dispatch]
    add rsp, 38h
    ret
pz_assistant_header_entry ENDP
END
