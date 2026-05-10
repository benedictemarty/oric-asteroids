;=================================================================
; sound.s — Driver PSG (AY-3-8912) pour Asteroids Oric-1
; Phase 8 — effets sonores : tir, explosion, thump
;
; Architecture PSG sur Oric-1 (cf. input.s) :
;   - VIA Port A ($0301) = bus de données 8 bits
;   - VIA CA2 (PCR bits 1-3) = BC1
;   - VIA CB2 (PCR bits 5-7) = BDIR
;   - Modes : Inactive ($CC), Latch Address ($EE), Write Data ($EC)
;
; Registres AY-3-8912 :
;   $00/$01  Freq canal A (lo, hi 4 bits)
;   $02/$03  Freq canal B
;   $04/$05  Freq canal C
;   $06      Freq noise (5 bits)
;   $07      Mixer (bits 0-2 tone enable inv, 3-5 noise enable inv,
;                   6 = port A direction (1=input), 7 = port B)
;   $08/$09  Volume canal A/B (0-15, bit 4 = enveloppe)
;   $0A      Volume canal C
;   $0B/$0C  Enveloppe période
;   $0D      Enveloppe shape
;
; Politique : 1 effet à la fois (sfx_id != 0), simplification Phase 8.
; Le scan clavier (input.s) modifie aussi reg 14 et 7 — pas de conflit
; tant que sound_tick n'est pas appelé pendant key_scan (séquentiel ok).
;=================================================================

        .export   _psg_write, _sound_init, _sound_tick
        .export   _sound_play_fx
        .export   _tune_play_note, _tune_stop
        .exportzp _sfx_id, _sfx_timer

        .importzp kb_pcr_save        ; partagée avec input.s

        VIA_ORB    = $0300
        VIA_ORA    = $0301
        VIA_DDRA   = $0303
        VIA_PCR    = $030C

        ; SFX IDs
        FX_NONE    = 0
        FX_FIRE    = 1
        FX_EXPLODE = 2
        FX_THUMP   = 3
        FX_HYPER   = 4    ; Phase 9b — whoosh hyperespace
        FX_THRUST  = 5    ; Phase 9f — noise court continu (override par hold)
        FX_LIFE    = 6    ; Phase 9f — chime extra ship
        FX_UFO     = 7    ; Phase 10n — bip-bip UFO (canal C, re-déclenché)

;-----------------------------------------------------------------
; Variables ZP
;-----------------------------------------------------------------
        .zeropage
_sfx_id:    .res 1
_sfx_timer: .res 1
sound_tmp:  .res 1

;-----------------------------------------------------------------
; _psg_write — A = valeur, Y = numéro de registre
;
; Préserve : Y. Détruit : A.
; SEI/CLI géré par appelant si nécessaire (key_scan le fait déjà).
; Suppose DDRA = $FF avant l'appel ; restore par appelant si besoin.
;-----------------------------------------------------------------
        .segment "CODE"

_psg_write:
        sta  sound_tmp        ; sauvegarder valeur
        tya
        sta  VIA_ORA          ; ORA = numéro de registre

        lda  kb_pcr_save
        ora  #$EE             ; Latch Address (BDIR=1, BC1=1)
        sta  VIA_PCR
        lda  kb_pcr_save
        ora  #$CC             ; Inactive
        sta  VIA_PCR

        lda  sound_tmp
        sta  VIA_ORA          ; ORA = valeur

        lda  kb_pcr_save
        ora  #$EC             ; Write Data (BDIR=1, BC1=0)
        sta  VIA_PCR
        lda  kb_pcr_save
        ora  #$CC             ; Inactive
        sta  VIA_PCR
        rts

