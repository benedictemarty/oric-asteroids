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
        .export   _irq_handler, _irq_install
        .exportzp _sfx_id, _sfx_timer, _frame_cnt

        .importzp kb_pcr_save        ; partagée avec input.s

        VIA_ORB    = $0300
        VIA_ORA    = $0301
        VIA_DDRA   = $0303
        VIA_PCR    = $030C

        ; SFX IDs
        FX_NONE        = 0
        FX_FIRE        = 1
        FX_EXPLODE     = 2    ; Étape sons 1 — large bang (R6=12, ≈167 Hz arcade)
        FX_THUMP       = 3
        FX_HYPER       = 4    ; Phase 9b — whoosh hyperespace
        FX_THRUST      = 5    ; Phase 9f — noise court continu (override par hold)
        FX_LIFE        = 6    ; Phase 9f — chime extra ship
        FX_UFO         = 7    ; Phase 10n — bip-bip UFO (canal C, re-déclenché)
        FX_BANG_MEDIUM = 8    ; Étape sons 1 — medium bang (R6=8, ≈246 Hz)
        FX_BANG_SMALL  = 9    ; Étape sons 1 — small bang  (R6=6, ≈306 Hz)
        FX_THUMP_2     = 10   ; Étape sons 4 — Beat2 (sym. au FX_THUMP/Beat1)
        FX_UFO_SMALL   = 11   ; Étape sons 5 — petit UFO (sweep MONTANT)
        ; Note : FX_THUMP (3) = Beat1 ; FX_UFO (7) = large UFO (sweep descendant)

;-----------------------------------------------------------------
; Variables ZP
;-----------------------------------------------------------------
        .zeropage
_sfx_id:    .res 1
_sfx_timer: .res 1
sound_tmp:  .res 1
_frame_cnt: .res 1     ; incrémenté par _irq_handler à chaque tick T1 (50 Hz)
sweep_idx:  .res 1     ; index générique des sweeps (FX_LIFE / FX_THUMP / FX_UFO)

