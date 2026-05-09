;=================================================================
; input.s — Scan clavier Oric-1 direct (VIA + PSG), Phase 3
;
; Exporte _key_scan : remplit _key_state avec un bitmask :
;   bit 0 : ← (LEFT, row 4 col 5)
;   bit 1 : → (RIGHT, row 4 col 7)
;   bit 2 : ↑ (UP, row 4 col 3) — thrust
;   bit 3 : SPACE (row 4 col 0) — tir
;   bit 4 : ↓ (DOWN, row 4 col 6) — hyperespace (Phase 7)
;
; Architecture clavier Oric-1 (cf. sources Phosphoric / Oricutron) :
;   - VIA Port B bits 0-2 ($0300) sélectionnent la colonne (74LS138)
;   - PSG (AY-3-8912) Port A en input = lecture des rangées
;   - Reg PSG 14 = masque de rangées actives (0 = activée)
;   - VIA PB3 (bit 3 de $0300) = 0 si une rangée active a une touche pressée
;
; PSG contrôlé via VIA :
;   BC1  = CA2 (PCR bits 1-3 ; mode 6 = low, mode 7 = high)
;   BDIR = CB2 (PCR bits 5-7 ; mode 6 = low, mode 7 = high)
;
; PCR bits 4 (CB1, VSync ROM) et 0 (CA1) doivent être préservés.
;
; Modes utilisés :
;   $CC = inactive (BDIR=0, BC1=0)
;   $EE = latch address (BDIR=1, BC1=1)
;   $EC = write data (BDIR=1, BC1=0)
;   $CE = read data (BDIR=0, BC1=1) — non utilisé ici
;
; SEI/CLI encadrent la séquence : la ROM scanne aussi le PSG dans son
; IRQ VSync, on ne veut pas être interrompu en plein latch d'adresse.
;=================================================================

        .export   _key_scan
        .importzp _key_state
        .exportzp kb_pcr_save        ; partagé avec sound.s

        VIA_ORB    = $0300
        VIA_ORA    = $0301
        VIA_DDRA   = $0303
        VIA_PCR    = $030C

        PCR_BC1_HI = $0E    ; CA2 mode 7 → BC1 = 1 (bits 1-3 = 111)
        PCR_BC1_LO = $0C    ; CA2 mode 6 → BC1 = 0 (bits 1-3 = 110)
        PCR_BDR_HI = $E0    ; CB2 mode 7 → BDIR = 1 (bits 5-7 = 111)
        PCR_BDR_LO = $C0    ; CB2 mode 6 → BDIR = 0 (bits 5-7 = 110)

;-----------------------------------------------------------------
; Variables ZP
;-----------------------------------------------------------------
        .zeropage
kb_pcr_save: .res 1     ; bits préservés du PCR (0 et 4)
kb_ddra_save: .res 1    ; DDRA d'origine (à restaurer après)
kb_tmp:      .res 1

;-----------------------------------------------------------------
; psg_write — écrit la valeur A dans le registre Y du PSG
; Détruit : A
;
; Séquence (BDIR / BC1) :
;   1. ORA = numéro de registre, PCR = $EE (latch addr)
;   2. PCR = $CC (inactive)
;   3. ORA = valeur, PCR = $EC (write data)
;   4. PCR = $CC (inactive)
;-----------------------------------------------------------------
        .segment "CODE"

psg_write:
        ; A = valeur, Y = numéro de registre
        sta  kb_tmp           ; sauver la valeur
        tya
        sta  VIA_ORA          ; ORA = numéro de registre

        lda  kb_pcr_save
        ora  #(PCR_BDR_HI | PCR_BC1_HI)   ; $EE — Latch Address
        sta  VIA_PCR
        lda  kb_pcr_save
        ora  #(PCR_BDR_LO | PCR_BC1_LO)   ; $CC — Inactive
        sta  VIA_PCR

        lda  kb_tmp
        sta  VIA_ORA          ; ORA = valeur du registre

        lda  kb_pcr_save
        ora  #(PCR_BDR_HI | PCR_BC1_LO)   ; $EC — Write Data
        sta  VIA_PCR
        lda  kb_pcr_save
        ora  #(PCR_BDR_LO | PCR_BC1_LO)   ; $CC — Inactive
        sta  VIA_PCR
        rts