;-----------------------------------------------------------------
; _sound_init — config initiale PSG
;
; Sauvegarde DDRA, met DDRA=$FF, écrit registres PSG, restore DDRA.
; Mixer reg 7 = $7F : tous canaux désactivés, port A input (clavier).
;-----------------------------------------------------------------
_sound_init:
        sei

        ; Init kb_pcr_save (au cas où sound_init appelé avant key_scan)
        lda  VIA_PCR
        and  #$11
        sta  kb_pcr_save

        ; Sauvegarder DDRA, forcer output
        lda  VIA_DDRA
        pha
        lda  #$FF
        sta  VIA_DDRA

        ; Volumes canaux A/B/C = 0 (silence)
        lda  #0
        ldy  #8
        jsr  _psg_write
        lda  #0
        ldy  #9
        jsr  _psg_write
        lda  #0
        ldy  #10
        jsr  _psg_write

        ; Reg 7 : mixer = $7F (tout off, port A in, port B out)
        lda  #$7F
        ldy  #7
        jsr  _psg_write

        ; État SFX = aucun
        lda  #FX_NONE
        sta  _sfx_id
        lda  #0
        sta  _sfx_timer

        ; Restore DDRA
        pla
        sta  VIA_DDRA
        cli
        rts

;-----------------------------------------------------------------
; _sound_play_fx — A = ID effet, démarrer
;
; Initialise sfx_id, sfx_timer + écrit registres PSG initiaux selon FX.
; Phase 8 : un seul effet à la fois (override le précédent).
;-----------------------------------------------------------------
_sound_play_fx:
        sta  _sfx_id

        sei
        lda  VIA_DDRA
        pha
        lda  #$FF
        sta  VIA_DDRA

        ; Cas FX_FIRE — port Mine Storm Vectrex SS.BLT (rev C)
        lda  _sfx_id
        cmp  #FX_FIRE
        bne  @not_fire
        ; SS.BLT : reg 2=$39 (tone B freq lo), reg 3=$00 (tone B hi),
        ;          reg 6=$1F (noise grave), reg 7=$05+$40 (mixer : tone B on
        ;          + noise A on + port A input pour scan clavier Oric),
        ;          reg 9=$0F (volume B max). Durée 6 frames.
        lda  #$39
        ldy  #2
        jsr  _psg_write       ; freq B lo
        lda  #$00
        ldy  #3
        jsr  _psg_write       ; freq B hi
        lda  #$1F
        ldy  #6
        jsr  _psg_write       ; noise grave max
        lda  #$0F
        ldy  #9
        jsr  _psg_write       ; volume B max
        lda  #$45             ; mixer Vectrex $05 + bit 6 (port A input Oric)
        ldy  #7
        jsr  _psg_write
        lda  #6
        sta  _sfx_timer
        jmp  @done
@not_fire:

        cmp  #FX_EXPLODE
        bne  @not_explode
        ; SS.EXP Mine Storm : reg 6=$1F (noise grave), reg 7=$07+$40
        ; (mixer noise tous canaux, port A input), reg 10=$10 (vol C
        ; en mode enveloppe), regs 11-12=$0038 (envelope period), reg
        ; 13=$00 (envelope shape : decay puis hold à 0). Fade-out AY natif.
        lda  #$1F
        ldy  #6
        jsr  _psg_write       ; noise grave max
        lda  #$10              ; volume C en mode enveloppe
        ldy  #10
        jsr  _psg_write
        lda  #$00              ; envelope period lo
        ldy  #11
        jsr  _psg_write
        lda  #$38              ; envelope period hi
        ldy  #12
        jsr  _psg_write
        lda  #$00              ; envelope shape : \___  (decay + hold)
        ldy  #13
        jsr  _psg_write
        lda  #$47              ; mixer noise tous canaux + port A input
        ldy  #7
        jsr  _psg_write
        lda  #25
        sta  _sfx_timer
        jmp  @done