;-----------------------------------------------------------------
; Tables RODATA — sweeps arcade-fidèles (cf. docs/arcade-sounds-analysis.md)
;
; Period AY = 1 MHz / (16 × freq_Hz). Toutes les tables sont indexées
; par sweep_idx. Index 0 = valeur initiale (écrite par play_fx), index N
; = valeur appliquée au tick N par sound_tick. Au-delà du max, sweep
; arrêté (period reste figée jusqu'au timeout du fx).
;
; FX_LIFE (canal A, R0/R1, 4 entries) — extra ship whoosh descendant :
;   0: 2786 Hz / 1: 1393 Hz / 2: 696 Hz / 3: 348 Hz
;
; FX_THUMP (canal B, R2/R3, 2 entries) — beat1, 134→81 Hz :
;   0: period $01D2 / 1: $0304
;
; FX_THUMP_2 (canal B, R2/R3, 2 entries) — beat2, 129→77 Hz :
;   0: period $01E4 / 1: $032C
;
; FX_UFO (canal C, R4/R5, 2 entries) — large UFO, 1259→879 Hz descendant :
;   0: period $0031 / 1: $0047
;
; FX_UFO_SMALL (canal C, R4/R5, 2 entries) — small UFO, 983→1354 Hz MONTANT :
;   0: period $003F / 1: $002E
;-----------------------------------------------------------------
        .segment "RODATA"
life_per_lo:
        .byte $16, $2D, $5A, $B3
life_per_hi:
        .byte $00, $00, $00, $00
thump1_per_lo:
        .byte $D2, $04
thump1_per_hi:
        .byte $01, $03
thump2_per_lo:
        .byte $E4, $2C
thump2_per_hi:
        .byte $01, $03
; UFO Large — oscillation 1000/800/1000/800 Hz (4 entries), bip-bip arcade.
; Periods : 1000 Hz → $3E ; 800 Hz → $4E.
ufo_l_per_lo:
        .byte $3E, $4E, $3E, $4E
ufo_l_per_hi:
        .byte $00, $00, $00, $00
; UFO Small — sweep MONTANT 700→1300 Hz (4 entries).
; Periods : 700 Hz → $59 ; 1000 → $3E ; 1100 → $39 ; 1300 → $30.
ufo_s_per_lo:
        .byte $59, $3E, $39, $30
ufo_s_per_hi:
        .byte $00, $00, $00, $00

; FX_FIRE noise sweep — table R6 décalée vers l'aigu pour bien distinguer
; de l'EXPLODE qui démarre à R6=02 (~977 Hz). Table à valeurs ≤ 02 :
;   $01 = 1953 Hz / $02 = 977 Hz
; Profil "psssht" aigu qui module rapidement, vs EXPLODE qui s'enfonce
; dans le grave.
fire_noise_per:
        .byte $01, $01, $02, $02, $01, $01, $01

; Explosions — table R6 noise pour profil "impact aigu + corps grave".
; Arcade : 1ère frame ~1000 Hz (R6=2), corps suivant ~150/250/300 Hz selon
; taille (R6=12/8/6). 2 entries par taille, le hook sweep s'arrête à idx=2.
;
; bang_large : impact $02 puis corps $0C (167 Hz)
; bang_med   : impact $02 puis corps $08 (246 Hz)
; bang_small : impact $02 puis corps $06 (306 Hz)
bang_l_noise_per:
        .byte $02, $0C
bang_m_noise_per:
        .byte $02, $08
bang_s_noise_per:
        .byte $02, $06

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

        ; Cas FX_FIRE — "psssht" aigu modulé. Table fire_noise_per pilote
        ; R6 au fil des ticks. Volume A en mode enveloppe pour fade-out
        ; naturel (vs cut sec). Mixer = noise A only (différent de l'EXPLODE
        ; qui ouvre noise A+B+C avec vol C en enveloppe).
        lda  _sfx_id
        cmp  #FX_FIRE
        bne  @not_fire
        lda  fire_noise_per           ; R6 init = $01 (aigu)
        ldy  #6
        jsr  _psg_write
        lda  #$10                     ; volume A en mode enveloppe
        ldy  #8
        jsr  _psg_write
        lda  #$45                     ; env period lo (~280 ms à 25 Hz)
        ldy  #11
        jsr  _psg_write
        lda  #$04                     ; env period hi
        ldy  #12
        jsr  _psg_write
        lda  #$00                     ; env shape : \___ decay + hold
        ldy  #13
        jsr  _psg_write
        lda  #$77                     ; mixer : NOISE A ONLY (bit 3 = 0,
                                       ; bits 4-5 = 1 noise B/C off) + port A in
        ldy  #7
        jsr  _psg_write
        lda  #1
        sta  sweep_idx        ; tick suivant utilisera fire_noise_per[1]
        lda  #6
        sta  _sfx_timer       ; 6 ticks (+ init) = 7 frames ≈ 280 ms
        jmp  @done
@not_fire:

        cmp  #FX_EXPLODE
        bne  @not_explode_large
        ; Large bang — impact $02 (~1000 Hz) puis corps $0C (167 Hz).
        lda  #$02              ; R6 impact aigu (entry 0 de bang_l)
        jmp  @bang_setup
@not_explode_large:

        cmp  #FX_BANG_MEDIUM
        bne  @not_bang_med
        ; Medium bang — impact $02 puis corps $08 (246 Hz).
        lda  #$02
        jmp  @bang_setup
@not_bang_med:

        cmp  #FX_BANG_SMALL
        bne  @not_bang_small
        ; Small bang — impact $02 puis corps $06 (306 Hz).
        lda  #$02
@bang_setup:
        ; A = R6 impact ($02 pour tous). Setup commun :
        ; reg 7=$47 (mixer noise A+B+C + port A input), reg 10=$10
        ; (vol C en mode enveloppe), regs 11-12=$0038 (envelope period),
        ; reg 13=$00 (decay + hold à 0). Fade-out AY natif.
        ; Le hook sweep dans sound_tick basculera vers le corps grave
        ; à la frame suivante (entry 1 de la table selon FX_id).
        ldy  #6
        jsr  _psg_write       ; noise period (impact = $02)
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
        lda  #1
        sta  sweep_idx        ; next tick : corps grave (entry 1)
        lda  #25
        sta  _sfx_timer
        jmp  @done
