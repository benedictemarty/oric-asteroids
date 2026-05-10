;=================================================================
; input.s — Scan clavier Oric-1 direct (VIA + PSG), Phase 18e
;
; Exporte _key_scan : remplit _key_state avec un bitmask :
;   bit 0 : ← (LEFT, HW row 5 col 4)
;   bit 1 : → (RIGHT, HW row 7 col 4)
;   bit 2 : ↑ (UP, HW row 3 col 4) — thrust
;   bit 3 : SPACE (HW row 0 col 4) — tir
;   bit 4 : ↓ (DOWN, HW row 6 col 4) — hyperespace
;
; Mapping HW Oric-1 (validé contre Phosphoric corrigé) : TOUTES les
; touches utilisées sont sur HW colonne 4 (la colonne LSHIFT/FUNCT
; du 74LS138), différenciées par leur ligne PSG R14 :
;
;   ┌────────┬──────────┬─────────────┬─────────┐
;   │ Touche │ HW col   │ HW row      │ R14     │
;   │        │ (VIA ORB)│ (mask R14)  │ scan    │
;   ├────────┼──────────┼─────────────┼─────────┤
;   │ SPACE  │ 4        │ 0           │ $FE     │
;   │ UP     │ 4        │ 3           │ $F7     │
;   │ LEFT   │ 4        │ 5           │ $DF     │
;   │ DOWN   │ 4        │ 6           │ $BF     │
;   │ RIGHT  │ 4        │ 7           │ $7F     │
;   ├────────┼──────────┼─────────────┼─────────┤
;   │ LSHIFT │ 4        │ 4           │ $EF     │ (non utilisé ici)
;   └────────┴──────────┴─────────────┴─────────┘
;
; Algo : ORB[0:2] = 4 fixé pour toute la séquence. Pour chaque touche,
; on écrit le R14 correspondant (un seul bit à 0 = ligne active) et on
; lit PB3 :
;   - PB3 = 1 → touche pressée à (R14_row, col=4)
;   - PB3 = 0 → pas de touche pressée
;
; Architecture clavier Oric-1 :
;   - VIA Port B bits 0-2 ($0300) sélectionnent la colonne (74LS138)
;   - PSG (AY-3-8912) Port A en input = lecture des rangées
;   - Reg PSG 14 = masque de rangées actives (bit à 0 = activée)
;   - VIA PB3 = combinatoire : 1 si une rangée active a une touche
;     pressée sur la colonne sélectionnée
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
;
; SEI/CLI encadrent la séquence : la ROM scanne aussi le PSG dans son
; IRQ VSync, on ne veut pas être interrompu en plein latch d'adresse.
;=================================================================

        .export   _key_scan
        .importzp _key_state
        .exportzp kb_pcr_save        ; partagé avec sound.s

        VIA_ORB    = $0300
        VIA_ORA    = $0301
        VIA_DDRB   = $0302
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
kb_pcr_save:  .res 1     ; bits préservés du PCR (0 et 4)
kb_ddra_save: .res 1     ; DDRA d'origine (à restaurer après)
kb_ddrb_save: .res 1     ; DDRB d'origine (à restaurer après)
kb_tmp:       .res 1

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
; _key_scan — lit les 5 touches de jeu, écrit le bitmask dans _key_state
;
; ORB[0:2] = 4 (col fixe).  Pour chaque touche :
;   - PSG reg14 = mask avec UN SEUL bit à 0 (ligne active)
;   - Lecture PB3 → 1 si touche pressée
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

        ; Sauvegarder DDRB et forcer PB3 = input (bits 0-2 et 4-7 = output).
        ; Sur la VIA 6522, lda VIA_ORB retourne (orb & ddrb) | (pin & ~ddrb) :
        ; si DDRB[3] = 1 (output), on relit ce qu'on a écrit, pas l'état clavier.
        lda  VIA_DDRB
        sta  kb_ddrb_save
        lda  #$F7
        sta  VIA_DDRB

        ; PSG reg 7 = $7F : mixer tout muet + Port A en input (bit 6 = 1).
        ; Si bit 6 = 0 (Port A en output côté PSG), le matériel ne peut
        ; PAS lire la matrice → PB3 reste à 0 quoi qu'il arrive.
        lda  #$7F
        ldy  #7
        jsr  psg_write

        ; Sélectionner HW col 4 sur ORB[0:2] (commun à tous les tests).
        lda  VIA_ORB
        and  #$F8
        ora  #4
        sta  VIA_ORB

        lda  #0
        sta  _key_state

        ;------------------------------------------------------------
        ; SPACE = HW row 0, col 4 → reg14 = $FE (bit 0)
        ;------------------------------------------------------------
        lda  #$FE
        ldy  #14
        jsr  psg_write
        ; Délai de stabilisation matériel (le PSG met ~1 cycle à
        ; refléter le changement de R14 sur PB3).
        nop
        nop
        lda  VIA_ORB
        and  #$08             ; PB3 = 1 si touche pressée
        beq  @no_fire
        lda  _key_state
        ora  #$08             ; bit 3 → SPACE
        sta  _key_state
@no_fire:

        ;------------------------------------------------------------
        ; UP = HW row 3, col 4 → reg14 = $F7 (bit 3)
        ;------------------------------------------------------------
        lda  #$F7
        ldy  #14
        jsr  psg_write
        nop
        nop
        lda  VIA_ORB
        and  #$08
        beq  @no_up
        lda  _key_state
        ora  #$04             ; bit 2 → UP
        sta  _key_state
@no_up:

        ;------------------------------------------------------------
        ; LEFT = HW row 5, col 4 → reg14 = $DF (bit 5)
        ;------------------------------------------------------------
        lda  #$DF
        ldy  #14
        jsr  psg_write
        nop
        nop
        lda  VIA_ORB
        and  #$08
        beq  @no_left
        lda  _key_state
        ora  #$01             ; bit 0 → LEFT
        sta  _key_state
@no_left:

        ;------------------------------------------------------------
        ; DOWN = HW row 6, col 4 → reg14 = $BF (bit 6)
        ;------------------------------------------------------------
        lda  #$BF
        ldy  #14
        jsr  psg_write
        nop
        nop
        lda  VIA_ORB
        and  #$08
        beq  @no_hyper
        lda  _key_state
        ora  #$10             ; bit 4 → DOWN
        sta  _key_state
@no_hyper:

        ;------------------------------------------------------------
        ; RIGHT = HW row 7, col 4 → reg14 = $7F (bit 7)
        ;------------------------------------------------------------
        lda  #$7F
        ldy  #14
        jsr  psg_write
        nop
        nop
        lda  VIA_ORB
        and  #$08
        beq  @no_right
        lda  _key_state
        ora  #$02             ; bit 1 → RIGHT
        sta  _key_state
@no_right:

        ; Restaurer reg14 = $FF (toutes rangées désactivées)
        lda  #$FF
        ldy  #14
        jsr  psg_write

        ; Restaurer DDRA et DDRB
        lda  kb_ddra_save
        sta  VIA_DDRA
        lda  kb_ddrb_save
        sta  VIA_DDRB

        cli
        rts
