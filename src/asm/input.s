;=================================================================
; input.s — Scan clavier Oric-1 direct (VIA + PSG), Phase 18e
;           + joystick IJK OR-é dans le même bitmask (Phase 38,
;           protocole VIA direct corrigé Phase 39 — cf. bloc IJK)
;           + touches remappables via _key_map et scan complet
;             _key_probe pour l'écran de config (Phase 40)
;
; Exporte _key_scan : remplit _key_state avec un bitmask :
;   bit 0 : LEFT   (défaut ←, HW col 4 row 5)  | IJK Left
;   bit 1 : RIGHT  (défaut →, HW col 4 row 7)  | IJK Right
;   bit 2 : THRUST (défaut ↑, HW col 4 row 3)  | IJK Up
;   bit 3 : FIRE   (défaut SPACE, col 4 row 0) | IJK Fire
;   bit 4 : HYPER  (défaut ↓, HW col 4 row 6)  | IJK Down
;   bit 5 : ESC (HW col 1 row 5) — quitter, NON remappable
;
; Phase 40 : les 5 touches d'action sont lues depuis _key_map (DATA,
; modifiable par keys.c), 2 octets par action : (colonne ORB, masque
; R14 actif bas). Les défauts ci-dessus reproduisent le mapping
; historique flèches + SPACE (validé Phosphoric + Oric-1 réel).
;
; Exporte _key_probe : scan complet de la matrice 8×8, retourne dans A
; le code col*8+row de la première touche pressée trouvée (ordre de
; scan : row-major), ou $FF si aucune. Réservé aux écrans titre/config
; (~8 psg_write + 64 lectures ORB par appel).
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
        .export   _key_probe
        .export   _key_map
        .importzp _key_state
        .import   mixer_shadow       ; sound.s (BSS) — dernière valeur R7 (Phase 23)
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
kb_bit:       .res 1     ; bit key_state de l'action en cours (Phase 40)

;-----------------------------------------------------------------
; _key_map — 5 actions × (colonne ORB, masque R14 actif bas).
; Ordre = bits 0-4 de _key_state : LEFT, RIGHT, THRUST, FIRE, HYPER.
; Modifiée par keys.c (key_apply). Défauts = flèches + SPACE.
;-----------------------------------------------------------------
        .segment "DATA"
_key_map:
        .byte 4, $DF        ; LEFT   = ← (col 4, row 5)
        .byte 4, $7F        ; RIGHT  = → (col 4, row 7)
        .byte 4, $F7        ; THRUST = ↑ (col 4, row 3)
        .byte 4, $FE        ; FIRE   = SPACE (col 4, row 0)
        .byte 4, $BF        ; HYPER  = ↓ (col 4, row 6)

;-----------------------------------------------------------------
; kb_rowmask — masque R14 par rangée (un seul bit à 0)
;-----------------------------------------------------------------
        .segment "RODATA"
kb_rowmask:
        .byte $FE, $FD, $FB, $F7, $EF, $DF, $BF, $7F

;-----------------------------------------------------------------
; psg_write — écrit la valeur A dans le registre Y du PSG
; Détruit : A. Préserve X et Y.
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
; kb_setup — préambule commun _key_scan / _key_probe (sous SEI).
; Sauve PCR (bits invariants), DDRA/DDRB, force les directions, et
; réécrit R7 = mixer_shadow (bit 6 = 1 ⇒ port A PSG en sortie, sinon
; R14 ne pilote pas les rangées et PB3 reste muet — cf. Phase 23).
;-----------------------------------------------------------------
kb_setup:
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

        ; PSG reg 7 : réécrire la valeur mixer COURANTE (mixer_shadow,
        ; maintenue par sound.s, bit 6 toujours = 1) au lieu d'un $7F figé
        ; qui hacherait les canaux actifs (bug « audio ship explosion »).
        lda  mixer_shadow
        ldy  #7
        jmp  psg_write        ; tail-call (rts de psg_write)