@not_bang_small:

        cmp  #FX_THUMP
        bne  @not_thump
        ; Beat1 arcade — SWEEP descendant 134→81 Hz, ~74 ms.
        ; Tone canal B, period from thump1_per[] via sweep_idx.
        ; Phase 21 — enveloppe \___ pour decay percussif naturel (était cut sec).
        ; Period env $0A → cycle 32 steps × 256 × 10 / 1MHz ≈ 82 ms ≈ 2 frames.
        lda  thump1_per_lo
        ldy  #2
        jsr  _psg_write       ; period init (R2 lo)
        lda  thump1_per_hi
        ldy  #3
        jsr  _psg_write       ; period init (R3 hi)
        lda  #$10
        ldy  #9
        jsr  _psg_write       ; vol B en mode enveloppe (était $0F fixe)
        lda  #$0A
        ldy  #11
        jsr  _psg_write       ; env period lo (~82 ms decay)
        lda  #$00
        ldy  #12
        jsr  _psg_write       ; env period hi
        lda  #$00
        ldy  #13
        jsr  _psg_write       ; env shape \___
        lda  #$7D             ; mixer : tone B on, port A input
        ldy  #7
        jsr  _psg_write
        lda  #1
        sta  sweep_idx        ; next tick uses thump1_per[1]
        lda  #2
        sta  _sfx_timer       ; 2 ticks (init + 1 sweep + decay = ~3 frames)
        jmp  @done
@not_thump:

        cmp  #FX_THUMP_2
        bne  @not_thump2
        ; Beat2 arcade — SWEEP descendant 129→77 Hz, ~78 ms (sym. Beat1).
        ; Phase 21 — enveloppe \___ pour decay percussif (sym. Beat1).
        lda  thump2_per_lo
        ldy  #2
        jsr  _psg_write
        lda  thump2_per_hi
        ldy  #3
        jsr  _psg_write
        lda  #$10
        ldy  #9
        jsr  _psg_write       ; vol B en mode enveloppe
        lda  #$0A
        ldy  #11
        jsr  _psg_write       ; env period lo (~82 ms)
        lda  #$00
        ldy  #12
        jsr  _psg_write
        lda  #$00
        ldy  #13
        jsr  _psg_write       ; env shape \___
        lda  #$7D
        ldy  #7
        jsr  _psg_write
        lda  #1
        sta  sweep_idx
        lda  #2
        sta  _sfx_timer
        jmp  @done
@not_thump2:

        cmp  #FX_HYPER
        bne  @not_hyper
        ; Hyperespace : noise + tone sweep grave, durée 14 frames (~560 ms)
        ; Phase 21 — enveloppe AY pour decay naturel \___ au lieu du cut sec
        ; Période env $0044 : un cycle de 32 steps = 32 × 256 × 68 / 1MHz
        ; ≈ 558 ms ≈ 14 frames @ 25 Hz. Shape $00 = decay + hold low.
        lda  #$10              ; freq noise medium
        ldy  #6
        jsr  _psg_write
        lda  #$80              ; freq tone A lo (basse)
        ldy  #0
        jsr  _psg_write
        lda  #$01              ; freq tone A hi
        ldy  #1
        jsr  _psg_write
        lda  #$10              ; volume A en mode enveloppe (bit 4 set)
        ldy  #8
        jsr  _psg_write
        lda  #$44              ; env period lo (~558 ms decay)
        ldy  #11
        jsr  _psg_write
        lda  #$00              ; env period hi
        ldy  #12
        jsr  _psg_write
        lda  #$00              ; env shape \___ (decay + hold low)
        ldy  #13
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
        ; Arcade : TONE 82 Hz rumble grave, 288 ms. Pas de noise principal.
        ; Period 82 Hz = 1 MHz / (16 × 82) ≈ 762 = $02FA.
        ; Canal A, tone only. Re-déclenché chaque ~3-7 frames par game.c
        ; pendant que UP est maintenu (effet continu).
        lda  #$FA
        ldy  #0
        jsr  _psg_write       ; freq A lo
        lda  #$02
        ldy  #1
        jsr  _psg_write       ; freq A hi
        lda  #$0E
        ldy  #8
        jsr  _psg_write       ; volume A presque max
        lda  #$7E             ; mixer : tone A only + port A input
        ldy  #7
        jsr  _psg_write
        lda  #7
        sta  _sfx_timer       ; ~280 ms si pas re-déclenché
        jmp  @done
