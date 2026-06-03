; Naked trampoline for the Yakuza 0 camera state hook at RVA 0x18FD38.
;
; Hook contract (from .lab/camera-analysis.md):
;   On entry: xmm4=up, xmm5=focus, xmm6=position. rax points to a camera-config
;   struct with [rax+0xAC] holding the FOV (radians, low 4 bytes of an 8-byte
;   slot the engine treats as packed). rsp is whatever the engine has it at
;   mid-function -- arbitrary alignment.
;
;   The patched 5 bytes encode `movaps [rsp+0x40], xmm4` (the first of three
;   back-to-back xmm spills at rsp+0x40 / rsp+0x50 / rsp+0x60). We must spill
;   xmm4 to that slot ourselves before jumping back, because the patched
;   bytes won't execute.
;
; Strategy: save volatile regs we clobber, copy clean xmm4/5/6 + FOV into our
; CameraState buffer, call into C++ (which may modify xmm4 / xmm5 / xmm6 in
; the buffer), reload modified xmm4 / xmm5 / xmm6, restore volatiles, execute
; the displaced instruction, jump back to (hook_addr + 5).

EXTERN cul_camera_inject:PROC

PUBLIC cul_camera_buffer
PUBLIC cul_camera_resume
PUBLIC cul_camera_trampoline

.data
cul_camera_buffer   QWORD 0   ; CameraState*  -- assigned at hook install
cul_camera_resume   QWORD 0   ; absolute address of (hook_addr + 5)

.code

cul_camera_trampoline PROC
    ; --- Save volatiles we will clobber -------------------------------------
    push    rax
    push    rcx
    push    rdx
    push    r8
    push    r9
    push    r10
    push    r11
    push    rbp                 ; non-volatile, used as unaligned-rsp anchor

    mov     rbp, rsp            ; remember unaligned stack
    and     rsp, -16            ; 16-align for C calling convention
    sub     rsp, 80h            ; shadow space + 4x xmm save

    movups  xmmword ptr [rsp+20h], xmm0
    movups  xmmword ptr [rsp+30h], xmm1
    movups  xmmword ptr [rsp+40h], xmm2
    movups  xmmword ptr [rsp+50h], xmm3

    ; --- Snapshot clean camera state into our buffer ------------------------
    mov     r10, qword ptr [cul_camera_buffer]
    test    r10, r10
    jz      skip_inject

    movups  xmmword ptr [r10+00h], xmm5      ; focus
    movups  xmmword ptr [r10+20h], xmm6      ; position
    movups  xmmword ptr [r10+40h], xmm4      ; up
    mov     r11, qword ptr [rax+0ACh]
    mov     qword ptr [r10+60h], r11         ; FOV (8-byte slot)
    mov     qword ptr [r10+70h], rbx         ; camera instance (rbx is live here;
                                             ; *rbx is its vtable = type discriminator)

    ; --- Call into C++ to apply head-tracked rotation -----------------------
    mov     rcx, r10
    call    cul_camera_inject

    ; --- Reload xmm4 (up), xmm5 (focus), xmm6 (position) from buffer --------
    ; xmm6 is callee-saved, but the engine overwrites xmm5/xmm6 with constants
    ; after the matrix-builder call (0x18FE35/0x18FE4B) before any read, so
    ; writing the 6DOF-offset position back is render-only like focus/up.
    mov     r10, qword ptr [cul_camera_buffer]
    movups  xmm4, xmmword ptr [r10+40h]
    movups  xmm5, xmmword ptr [r10+00h]
    movups  xmm6, xmmword ptr [r10+20h]

skip_inject:

    movups  xmm0, xmmword ptr [rsp+20h]
    movups  xmm1, xmmword ptr [rsp+30h]
    movups  xmm2, xmmword ptr [rsp+40h]
    movups  xmm3, xmmword ptr [rsp+50h]

    mov     rsp, rbp            ; restore unaligned stack
    pop     rbp
    pop     r11
    pop     r10
    pop     r9
    pop     r8
    pop     rdx
    pop     rcx
    pop     rax

    ; --- Execute the displaced instruction and return to (hook + 5) ---------
    movaps  xmmword ptr [rsp+40h], xmm4
    jmp     qword ptr [cul_camera_resume]
cul_camera_trampoline ENDP

END
