; Native invocation frame for private summary calculation. It supplies the exact
; native entry RSP to the held scope before jumping through the original route.
; Exceptions originate in native code after the relocated prologue; this caller
; has ordinary static Win64 unwind metadata.
OPTION CASEMAP:NONE
.code
pz_category_summary_invoke PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    mov rax, QWORD PTR [rcx]
    lea r9, [rsp-8]
    mov QWORD PTR [rcx+18h], r9
    mov r8d, DWORD PTR [rcx+14h]
    mov edx, DWORD PTR [rcx+10h]
    mov rcx, QWORD PTR [rcx+8]
    call rax
    add rsp, 28h
    ret
pz_category_summary_invoke ENDP
END
