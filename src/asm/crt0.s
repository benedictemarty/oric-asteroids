; crt0.s — startup Oric-1 utilisant initlib/donelib de cc65
;
; initlib appelle zerobss (clear BSS officiel) puis les constructeurs.
; donelib appelle les destructeurs (à RTS de _main).

        .import         _main
        .import         __STACKSTART__
        .import         initlib, donelib
        .importzp       c_sp

        .segment        "STARTUP"

start:
        ldx     #$FF
        txs

        ; Init c_sp
        lda     #<__STACKSTART__
        sta     c_sp
        lda     #>__STACKSTART__
        sta     c_sp+1

        ; Init lib cc65 (zerobss + constructeurs CONDES)
        jsr     initlib

        ; Programme principal
        jsr     _main

        ; Cleanup (destructeurs CONDES)
        jsr     donelib

hang:   jmp     hang
