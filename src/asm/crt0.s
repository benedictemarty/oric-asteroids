; crt0.s — startup Oric-1 pour cc65 target none
; Initialise pile matérielle, c_sp (pile logicielle cc65), BSS, appelle _main.

        .import         _main
        .import         __STACKSTART__
        .import         __BSS_RUN__, __BSS_SIZE__
        .importzp       c_sp            ; pile logicielle cc65 (adresse résolue par le linker)

        .segment        "STARTUP"

start:
        ; Pointeur de pile matériel (hardware stack) pleine page $01xx
        ldx     #$FF
        txs

        ; Pointeur de pile logiciel cc65 : c_sp est dans la ZP (adresse assignée par ld65)
        ; NE PAS utiliser $80/$81 en dur — c_sp peut être à une adresse quelconque après _lx0.._ly1
        lda     #<__STACKSTART__
        sta     c_sp
        lda     #>__STACKSTART__
        sta     c_sp+1

        ; Effacement du segment BSS (__BSS_SIZE__ == 0 dans notre cas, mais code générique)
        lda     #<__BSS_SIZE__
        ora     #>__BSS_SIZE__
        beq     bss_done            ; taille nulle → rien à faire

        ; Pointeur BSS dans sreg ($82/$83 sont sreg de cc65, utilisables ici avant _main)
        lda     #<__BSS_RUN__
        sta     $82
        lda     #>__BSS_RUN__
        sta     $83
        lda     #<__BSS_SIZE__
        ldx     #>__BSS_SIZE__
        beq     bss_partial
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