;-----------------------------------------------------------------
; kb_teardown — épilogue commun : R14 = $FF (toutes rangées off),
; restaure DDRA/DDRB. L'appelant fait le cli.
;-----------------------------------------------------------------
kb_teardown:
        lda  #$FF
        ldy  #14
        jsr  psg_write
        lda  kb_ddra_save
        sta  VIA_DDRA
        lda  kb_ddrb_save
        sta  VIA_DDRB
        rts

;-----------------------------------------------------------------
; _key_scan — lit les 5 actions (_key_map) + ESC, écrit le bitmask
; dans _key_state, puis OR-e le joystick IJK.
;
; Pour chaque action :
;   - ORB[0:2] = colonne (key_map[2i])
;   - PSG reg14 = masque rangée (key_map[2i+1], un seul bit à 0)
;   - Lecture PB3 → 1 si touche pressée → key_state |= 1<<i
;-----------------------------------------------------------------
_key_scan:
        sei                   ; pas d'IRQ ROM pendant l'accès PSG
        jsr  kb_setup

        lda  #0
        sta  _key_state
        lda  #$01
        sta  kb_bit           ; bit de l'action courante (1,2,4,8,16)
        ldx  #0               ; index dans _key_map (0,2,4,6,8)

@scan_loop:
        ; ORB[0:2] = colonne de l'action
        lda  VIA_ORB
        and  #$F8
        ora  _key_map,x
        sta  VIA_ORB
        inx
        ; R14 = masque rangée (psg_write préserve X)
        lda  _key_map,x
        inx
        ldy  #14
        jsr  psg_write
        ; Délai de stabilisation matériel (le PSG met ~1 cycle à
        ; refléter le changement de R14 sur PB3).
        nop
        nop
        lda  VIA_ORB
        and  #$08             ; PB3 = 1 si touche pressée
        beq  @no_key
        lda  _key_state
        ora  kb_bit
        sta  _key_state
@no_key:
        asl  kb_bit
        cpx  #10
        bcc  @scan_loop

        ;------------------------------------------------------------
        ; ESC = HW row 5, col 1 → reg14 = $DF (bit 5), ORB[0:2] = 1
        ; NON remappable (réservé quitter/annuler).
        ;------------------------------------------------------------
        lda  VIA_ORB
        and  #$F8
        ora  #1
        sta  VIA_ORB
        lda  #$DF
        ldy  #14
        jsr  psg_write
        nop
        nop
        lda  VIA_ORB
        and  #$08
        beq  @no_esc
        lda  _key_state
        ora  #$20             ; bit 5 → ESC
        sta  _key_state
