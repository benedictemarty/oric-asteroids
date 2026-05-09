; crt0.s — startup Oric-1 pour cc65 target none
; Initialise pile matérielle, c_sp (pile logicielle cc65), BSS, appelle _main.
;
; Utilise ptr1 (alloué par cc65 dans la ZP) comme pointeur temporaire pour
; le clear BSS — n'écrase pas line.s/ship.s qui réservent $80-$8E en ZP.

        .import         _main
        .import         __STACKSTART__
        .import         __BSS_RUN__, __BSS_SIZE__
        .importzp       c_sp, ptr1

        .segment        "STARTUP"

start:
        ldx     #$FF
        txs

        lda     #<__STACKSTART__
        sta     c_sp
        lda     #>__STACKSTART__
        sta     c_sp+1

        ; Pas de clear BSS automatique : chaque module C initialise
        ; explicitement ses variables (bullets_init, asteroids_init, hud_init,
        ; ufo_init...). Évite un bug Phosphoric/cc65 reproduit Phase 6 quand
        ; BSS_SIZE >= $83 (cause précise non identifiée — la zone HIRES
        ; n'est plus activée correctement après le clear).

        jsr     _main

hang:
        jmp     hang
