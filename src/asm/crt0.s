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
        sei                     ; disable IRQ tout de suite (ROM Oric)
        ldx     #$FF
        txs

        ; ── Reset VIA 6522 dans un état connu ──────────────────────
        ; Avec auto-exec via .tap autorun (Phosphoric v1.16.3+) ou
        ; oricutron, la ROM peut laisser le VIA dans un état imprévu
        ; (PCR bit 4 = 1 = CB1 rising edge ⇒ frame_wait bloque).
        ; On force ici un état standard Oric-1 :
        ;   PCR = $CD : CA1 positive (bit 0=1), CA2 mode 6 (BC1 low,
        ;               bits 1-3 = 110), CB1 FALLING EDGE (bit 4 = 0),
        ;               CB2 mode 6 (BDIR low, bits 5-7 = 110).
        ;               Valeur "idle PSG" + CB1 OK pour poll VSync.
        ;               ATTENTION : $1D = bit 4 = 1 (CB1 RISING) → bug,
        ;               le flag CB1 IFR ne se latche jamais sur VSync ULA
        ;               qui est falling edge.
        ;   IER = $7F : disable toutes les IRQ
        ;   IFR = $7F : clear tous les flags latched
        ;   ACR = $00 : timers disabled
        lda     #$CD
        sta     $030C           ; VIA_PCR — CB1 falling edge (bit 4 = 0)
        lda     #$7F
        sta     $030E           ; VIA_IER — disable all IRQ
        sta     $030D           ; VIA_IFR — clear all flags
        lda     #$00
        sta     $030B           ; VIA_ACR — no shift reg / timer continuous
        cli                     ; ré-active IRQ ; mais IER off = aucune ne sera servie

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
