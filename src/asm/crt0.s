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

        ; Retour au BASIC — JMP vecteur RESET ROM. La ROM ré-init
        ; le hardware (mode TEXT, clavier, etc.) et arrive au prompt READY.
        ;
        ; ATTENTION — Oric-1 ONLY ! Vecteur reset diffère sur Atmos :
        ;   Oric-1 BASIC 1.0/1.1 : $F800
        ;   Atmos BASIC 1.1      : $F88F (ou $FAA5 selon contexte)
        ; Un futur port Atmos devra conditionner cette adresse.
        jmp     $F800
