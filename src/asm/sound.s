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

        ; Cas FX_FIRE
        lda  _sfx_id
        cmp  #FX_FIRE
        bne  @not_fire
        ; Tir : tone canal A, freq ~$50 (~310 Hz), durée 6 frames
        lda  #$50
        ldy  #0
        jsr  _psg_write       ; freq A lo
        lda  #$00
        ldy  #1
        jsr  _psg_write       ; freq A hi
        lda  #$0E             ; volume max canal A
        ldy  #8
        jsr  _psg_write
        lda  #$7E             ; mixer : tone A on
        ldy  #7
        jsr  _psg_write
        lda  #6
        sta  _sfx_timer
        jmp  @done
@not_fire:

        cmp  #FX_EXPLODE
        bne  @not_explode
        ; Explosion : noise canal A, freq noise basse, durée 25 frames
        lda  #$1F             ; freq noise (max = grave)
        ldy  #6
        jsr  _psg_write
        lda  #$0F             ; volume canal A max
        ldy  #8
        jsr  _psg_write
        lda  #$77             ; mixer : noise A on (bit 3=0), tone off (bit 0=1)
        ldy  #7
        jsr  _psg_write
        lda  #25
        sta  _sfx_timer
        jmp  @done
@not_explode:

        cmp  #FX_THUMP
        bne  @not_thump
        ; Thump : tone très grave canal B, durée 8 frames
        lda  #$00
        ldy  #2
        jsr  _psg_write       ; freq B lo
        lda  #$08             ; freq B hi (very low pitch)
        ldy  #3
        jsr  _psg_write
        lda  #$0E             ; volume B max
        ldy  #9
        jsr  _psg_write
        lda  #$7D             ; mixer : tone A off, tone B on
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
        ; Thrust : noise canal C, court (3 frames). Re-déclenché chaque
        ; frame que UP est held → effet de bruit continu approximatif.
        lda  #$0F              ; freq noise high
        ldy  #6
        jsr  _psg_write
        lda  #$08              ; volume C low (discret)
        ldy  #10
        jsr  _psg_write
        lda  #$3F              ; mixer : noise C on (bit 5 = 0)
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