@not_thrust:

        cmp  #FX_LIFE
        bne  @not_life
        ; Extra ship — re-analysé depuis MP3 22 kHz arcade :
        ; TONE PLATEAU à ~2956 Hz pendant ~85 ms puis fade-out rapide.
        ; (Le sweep descendant déduit du .wav 11 kHz dégradé était faux.)
        ;
        ; Period 2956 Hz = 1 MHz / (16 × 2956) ≈ 21 = $0015.
        ; Canal A en mode enveloppe pour fade-out naturel :
        ;   R8 = $10 → vol piloté par enveloppe
        ;   R11/R12 = $0080 → period 128 × 256 µs = 32.8 ms par phase
        ;   R13 = $00 → shape \___ (decay puis hold à 0)
        ; Timer = 4 frames (~160 ms à 25 Hz) — couvre attaque + fade.
        ; sweep_idx mis à 4 pour court-circuiter le hook sweep sound_tick.
        lda  #$15
        ldy  #0
        jsr  _psg_write       ; R0 period lo
        lda  #$00
        ldy  #1
        jsr  _psg_write       ; R1 period hi
        lda  #$10
        ldy  #8
        jsr  _psg_write       ; vol A en mode enveloppe
        lda  #$80
        ldy  #11
        jsr  _psg_write       ; env period lo
        lda  #$00
        ldy  #12
        jsr  _psg_write       ; env period hi
        lda  #$00
        ldy  #13
        jsr  _psg_write       ; env shape : decay + hold
        lda  #$7E             ; mixer : tone A on, port A input
        ldy  #7
        jsr  _psg_write
        lda  #4
        sta  sweep_idx        ; ≥ 4 : pas de sweep dans sound_tick
        lda  #4
        sta  _sfx_timer
        jmp  @done
@not_life:

        cmp  #FX_UFO
        bne  @not_ufo_large
        ; Large UFO — SWEEP descendant 1259→879 Hz, ~168 ms (menaçant).
        ; Tone canal C, periods depuis ufo_l_per[].
        lda  ufo_l_per_lo
        ldy  #4
        jsr  _psg_write       ; period init (R4 lo)
        lda  ufo_l_per_hi
        ldy  #5
        jsr  _psg_write       ; period init (R5 hi)
        lda  #$0A              ; volume C modéré
        ldy  #10
        jsr  _psg_write
        lda  #$7B              ; mixer : tone C on, port A input
        ldy  #7
        jsr  _psg_write
        lda  #1
        sta  sweep_idx        ; next tick uses ufo_l_per[1]
        lda  #4
        sta  _sfx_timer       ; 4 ticks pour parcourir les 4 entries (~5 frames)
        jmp  @done
@not_ufo_large:

        cmp  #FX_UFO_SMALL
        bne  @not_ufo_small
        ; Small UFO — SWEEP MONTANT 983→1354 Hz, ~124 ms (nerveux).
        lda  ufo_s_per_lo
        ldy  #4
        jsr  _psg_write
        lda  ufo_s_per_hi
        ldy  #5
        jsr  _psg_write
        lda  #$0A
        ldy  #10
        jsr  _psg_write
        lda  #$7B
        ldy  #7
        jsr  _psg_write
        lda  #1
        sta  sweep_idx
        lda  #4
        sta  _sfx_timer       ; 4 ticks (~5 frames)
        jmp  @done
@not_ufo_small:

@done:
        pla
        sta  VIA_DDRA
        cli
        rts