@not_explode:

        cmp  #FX_THUMP
        bne  @not_thump
        ; SS.POP Mine Storm (mine pop) — adapté pour notre thump :
        ; reg 2=$30 (tone B freq lo grave), reg 3=$00, reg 6=$1F, reg
        ; 7=$3D+$40 (mixer tone B on uniquement), reg 9=$0F. Durée 8.
        lda  #$30
        ldy  #2
        jsr  _psg_write       ; freq B lo (~155 Hz à 1.79 MHz / 16)
        lda  #$00
        ldy  #3
        jsr  _psg_write
        lda  #$1F
        ldy  #6
        jsr  _psg_write       ; noise (silencieux car bits 4-5 = 1)
        lda  #$0F
        ldy  #9
        jsr  _psg_write       ; volume B max
        lda  #$7D             ; mixer : tone B on, port A input ($3D + $40)
        ldy  #7
        jsr  _psg_write
        lda  #8
        sta  _sfx_timer
        jmp  @done
@not_thump:

        cmp  #FX_HYPER
        bne  @not_hyper
        ; Hyperespace : noise + tone sweep grave, durée 14 frames
        lda  #$10              ; freq noise medium
        ldy  #6
        jsr  _psg_write
        lda  #$80              ; freq tone A lo (basse)
        ldy  #0
        jsr  _psg_write
        lda  #$01              ; freq tone A hi
        ldy  #1
        jsr  _psg_write
        lda  #$0E              ; volume A
        ldy  #8
        jsr  _psg_write
        lda  #$76              ; mixer : tone A on (bit 0 = 0) + noise A on (bit 3 = 0)
        ldy  #7
        jsr  _psg_write
        lda  #14
        sta  _sfx_timer
        jmp  @done
@not_hyper:

        cmp  #FX_THRUST
        bne  @not_thrust
        ; SS.THR Mine Storm : reg 0=$10, reg 1=$00 (tone A grave), reg
        ; 6=$1F (noise grave), reg 7=$06+$40 (tone A + noise A+B+C on,
        ; port A input), reg 8=$0F (volume A max). Re-déclenché 3 frames.
        lda  #$10
        ldy  #0
        jsr  _psg_write       ; freq A lo
        lda  #$00
        ldy  #1
        jsr  _psg_write
        lda  #$1F
        ldy  #6
        jsr  _psg_write
        lda  #$0F
        ldy  #8
        jsr  _psg_write       ; volume A max
        lda  #$46             ; mixer Vectrex $06 + port A input Oric
        ldy  #7
        jsr  _psg_write
        lda  #3
        sta  _sfx_timer
        jmp  @done
@not_thrust:

        cmp  #FX_LIFE
        bne  @not_life
        ; Extra ship : chime tone aigu canal A, durée 20 frames
        lda  #$30
        ldy  #0
        jsr  _psg_write
        lda  #$00
        ldy  #1
        jsr  _psg_write
        lda  #$0C
        ldy  #8
        jsr  _psg_write
        lda  #$7E              ; mixer : tone A on
        ldy  #7
        jsr  _psg_write
        lda  #20
        sta  _sfx_timer
        jmp  @done
@not_life:

        cmp  #FX_UFO
        bne  @not_ufo
        ; UFO : tone canal C grave (freq $C0), durée 4 frames.
        ; Re-déclenché par game.c chaque ~16 frames pour effet bip-bip continu.
        lda  #$C0
        ldy  #4
        jsr  _psg_write       ; freq C lo
        lda  #$00
        ldy  #5
        jsr  _psg_write       ; freq C hi
        lda  #$0A              ; volume C modéré
        ldy  #10
        jsr  _psg_write
        lda  #$7B              ; mixer : tone C on (bit 2 = 0), autres off
        ldy  #7
        jsr  _psg_write
        lda  #4
        sta  _sfx_timer
        jmp  @done
@not_ufo:

@done:
        pla
        sta  VIA_DDRA
        cli
        rts

