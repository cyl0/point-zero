; Capture only known entry-frame values. The outer category at entryRSP+78h
; belongs to one admitted caller and is read through MemoryView by C++ only.
OPTION CASEMAP:NONE
EXTERN pz_filter_apply_dispatch:QWORD
EXTERN pz_filter_level_dispatch:QWORD
.code
pz_filter_apply_entry PROC FRAME
    sub rsp, 38h
    .allocstack 38h
    .endprolog
    lea r9, [rsp+38h]
    mov QWORD PTR [rsp+20h], r15
    mov rax, QWORD PTR [rsp+38h]
    mov QWORD PTR [rsp+28h], rax
    call QWORD PTR [pz_filter_apply_dispatch]
    add rsp, 38h
    ret
pz_filter_apply_entry ENDP

pz_filter_level_entry PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    mov r8, QWORD PTR [rsp+28h]
    call QWORD PTR [pz_filter_level_dispatch]
    add rsp, 28h
    ret
pz_filter_level_entry ENDP
END
