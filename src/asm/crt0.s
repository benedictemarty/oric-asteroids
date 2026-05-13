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

        ; Retour au BASIC — JMP indirect via vecteur RESET hardware
        ; ($FFFC/$FFFD). Convention 6502 : pointe vers le cold-start
        ; spécifique de la machine. Portable Oric-1 / Atmos sans
        ; détection runtime :
        ;   Oric-1 BASIC 1.0/1.1 : $FFFC/$FFFD = $00/$F8 → reset $F800
        ;   Atmos  BASIC 1.1     : $FFFC/$FFFD = $8F/$F8 → reset $F88F
        ; Pas de bug NMOS 6502 JMP indirect : $FFFC n'est pas en $xxFF.
        jmp     ($FFFC)
