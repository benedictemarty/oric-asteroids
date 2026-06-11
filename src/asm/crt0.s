; crt0.s — startup Oric-1 utilisant zerobss/initlib/donelib de cc65
;
; Phase 36 : zerobss est un appel SÉPARÉ — initlib ne lance que les
; constructeurs CONDES. L'ancien commentaire « initlib appelle
; zerobss » était faux : la BSS partait avec le pattern RAM $55
; (constaté par dump Phosphoric) alors que le C suppose static = 0.
; Resté invisible tant que tout l'état était initialisé explicitement ;
; révélé par l'état écran des torpilles (blt_drawn, Phase 36).
; donelib appelle les destructeurs (à RTS de _main).

        .import         _main
        .import         __STACKSTART__
        .import         zerobss, initlib, donelib
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

        ; BSS = 0 (exigence C pour les statics sans initialiseur)
        jsr     zerobss

        ; Init lib cc65 (constructeurs CONDES)
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