;-----------------------------------------------------------------
; _sound_tick — appelé chaque frame de jeu
;
; Si sfx_id != 0, décrémente timer.
; Quand timer = 0 : coupe le son (mixer = $7F, volumes = 0).
; Phase 8 : pas d'enveloppe progressive, juste cut net.
;-----------------------------------------------------------------
_sound_tick:
        lda  _sfx_id
        beq  @no_sfx
        lda  _sfx_timer
        beq  @stop
        dec  _sfx_timer
        bne  @keep
@stop:
        sei
        lda  VIA_DDRA
        pha
        lda  #$FF
        sta  VIA_DDRA

        ; Couper : volumes 0, mixer tout off
        lda  #0
        ldy  #8
        jsr  _psg_write
        lda  #0
        ldy  #9
        jsr  _psg_write
        lda  #0
        ldy  #10
        jsr  _psg_write
        lda  #$7F
        ldy  #7
        jsr  _psg_write

        lda  #FX_NONE
        sta  _sfx_id

        pla
        sta  VIA_DDRA
        cli
@keep:
@no_sfx:
        rts

;-----------------------------------------------------------------
; _tune_play_note — A = index de note (0..6) : G2 GS2 A2 AS2 B2 C3 CS3
;
; Joue la note sur le canal A du PSG (regs 0/1 = freq, reg 8 = volume).
; Mixer (reg 7) configuré : tone A on, autres off, port A input (clavier OK).
; Pour Oric à 1 MHz (PSG clock).
;
; Cette fonction NE touche PAS sfx_id/sfx_timer : la note reste jouée
; jusqu'au prochain _tune_play_note ou _tune_stop. La mélodie est gérée
; côté C (compteur de frames, séquence de notes).
;-----------------------------------------------------------------

; Tables fréquence PSG pour notes (1 MHz / (16 × freq_Hz)) :
;   G2  98.00 Hz  → 638 = $027E
;   GS2 103.83 Hz → 602 = $025A
;   A2  110.00 Hz → 568 = $0238
;   AS2 116.54 Hz → 536 = $0218
;   B2  123.47 Hz → 506 = $01FA
;   C3  130.81 Hz → 478 = $01DE
;   CS3 138.59 Hz → 451 = $01C3
note_lo:  .byte $7E, $5A, $38, $18, $FA, $DE, $C3
note_hi:  .byte $02, $02, $02, $02, $01, $01, $01

_tune_play_note:
        sta  sound_tmp        ; sauvegarder index
        sei

        ; Init kb_pcr_save (au cas où aucun key_scan/sound_init n'a tourné)
        lda  VIA_PCR
        and  #$11
        sta  kb_pcr_save

        ; DDRA = $FF pour les écritures PSG
        lda  VIA_DDRA
        pha
        lda  #$FF
        sta  VIA_DDRA

        ; reg 0 = note_lo[idx]
        ldx  sound_tmp
        lda  note_lo,x
        ldy  #0
        jsr  _psg_write

        ; reg 1 = note_hi[idx]
        ldx  sound_tmp
        lda  note_hi,x
        ldy  #1
        jsr  _psg_write

        ; reg 8 = volume A (10)
        lda  #$0A
        ldy  #8
        jsr  _psg_write

        ; reg 7 = mixer (tone A on bit 0=0, autres off, port A input bit 6=1)
        lda  #$7E
        ldy  #7
        jsr  _psg_write

        pla
        sta  VIA_DDRA
        cli
        rts

;-----------------------------------------------------------------
; _tune_stop — coupe la note (silence canal A)
;-----------------------------------------------------------------
_tune_stop:
        sei
        lda  VIA_PCR
        and  #$11
        sta  kb_pcr_save

        lda  VIA_DDRA
        pha
        lda  #$FF
        sta  VIA_DDRA

        lda  #$00              ; volume A = 0
        ldy  #8
        jsr  _psg_write
        lda  #$7F              ; mixer tout off, port A input
        ldy  #7
        jsr  _psg_write

        pla
        sta  VIA_DDRA
        cli
        rts