;-----------------------------------------------------------------
; _sound_tick — appelé par _irq_handler à 50 Hz (Phase 20).
;
; Phase 21b — fix régression Phase 20 : tous les _sfx_timer ont été
; calibrés pour un sound_tick à 25 Hz (durées exprimées en frames).
; Depuis Phase 20, sound_tick tourne à 50 Hz → toutes les durées
; étaient divisées par 2 (explosion ship ~500 ms au lieu d'1 s,
; trop court pour être perçu une fois l'enveloppe AY déclenchée).
;
; Fix : skip 1 IRQ sur 2 via le LSB de _frame_cnt → cadence effective
; 25 Hz comme avant, mais le main loop reste libéré (handler IRQ minimal).
;-----------------------------------------------------------------
_sound_tick:
        lda  _frame_cnt
        and  #1
        bne  @tick_skip            ; IRQ impaire → skip
        lda  _sfx_id
        bne  @go_sweep
        jmp  @no_sfx
@tick_skip:
        rts
@go_sweep:

        ; ==============================================================
        ; Hook sweeps : pour les effets qui modifient leur period au fil
        ; des ticks (FX_LIFE / FX_THUMP / FX_THUMP_2 / FX_UFO / FX_UFO_SMALL).
        ; Doit être fait AVANT le décrément du timer pour que les ticks
        ; utiles touchent les entrées des tables.
        ;
        ; Convention : sweep_idx = index dans la table (0..max-1 entries
        ; déjà écrites, max+ = sweep terminé). À l'init dans play_fx,
        ; period[0] est écrite et sweep_idx = 1 (= prochain index à appliquer).
        ; ==============================================================
        sei
        lda  VIA_DDRA
        pha
        lda  #$FF
        sta  VIA_DDRA

        lda  _sfx_id
        ldx  sweep_idx

        ; Switch sur _sfx_id vers la routine de sweep correspondante.
        ; bne local + jmp à cause des distances qui peuvent dépasser 128.
        cmp  #FX_FIRE
        bne  @ck_bang_l
        jmp  @sw_fire
@ck_bang_l:
        cmp  #FX_EXPLODE
        bne  @ck_bang_m
        jmp  @sw_bang_l
@ck_bang_m:
        cmp  #FX_BANG_MEDIUM
        bne  @ck_bang_s
        jmp  @sw_bang_m
@ck_bang_s:
        cmp  #FX_BANG_SMALL
        bne  @ck_life
        jmp  @sw_bang_s
@ck_life:
        cmp  #FX_LIFE
        bne  @ck_thump1
        jmp  @sw_life
@ck_thump1:
        cmp  #FX_THUMP
        bne  @ck_thump2
        jmp  @sw_thump1
@ck_thump2:
        cmp  #FX_THUMP_2
        bne  @ck_ufo_l
        jmp  @sw_thump2
@ck_ufo_l:
        cmp  #FX_UFO
        bne  @ck_ufo_s
        jmp  @sw_ufo_l
@ck_ufo_s:
        cmp  #FX_UFO_SMALL
        bne  @sw_no_match
        jmp  @sw_ufo_s
@sw_no_match:
        jmp  @sw_done                 ; pas de sweep pour ce fx

@sw_fire:
        cpx  #7
        bcc  @sw_fire_ok
        jmp  @sw_done
@sw_fire_ok:
        lda  fire_noise_per,x
        ldy  #6                        ; R6 = noise period
        jsr  _psg_write
        inc  sweep_idx
        jmp  @sw_done

@sw_bang_l:
        cpx  #2
        bcc  @sw_bang_l_ok
        jmp  @sw_done
@sw_bang_l_ok:
        lda  bang_l_noise_per,x
        ldy  #6
        jsr  _psg_write
        inc  sweep_idx
        jmp  @sw_done

@sw_bang_m:
        cpx  #2
        bcc  @sw_bang_m_ok
        jmp  @sw_done
@sw_bang_m_ok:
        lda  bang_m_noise_per,x
        ldy  #6
        jsr  _psg_write
        inc  sweep_idx
        jmp  @sw_done

@sw_bang_s:
        cpx  #2
        bcc  @sw_bang_s_ok
        jmp  @sw_done
@sw_bang_s_ok:
        lda  bang_s_noise_per,x
        ldy  #6
        jsr  _psg_write
        inc  sweep_idx
        jmp  @sw_done

@sw_life:
        cpx  #4
        bcs  @sw_done
        lda  life_per_lo,x
        ldy  #0
        jsr  _psg_write
        lda  life_per_hi,x
        ldy  #1
        jsr  _psg_write
        inc  sweep_idx
        jmp  @sw_done

@sw_thump1:
        cpx  #2
        bcs  @sw_done
        lda  thump1_per_lo,x
        ldy  #2
        jsr  _psg_write
        lda  thump1_per_hi,x
        ldy  #3
        jsr  _psg_write
        inc  sweep_idx
        jmp  @sw_done

@sw_thump2:
        cpx  #2
        bcs  @sw_done
        lda  thump2_per_lo,x
        ldy  #2
        jsr  _psg_write
        lda  thump2_per_hi,x
        ldy  #3
        jsr  _psg_write
        inc  sweep_idx
        jmp  @sw_done

@sw_ufo_l:
        cpx  #4
        bcs  @sw_done
        lda  ufo_l_per_lo,x
        ldy  #4
        jsr  _psg_write
        lda  ufo_l_per_hi,x
        ldy  #5
        jsr  _psg_write
        inc  sweep_idx
        jmp  @sw_done

@sw_ufo_s:
        cpx  #4
        bcs  @sw_done
        lda  ufo_s_per_lo,x
        ldy  #4
        jsr  _psg_write
        lda  ufo_s_per_hi,x
        ldy  #5
        jsr  _psg_write
        inc  sweep_idx
@sw_done:
        pla
        sta  VIA_DDRA
        cli

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

;-----------------------------------------------------------------
; _irq_handler — Handler IRQ T1 portable Oric-1 / Atmos
;
; Contrat (validé équipe Phosphoric, cf docs/notes/2026-05-13-asteroids-
; irq-debug-response.md) :
; - Le CPU 6502 push automatiquement PC + P sur IRQ (3 bytes)
; - Le CPU dispatch DIRECTEMENT vers $0228 (Oric-1) / $0244 (Atmos)
;   via le vecteur en $FFFE-$FFFF
; - Quand on patche $0228/$0244, la ROM est ENTIÈREMENT bypassée
; - Notre handler est le PREMIER code exécuté : il DOIT push A/X/Y
;   et les restaurer avant RTI
;-----------------------------------------------------------------
        VIA_IFR    = $030D
        VIA_T1CL   = $0304

_irq_handler:
        pha                       ; save A
        txa
        pha                       ; save X
        tya
        pha                       ; save Y

        lda  VIA_IFR
        and  #$40                 ; T1 flag set ?
        beq  @not_t1
        lda  VIA_T1CL             ; lire T1CL -> clear T1 flag dans IFR
        jsr  _sound_tick
        inc  _frame_cnt
@not_t1:
        pla                       ; restore Y
        tay
        pla                       ; restore X
        tax
        pla                       ; restore A
        rti

;-----------------------------------------------------------------
; _irq_install — installe _irq_handler aux vecteurs ROM (Oric-1 et Atmos),
; désactive toutes les autres sources VIA IRQ, active T1 IRQ, CLI.
;
; Fix Phosphoric : disable+clear toutes les sources VIA IRQ AVANT
; d'activer T1, sinon une IRQ CB1/CA1/T2 résiduelle de la ROM fire,
; notre handler skip @not_t1 sans clear -> boucle infinie.
;-----------------------------------------------------------------
        VIA_IER    = $030E

_irq_install:
        sei                       ; CPU IRQ disabled pendant config

        ; Fix Phosphoric Q3 : désactiver TOUTES les sources VIA IRQ
        lda  #$7F                 ; bit 7=0 (clear mode) + bits 0-6=1 (toutes sources)
        sta  VIA_IER              ; -> disable T1, T2, CB1, CB2, SR, CA1, CA2
        lda  #$7F                 ; clear tous les flags IFR
        sta  VIA_IFR

        ; Patch Oric-1 vecteur IRQ ($0228) : JMP _irq_handler
        ; Note : c'est exactement ce que la ROM écrit elle-même au boot
        ; (`4C 03 EC` = JMP $EC03). On remplace par notre handler.
        lda  #$4C                 ; opcode JMP abs
        sta  $0228
        lda  #<_irq_handler
        sta  $0229
        lda  #>_irq_handler
        sta  $022A

        ; Patch Atmos vecteur IRQ ($0244) : JMP _irq_handler
        lda  #$4C
        sta  $0244
        lda  #<_irq_handler
        sta  $0245
        lda  #>_irq_handler
        sta  $0246

        ; Activer T1 IRQ uniquement (bit 7=1 set, bit 6=1 T1)
        lda  #$C0
        sta  VIA_IER

        ; Clear T1 flag initial pour partir propre
        lda  VIA_T1CL

        ; frame_cnt = 0
        lda  #0
        sta  _frame_cnt

        cli                       ; CPU IRQ enabled
        rts