@no_esc:

        ; Restaurer reg14 = $FF (toutes rangées désactivées) AVANT le
        ; bloc IJK : le protocole IJK exige le PSG inactif (PCR $CC,
        ; c'est l'état post-psg_write).
        lda  #$FF
        ldy  #14
        jsr  psg_write

        ;------------------------------------------------------------
        ; Joystick IJK (Phase 38, protocole corrigé Phase 39) —
        ; l'interface IJK est sur le port IMPRIMANTE = port A du VIA
        ; en DIRECT (pas via le PSG ! — bug v1 signalé par xahmol sur
        ; matériel réel, protocole validé contre Oricutron) :
        ;
        ;   - enable  : PB4 (strobe imprimante) en sortie, à 0
        ;   - select  : ORA bits 6-7 en sortie — bit6=1 → stick A,
        ;               bit7=1 → stick B, les DEUX à 1 → rien
        ;   - lecture : ORA bits 0-5 en entrée, actif bas :
        ;               bit0=Right, bit1=Left, bit2=Fire,
        ;               bit3=Down,  bit4=Up,
        ;               bit5=présence (0 = interface branchée)
        ;
        ; Sans interface, bit5 lit 1 (pull-up) ⇒ on ignore tout :
        ; aucun input fantôme possible. Le résultat est OR-é dans
        ; _key_state : clavier ET joystick simultanés. Le joystick
        ; n'est PAS remappable (mapping physique fixe de l'interface).
        ;------------------------------------------------------------
        ; PB4 = 0 (enable IJK). DDRB vaut $F7 ici → PB4 déjà en sortie.
        lda  VIA_ORB
        and  #$EF
        sta  VIA_ORB

        ; DDRA = $C0 (bits 6-7 sortie = select, bits 0-5 entrée),
        ; select stick A : bit6=1, bit7=0.
        lda  #$C0
        sta  VIA_DDRA
        lda  #$40
        sta  VIA_ORA
        nop                   ; stabilisation lignes (pull-ups)
        nop
        lda  VIA_ORA          ; bits 0-5 = état stick A (actif bas)
        sta  kb_tmp

        ; PB4 = 1 (disable IJK — strobe imprimante au repos)
        lda  VIA_ORB
        ora  #$10
        sta  VIA_ORB

        ; Présence : bit5 = 0 si interface branchée, sinon ignorer
        lda  kb_tmp
        and  #$20
        bne  @ijk_done

        ; Décodage : inversion (actif haut) puis mapping vers le
        ; bitmask _key_state (Left→0, Right→1, Up→2 thrust,
        ; Fire→3 tir, Down→4 hyperespace).
        lda  kb_tmp
        eor  #$FF
        sta  kb_tmp

        and  #$02             ; IJK bit 1 = Left
        beq  :+
        lda  _key_state
        ora  #$01
        sta  _key_state
:       lda  kb_tmp
        and  #$01             ; IJK bit 0 = Right
        beq  :+
        lda  _key_state
        ora  #$02
        sta  _key_state
:       lda  kb_tmp
        and  #$10             ; IJK bit 4 = Up → thrust
        beq  :+
        lda  _key_state
        ora  #$04
        sta  _key_state
:       lda  kb_tmp
        and  #$04             ; IJK bit 2 = Fire → tir
        beq  :+
        lda  _key_state
        ora  #$08
        sta  _key_state
:       lda  kb_tmp
        and  #$08             ; IJK bit 3 = Down → hyperespace
        beq  @ijk_done
        lda  _key_state
        ora  #$10
        sta  _key_state
@ijk_done:

        ; Restaurer DDRA et DDRB
        lda  kb_ddra_save
        sta  VIA_DDRA
        lda  kb_ddrb_save
        sta  VIA_DDRB

        cli
        rts

;-----------------------------------------------------------------
; _key_probe — scan complet de la matrice 8×8 (Phase 40).
; Retour cc65 : A = col*8 + row de la première touche pressée
; (scan row-major : row 0 col 0..7, row 1, ...), ou $FF ; X = 0.
;
; Une écriture R14 par rangée seulement (8 psg_write au total), les
; 8 colonnes d'une rangée se testent par simple réécriture d'ORB.
;-----------------------------------------------------------------
_key_probe:
        sei
        jsr  kb_setup

        ldx  #0               ; X = row
@row:
        lda  kb_rowmask,x
        ldy  #14
        jsr  psg_write        ; préserve X et Y
        ldy  #0               ; Y = col
@col:
        lda  VIA_ORB
        and  #$F8
        sta  kb_tmp
        tya
        ora  kb_tmp
        sta  VIA_ORB
        nop
        nop
        lda  VIA_ORB
        and  #$08
        bne  @found
        iny
        cpy  #8
        bcc  @col
        inx
        cpx  #8
        bcc  @row
        lda  #$FF             ; aucune touche
        bne  @done            ; (toujours pris)

@found:
        tya                   ; code = col*8 + row
        asl
        asl
        asl
        sta  kb_tmp
        txa
        ora  kb_tmp
@done:
        pha                   ; kb_teardown/psg_write détruisent A
        jsr  kb_teardown
        pla
        ldx  #0
        cli
        rts
