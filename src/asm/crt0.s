; crt0.s — startup Oric-1 pour cc65 target none
; Charge le pointeur de pile logiciel C, efface BSS, appelle _main.

        .import         _main
        .import         __STACKSTART__
        .import         __BSS_RUN__, __BSS_SIZE__

        .segment        "STARTUP"

start:
        ; Pointeur de pile matériel (hardware stack) pleine page $01xx
        ldx     #$FF
        txs

        ; Pointeur de pile logiciel cc65 (sp, 2 octets en ZP à __ZPSTART__)
        ; __ZPSTART__ = $80 → sp_lo = $80, sp_hi = $81
        lda     #<__STACKSTART__
        ldx     #>__STACKSTART__
        sta     $80
        stx     $81

        ; Effacement du segment BSS (variables globales non initialisées)
        lda     #<__BSS_RUN__
        sta     $82
        lda     #>__BSS_RUN__
        sta     $83
        lda     #<__BSS_SIZE__
        ldx     #>__BSS_SIZE__
        ora     $82             ; taille non nulle ?
        beq     bss_done
        txa
        beq     bss_partial     ; taille < 256 ?
        ; pages entières
bss_page:
        ldy     #0
        lda     #0
bss_page_loop:
        sta     ($82),y
        iny
        bne     bss_page_loop
        inc     $83
        dex
        bne     bss_page
bss_partial:
        lda     #<__BSS_SIZE__
        beq     bss_done
        ldy     #0
        lda     #0
bss_partial_loop:
        sta     ($82),y
        iny
        cpy     #<__BSS_SIZE__
        bne     bss_partial_loop
bss_done:

        ; Appel du programme principal C
        jsr     _main

        ; En cas de retour : boucle infinie (Phosphoric --cycles arrête la session)
hang:
        jmp     hang