;-----------------------------------------------------------------
; _key_scan — lit les 4 touches de jeu, écrit le bitmask dans _key_state
;
; Algo : sélectionne PSG reg14 = $EF (active la rangée 4 uniquement),
; puis pour chaque colonne d'intérêt : sélectionne via ORB, lit PB3.
; Si PB3 = 0 → touche (row 4, col) pressée.
;-----------------------------------------------------------------
_key_scan:
        sei                   ; pas d'IRQ ROM pendant l'accès PSG

        ; Sauvegarde bits invariants du PCR
        lda  VIA_PCR
        and  #$11             ; bit 0 (CA1) + bit 4 (CB1)
        sta  kb_pcr_save

        ; Sauvegarder DDRA et le mettre en output ($FF) pour écriture PSG.
        ; La ROM laisse DDRA = $00 (input) après son scan VSync ; nos
        ; écritures sur ORA seraient alors invisibles côté PSG.
        lda  VIA_DDRA
        sta  kb_ddra_save
        lda  #$FF
        sta  VIA_DDRA

        ; PSG reg14 = $EF (rangée 4 active, autres désactivées)
        lda  #$EF
        ldy  #14
        jsr  psg_write

        lda  #0
        sta  _key_state

        ;------------------------------------------------------------
        ; Test colonne 5 = LEFT ARROW (bit 0 du _key_state)
        ;------------------------------------------------------------
        lda  VIA_ORB
        and  #$F8
        ora  #5
        sta  VIA_ORB
        ; Délai de stabilisation (la ligne PB3 est combinatoire mais
        ; le PSG met 1 cycle à refléter le changement de colonne).
        nop
        nop
        lda  VIA_ORB
        and  #$08             ; PB3 = 1 si touche pressée (Phosphoric / 6522 hw)
        beq  @no_left
        lda  _key_state
        ora  #$01
        sta  _key_state
@no_left:

        ;------------------------------------------------------------
        ; Test colonne 7 = RIGHT ARROW (bit 1)
        ;------------------------------------------------------------
        lda  VIA_ORB
        and  #$F8
        ora  #7
        sta  VIA_ORB
        nop
        nop
        lda  VIA_ORB
        and  #$08
        beq  @no_right
        lda  _key_state
        ora  #$02
        sta  _key_state
@no_right:

        ;------------------------------------------------------------
        ; Test colonne 3 = UP ARROW (bit 2 → thrust)
        ;------------------------------------------------------------
        lda  VIA_ORB
        and  #$F8
        ora  #3
        sta  VIA_ORB
        nop
        nop
        lda  VIA_ORB
        and  #$08
        beq  @no_up
        lda  _key_state
        ora  #$04
        sta  _key_state
@no_up:

        ;------------------------------------------------------------
        ; Test colonne 0 = SPACE (bit 3 → tir)
        ;------------------------------------------------------------
        lda  VIA_ORB
        and  #$F8
        ora  #0
        sta  VIA_ORB
        nop
        nop
        lda  VIA_ORB
        and  #$08
        beq  @no_fire
        lda  _key_state
        ora  #$08
        sta  _key_state
@no_fire:

        ;------------------------------------------------------------
        ; Test colonne 6 = DOWN ARROW (bit 4 → hyperespace)
        ;------------------------------------------------------------
        lda  VIA_ORB
        and  #$F8
        ora  #6
        sta  VIA_ORB
        nop
        nop
        lda  VIA_ORB
        and  #$08
        beq  @no_hyper
        lda  _key_state
        ora  #$10
        sta  _key_state
@no_hyper:

        ; Restaurer reg14 = $FF (toutes rangées désactivées)
        lda  #$FF
        ldy  #14
        jsr  psg_write

        ; Restaurer DDRA (laisser la ROM dans son état attendu)
        lda  kb_ddra_save
        sta  VIA_DDRA

        cli
        rts
