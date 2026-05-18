;=================================================================
; sound.s — Driver PSG (AY-3-8912) pour Asteroids Oric-1
; Phase 22 — architecture 3 canaux indépendants
;
; VIA Port A ($0301) = bus données ; CA2 (PCR bits 1-3) = BC1 ;
; CB2 (PCR bits 5-7) = BDIR.
; Modes PCR : Inactive ($CC), Latch Address ($EE), Write Data ($EC).
;
; Registres AY-3-8912 :
;   R0/R1  Freq canal A    R2/R3  Freq canal B    R4/R5  Freq canal C
;   R6     Freq noise (5b) R7     Mixer           R8-R10 Volume A/B/C
;   R11/R12 Env période    R13    Env shape
;
; Canaux (Phase 22) :
;   A = effets primaires (fire, explode S/M/L, hyper, thrust, life)
;   B = thump dédié (Beat1/Beat2) — jamais interrompu par A
;   C = UFO dédié (large/small) — auto-restart tant que ufo_snd_act=1
;
; R7 est recalculé par update_mixer à chaque changement d'état de canal.
;=================================================================

        .export   _psg_write, _sound_init, _sound_tick
        .export   _sound_play_fx, _sound_stop_ufo
        .export   _tune_play_note, _tune_stop
        .export   _irq_handler, _irq_install
        .exportzp _sfx_id, _sfx_timer, _frame_cnt

        .importzp kb_pcr_save

        VIA_ORA    = $0301
        VIA_DDRA   = $0303
        VIA_PCR    = $030C
        VIA_IFR    = $030D
        VIA_IER    = $030E
        VIA_T1CL   = $0304

        FX_NONE        = 0
        FX_FIRE        = 1
        FX_EXPLODE     = 2    ; large bang  (R6=$0C ≈167 Hz) — canal A
        FX_THUMP       = 3    ; beat1       sweep 134→81 Hz   — canal B
        FX_HYPER       = 4    ; hyperespace tone+noise         — canal A
        FX_THRUST      = 5    ; rumble grave 82 Hz            — canal A
        FX_LIFE        = 6    ; chime extra ship               — canal A
        FX_UFO         = 7    ; large UFO oscillation          — canal C
        FX_BANG_MEDIUM = 8    ; medium bang (R6=$08 ≈246 Hz)  — canal A
        FX_BANG_SMALL  = 9    ; small bang  (R6=$06 ≈306 Hz)  — canal A
        FX_THUMP_2     = 10   ; beat2       sweep 129→77 Hz   — canal B
        FX_UFO_SMALL   = 11   ; small UFO   sweep montant     — canal C

;-----------------------------------------------------------------
; Zero Page
;-----------------------------------------------------------------
        .zeropage

; Canal A — effets primaires
_sfx_id:      .res 1     ; id effet courant (FX_*)
_sfx_timer:   .res 1     ; compteur frames restantes
mixer_a_mask: .res 1     ; masque R7 spécifique au FX en cours
sweep_a_idx:  .res 1     ; index dans la table de sweep A (255=aucun)

; Canal B — thump dédié
sfx_b_id:     .res 1     ; FX_THUMP ou FX_THUMP_2
sfx_b_timer:  .res 1
sweep_b_idx:  .res 1

; Canal C — UFO dédié
sfx_c_id:     .res 1     ; FX_UFO ou FX_UFO_SMALL
sfx_c_timer:  .res 1
sweep_c_idx:  .res 1
ufo_snd_act:  .res 1     ; 1 = UFO actif → relancer canal C à expiry

; Communes
_frame_cnt:   .res 1     ; incrémenté à 50 Hz par _irq_handler

;-----------------------------------------------------------------
; RODATA — tables de sweeps
;-----------------------------------------------------------------
; sound_tmp : scratch pour _psg_write (hors ZP — adressage absolu suffisant)
        .segment "BSS"
sound_tmp: .res 1

        .segment "RODATA"

; FX_LIFE (R0/R1, 4 entrées) — extra ship, sweep descendant
life_per_lo:  .byte $16, $2D, $5A, $B3
life_per_hi:  .byte $00, $00, $00, $00

; FX_THUMP Beat1 (R2/R3, 2 entrées) — 134→81 Hz
thump1_per_lo: .byte $D2, $04
thump1_per_hi: .byte $01, $03

; FX_THUMP_2 Beat2 (R2/R3, 2 entrées) — 129→77 Hz
thump2_per_lo: .byte $E4, $2C
thump2_per_hi: .byte $01, $03

; FX_UFO large (R4/R5, 4 entrées) — oscillation 1000/800 Hz
ufo_l_per_lo: .byte $3E, $4E, $3E, $4E
ufo_l_per_hi: .byte $00, $00, $00, $00

; FX_UFO_SMALL (R4/R5, 4 entrées) — sweep montant 700→1300 Hz
ufo_s_per_lo: .byte $59, $3E, $39, $30
ufo_s_per_hi: .byte $00, $00, $00, $00

; Explosions canal A (R6, 2 entrées) — impact aigu ($02) puis corps grave
bang_l_noise: .byte $02, $0C   ; large  ≈167 Hz
bang_m_noise: .byte $02, $08   ; medium ≈246 Hz
bang_s_noise: .byte $02, $06   ; small  ≈306 Hz

;-----------------------------------------------------------------
; CODE
;-----------------------------------------------------------------
        .segment "CODE"

;-----------------------------------------------------------------
; _psg_write — A = valeur, Y = n° registre
; Préserve Y. Détruit A. Suppose DDRA=$FF.
;-----------------------------------------------------------------
_psg_write:
        sta  sound_tmp
        tya
        sta  VIA_ORA
        lda  kb_pcr_save
        ora  #$EE
        sta  VIA_PCR
        lda  kb_pcr_save
        ora  #$CC
        sta  VIA_PCR
        lda  sound_tmp
        sta  VIA_ORA
        lda  kb_pcr_save
        ora  #$EC
        sta  VIA_PCR
        lda  kb_pcr_save
        ora  #$CC
        sta  VIA_PCR
        rts

;-----------------------------------------------------------------
; update_mixer — recalcule R7 depuis l'état des 3 canaux
;
; Appelé avec DDRA=$FF. Détruit A, Y. Préserve X.
;-----------------------------------------------------------------
update_mixer:
        lda  #$7F           ; base : tout off, port A input

        ldy  _sfx_timer     ; canal A actif ?
        beq  @no_a
        and  mixer_a_mask   ; appliquer masque spécifique au FX A
@no_a:
        ldy  sfx_b_timer    ; canal B actif ? (tone B = bit 1 clear)
        beq  @no_b
        and  #$7D
@no_b:
        ldy  sfx_c_timer    ; canal C actif ? (tone C = bit 2 clear)
        beq  @no_c
        and  #$7B
@no_c:
        ldy  #7
        jsr  _psg_write
        rts

;-----------------------------------------------------------------
; _sound_init — configuration initiale PSG + zéro variables
;-----------------------------------------------------------------
_sound_init:
        sei
        lda  VIA_PCR
        and  #$11
        sta  kb_pcr_save

        lda  VIA_DDRA
        pha
        lda  #$FF
        sta  VIA_DDRA

        ; Volumes A/B/C = 0
        lda  #0
        ldy  #8
        jsr  _psg_write
        lda  #0
        ldy  #9
        jsr  _psg_write
        lda  #0
        ldy  #10
        jsr  _psg_write

        ; Mixer = $7F (tout off)
        lda  #$7F
        ldy  #7
        jsr  _psg_write

        ; Zéro toutes les variables
        lda  #FX_NONE
        sta  _sfx_id
        sta  sfx_b_id
        sta  sfx_c_id
        lda  #0
        sta  _sfx_timer
        sta  sfx_b_timer
        sta  sfx_c_timer
        sta  ufo_snd_act
        sta  sweep_a_idx
        sta  sweep_b_idx
        sta  sweep_c_idx
        sta  mixer_a_mask
        sta  _frame_cnt

        pla
        sta  VIA_DDRA
        cli
        rts

;-----------------------------------------------------------------
; _sound_play_fx — A = FX_id, démarrer l'effet
;
; Route vers canal A, B ou C selon le FX. Override l'effet courant
; du même canal. Appelle update_mixer à la fin.
;-----------------------------------------------------------------
_sound_play_fx:
        sta  sound_tmp       ; sauvegarder argument AVANT sei
        sei
        lda  VIA_DDRA
        pha
        lda  #$FF
        sta  VIA_DDRA

        ; Router selon FX
        lda  sound_tmp
        cmp  #FX_THUMP
        beq  @route_b
        cmp  #FX_THUMP_2
        beq  @route_b
        cmp  #FX_UFO
        beq  @route_c
        cmp  #FX_UFO_SMALL
        beq  @route_c
        jmp  @route_a

; ─── Canal B : thump ──────────────────────────────────────────
@route_b:
        lda  sound_tmp
        sta  sfx_b_id

        cmp  #FX_THUMP
        bne  @b_thump2

        ; FX_THUMP — Beat1, sweep 134→81 Hz, enveloppe \___ decay
        lda  thump1_per_lo
        ldy  #2
        jsr  _psg_write
        lda  thump1_per_hi
        ldy  #3
        jsr  _psg_write
        lda  #$10           ; vol B = enveloppe
        ldy  #9
        jsr  _psg_write
        lda  #$0A           ; env period ≈82 ms decay
        ldy  #11
        jsr  _psg_write
        lda  #$00
        ldy  #12
        jsr  _psg_write
        lda  #$00           ; shape \___
        ldy  #13
        jsr  _psg_write
        lda  #1
        sta  sweep_b_idx
        lda  #2
        sta  sfx_b_timer
        jmp  @done

@b_thump2:
        ; FX_THUMP_2 — Beat2, sweep 129→77 Hz (symétrique Beat1)
        lda  thump2_per_lo
        ldy  #2
        jsr  _psg_write
        lda  thump2_per_hi
        ldy  #3
        jsr  _psg_write
        lda  #$10
        ldy  #9
        jsr  _psg_write
        lda  #$0A
        ldy  #11
        jsr  _psg_write
        lda  #$00
        ldy  #12
        jsr  _psg_write
        lda  #$00
        ldy  #13
        jsr  _psg_write
        lda  #1
        sta  sweep_b_idx
        lda  #2
        sta  sfx_b_timer
        jmp  @done

; ─── Canal C : UFO ────────────────────────────────────────────
@route_c:
        lda  sound_tmp
        sta  sfx_c_id
        lda  #1
        sta  ufo_snd_act    ; activer auto-restart

        lda  sound_tmp
        cmp  #FX_UFO
        bne  @c_ufo_small

        ; FX_UFO large — oscillation 1000/800 Hz (sweep 4 entrées)
        lda  ufo_l_per_lo
        ldy  #4
        jsr  _psg_write
        lda  ufo_l_per_hi
        ldy  #5
        jsr  _psg_write
        lda  #$0A           ; vol C modéré (fixe, pas d'enveloppe)
        ldy  #10
        jsr  _psg_write
        lda  #1
        sta  sweep_c_idx
        lda  #4
        sta  sfx_c_timer
        jmp  @done

@c_ufo_small:
        ; FX_UFO_SMALL — sweep montant 700→1300 Hz
        lda  ufo_s_per_lo
        ldy  #4
        jsr  _psg_write
        lda  ufo_s_per_hi
        ldy  #5
        jsr  _psg_write
        lda  #$0A
        ldy  #10
        jsr  _psg_write
        lda  #1
        sta  sweep_c_idx
        lda  #4
        sta  sfx_c_timer
        jmp  @done

; ─── Canal A : effets primaires ───────────────────────────────
@route_a:
        lda  sound_tmp
        sta  _sfx_id

        ; FX_FIRE — noise 740 Hz (R6=3) + enveloppe decay ~280 ms
        ; Correction Phase 22 : R6=$03 (vs $01 trop proche d'EXPLODE).
        ; Pas de sweep (sweep_a_idx=255).
        cmp  #FX_FIRE
        bne  @a_not_fire
        lda  #$03           ; noise 740 Hz (arcade ≈740 Hz filtré)
        ldy  #6
        jsr  _psg_write
        lda  #$10           ; vol A = enveloppe
        ldy  #8
        jsr  _psg_write
        lda  #$45           ; env period lo (~280 ms)
        ldy  #11
        jsr  _psg_write
        lda  #$04           ; env period hi
        ldy  #12
        jsr  _psg_write
        lda  #$00           ; shape \___
        ldy  #13
        jsr  _psg_write
        lda  #$77           ; mixer_a_mask : noise A only
        sta  mixer_a_mask
        lda  #255           ; sweep_a_idx=255 → pas de modulation R6
        sta  sweep_a_idx
        lda  #6
        sta  _sfx_timer
        jmp  @done
@a_not_fire:

        ; FX_EXPLODE / FX_BANG_MEDIUM / FX_BANG_SMALL
        ; Impact $02 (~1000 Hz) puis corps grave selon taille.
        ; Canal A uniquement (R8 enveloppe). Mixer noise A = $77.
        cmp  #FX_EXPLODE
        beq  @a_bang_setup
        cmp  #FX_BANG_MEDIUM
        beq  @a_bang_setup
        cmp  #FX_BANG_SMALL
        bne  @a_not_bang
@a_bang_setup:
        ; A = sfx_id, déjà dans _sfx_id
        lda  #$02           ; R6 impact aigu
        ldy  #6
        jsr  _psg_write
        lda  #$10           ; vol A = enveloppe (was R10 = canal C — corrigé Phase 22)
        ldy  #8
        jsr  _psg_write
        lda  #$00           ; env period lo
        ldy  #11
        jsr  _psg_write
        lda  #$38           ; env period hi (~880 ms decay)
        ldy  #12
        jsr  _psg_write
        lda  #$00           ; shape \___
        ldy  #13
        jsr  _psg_write
        lda  #$77           ; mixer_a_mask : noise A only (vs $47 tous canaux — corrigé Phase 22)
        sta  mixer_a_mask
        lda  #1             ; sweep_a_idx=1 → tick suivant = corps grave
        sta  sweep_a_idx
        lda  #25
        sta  _sfx_timer
        jmp  @done
@a_not_bang:

        cmp  #FX_THUMP      ; ne devrait pas arriver (routé vers B), mais sécurité
        bne  @a_ck_thump2
        jmp  @done
@a_ck_thump2:
        cmp  #FX_THUMP_2
        bne  @a_not_thump2_guard
        jmp  @done
@a_not_thump2_guard:

        cmp  #FX_HYPER
        bne  @a_not_hyper
        ; FX_HYPER — tone A + noise A, enveloppe decay ~558 ms
        lda  #$10           ; noise medium
        ldy  #6
        jsr  _psg_write
        lda  #$80           ; freq tone A lo (grave)
        ldy  #0
        jsr  _psg_write
        lda  #$01           ; freq tone A hi
        ldy  #1
        jsr  _psg_write
        lda  #$10           ; vol A = enveloppe
        ldy  #8
        jsr  _psg_write
        lda  #$44           ; env period lo (~558 ms)
        ldy  #11
        jsr  _psg_write
        lda  #$00
        ldy  #12
        jsr  _psg_write
        lda  #$00           ; shape \___
        ldy  #13
        jsr  _psg_write
        lda  #$76           ; mixer_a_mask : tone A + noise A
        sta  mixer_a_mask
        lda  #255
        sta  sweep_a_idx
        lda  #14
        sta  _sfx_timer
        jmp  @done
@a_not_hyper:

        cmp  #FX_THRUST
        bne  @a_not_thrust
        ; FX_THRUST — tone grave 82 Hz, rumble arcade
        lda  #$FA           ; period 762 = $02FA → 82 Hz
        ldy  #0
        jsr  _psg_write
        lda  #$02
        ldy  #1
        jsr  _psg_write
        lda  #$0E           ; vol A direct (presque max)
        ldy  #8
        jsr  _psg_write
        lda  #$7E           ; mixer_a_mask : tone A only
        sta  mixer_a_mask
        lda  #255
        sta  sweep_a_idx
        lda  #7
        sta  _sfx_timer
        jmp  @done
@a_not_thrust:

        cmp  #FX_LIFE
        bne  @a_not_life
        ; FX_LIFE — tone plateau ~2956 Hz + enveloppe decay
        lda  #$15           ; period 21 = $0015 → 2956 Hz
        ldy  #0
        jsr  _psg_write
        lda  #$00
        ldy  #1
        jsr  _psg_write
        lda  #$10           ; vol A = enveloppe
        ldy  #8
        jsr  _psg_write
        lda  #$80           ; env period lo (~33 ms/phase, decay rapide)
        ldy  #11
        jsr  _psg_write
        lda  #$00
        ldy  #12
        jsr  _psg_write
        lda  #$00           ; shape \___
        ldy  #13
        jsr  _psg_write
        lda  #$7E           ; mixer_a_mask : tone A only
        sta  mixer_a_mask
        lda  #4             ; sweep_a_idx=4 → pas de sweep (max 4 entrées)
        sta  sweep_a_idx
        lda  #4
        sta  _sfx_timer
        jmp  @done
@a_not_life:

        ; FX inconnu ou FX_UFO/UFO_SMALL mal routé : ignorer
        lda  #FX_NONE
        sta  _sfx_id

@done:
        jsr  update_mixer   ; recalculer R7 avec les 3 canaux
        pla
        sta  VIA_DDRA
        cli
        rts

;-----------------------------------------------------------------
; _sound_stop_ufo — arrêter le son UFO (canal C) immédiatement
;
; Appelé par ufo_kill() et à la sortie d'écran de l'UFO.
;-----------------------------------------------------------------
_sound_stop_ufo:
        sei
        lda  #0
        sta  ufo_snd_act    ; désactiver auto-restart
        sta  sfx_c_timer    ; marquer canal C comme inactif
        sta  sfx_c_id

        lda  VIA_DDRA
        pha
        lda  #$FF
        sta  VIA_DDRA

        lda  #0             ; vol C = 0
        ldy  #10
        jsr  _psg_write
        jsr  update_mixer   ; retirer canal C du mixer R7

        pla
        sta  VIA_DDRA
        cli
        rts

;-----------------------------------------------------------------
; _sound_tick — appelé par _irq_handler à 50 Hz
;
; Phase 21b : skip 1 IRQ/2 → cadence effective 25 Hz.
; Gère les 3 canaux indépendamment. Sauvegarde DDRA une seule fois.
;-----------------------------------------------------------------
_sound_tick:
        ; Skip IRQs impaires → 25 Hz effectif
        lda  _frame_cnt
        and  #1
        beq  @tick_even
        jmp  @skip          ; impaire → skip (branch trop longue)
@tick_even:

        ; Quelque chose à faire ?
        lda  _sfx_timer
        ora  sfx_b_timer
        ora  sfx_c_timer
        bne  @tick_start
        jmp  @skip          ; rien d'actif → skip
@tick_start:

        sei
        lda  VIA_DDRA
        pha
        lda  #$FF
        sta  VIA_DDRA

        ; ═══════════════════════════════════════════════════════
        ; Canal A — effets primaires
        ; ═══════════════════════════════════════════════════════
        lda  _sfx_timer
        beq  @a_done

        ; Sweep canal A
        ldx  sweep_a_idx
        lda  _sfx_id

        cmp  #FX_EXPLODE
        bne  @ck_a_bang_m
        jmp  @sw_a_bang_l
@ck_a_bang_m:
        cmp  #FX_BANG_MEDIUM
        bne  @ck_a_bang_s
        jmp  @sw_a_bang_m
@ck_a_bang_s:
        cmp  #FX_BANG_SMALL
        bne  @ck_a_life
        jmp  @sw_a_bang_s
@ck_a_life:
        cmp  #FX_LIFE
        bne  @sw_a_none
        jmp  @sw_a_life
@sw_a_none:
        jmp  @sw_a_done     ; FX_FIRE/HYPER/THRUST : pas de sweep (idx=255)

@sw_a_bang_l:
        cpx  #2
        bcs  @sw_a_done
        lda  bang_l_noise,x
        ldy  #6
        jsr  _psg_write
        inc  sweep_a_idx
        jmp  @sw_a_done

@sw_a_bang_m:
        cpx  #2
        bcs  @sw_a_done
        lda  bang_m_noise,x
        ldy  #6
        jsr  _psg_write
        inc  sweep_a_idx
        jmp  @sw_a_done

@sw_a_bang_s:
        cpx  #2
        bcs  @sw_a_done
        lda  bang_s_noise,x
        ldy  #6
        jsr  _psg_write
        inc  sweep_a_idx
        jmp  @sw_a_done

@sw_a_life:
        cpx  #4
        bcs  @sw_a_done
        lda  life_per_lo,x
        ldy  #0
        jsr  _psg_write
        lda  life_per_hi,x
        ldy  #1
        jsr  _psg_write
        inc  sweep_a_idx

@sw_a_done:
        ; Décrémenter timer A
        dec  _sfx_timer
        bne  @a_done
        ; Canal A expiré → couper volume A
        lda  #0
        ldy  #8
        jsr  _psg_write
        lda  #FX_NONE
        sta  _sfx_id
@a_done:

        ; ═══════════════════════════════════════════════════════
        ; Canal B — thump (Beat1/Beat2)
        ; ═══════════════════════════════════════════════════════
        lda  sfx_b_timer
        beq  @b_done

        ; Sweep canal B (2 entrées selon b_id)
        ldx  sweep_b_idx
        cpx  #2
        bcs  @sw_b_done
        lda  sfx_b_id
        cmp  #FX_THUMP
        bne  @sw_b_thump2
        lda  thump1_per_lo,x
        ldy  #2
        jsr  _psg_write
        lda  thump1_per_hi,x
        ldy  #3
        jsr  _psg_write
        inc  sweep_b_idx
        jmp  @sw_b_done
@sw_b_thump2:
        lda  thump2_per_lo,x
        ldy  #2
        jsr  _psg_write
        lda  thump2_per_hi,x
        ldy  #3
        jsr  _psg_write
        inc  sweep_b_idx

@sw_b_done:
        dec  sfx_b_timer
        bne  @b_done
        ; Canal B expiré → couper volume B
        lda  #0
        ldy  #9
        jsr  _psg_write
        lda  #FX_NONE
        sta  sfx_b_id
@b_done:

        ; ═══════════════════════════════════════════════════════
        ; Canal C — UFO (auto-restart si ufo_snd_act=1)
        ; ═══════════════════════════════════════════════════════
        lda  sfx_c_timer
        beq  @c_done

        ; Sweep canal C (4 entrées)
        ldx  sweep_c_idx
        cpx  #4
        bcs  @sw_c_done
        lda  sfx_c_id
        cmp  #FX_UFO
        bne  @sw_c_ufo_s
        lda  ufo_l_per_lo,x
        ldy  #4
        jsr  _psg_write
        lda  ufo_l_per_hi,x
        ldy  #5
        jsr  _psg_write
        inc  sweep_c_idx
        jmp  @sw_c_done
@sw_c_ufo_s:
        lda  ufo_s_per_lo,x
        ldy  #4
        jsr  _psg_write
        lda  ufo_s_per_hi,x
        ldy  #5
        jsr  _psg_write
        inc  sweep_c_idx

@sw_c_done:
        dec  sfx_c_timer
        bne  @c_done
        ; Canal C expiré
        lda  ufo_snd_act
        beq  @c_stop
        ; Auto-restart : rejouer depuis entrée 0
        lda  sfx_c_id
        cmp  #FX_UFO
        bne  @c_restart_small
        lda  ufo_l_per_lo     ; FX_UFO : large
        ldy  #4
        jsr  _psg_write
        lda  ufo_l_per_hi
        ldy  #5
        jsr  _psg_write
        jmp  @c_restart_common
@c_restart_small:
        lda  ufo_s_per_lo     ; FX_UFO_SMALL : small
        ldy  #4
        jsr  _psg_write
        lda  ufo_s_per_hi
        ldy  #5
        jsr  _psg_write
@c_restart_common:
        lda  #1
        sta  sweep_c_idx
        lda  #4
        sta  sfx_c_timer
        jmp  @c_done
@c_stop:
        lda  #0
        ldy  #10
        jsr  _psg_write
        lda  #FX_NONE
        sta  sfx_c_id

@c_done:
        ; Recalculer R7 après tous les changements d'état
        jsr  update_mixer

        pla
        sta  VIA_DDRA
        cli

@skip:
        rts

;-----------------------------------------------------------------
; _tune_play_note — A = index note (0..6) : G2 GS2 A2 AS2 B2 C3 CS3
; Joue sur canal A (overrides sfx canal A).
;-----------------------------------------------------------------
note_lo:  .byte $7E, $5A, $38, $18, $FA, $DE, $C3
note_hi:  .byte $02, $02, $02, $02, $01, $01, $01

_tune_play_note:
        sta  sound_tmp
        sei
        lda  VIA_PCR
        and  #$11
        sta  kb_pcr_save
        lda  VIA_DDRA
        pha
        lda  #$FF
        sta  VIA_DDRA

        ldx  sound_tmp
        lda  note_lo,x
        ldy  #0
        jsr  _psg_write
        ldx  sound_tmp
        lda  note_hi,x
        ldy  #1
        jsr  _psg_write
        lda  #$0A
        ldy  #8
        jsr  _psg_write
        lda  #$7E           ; tone A on, port A input
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

        lda  #$00
        ldy  #8
        jsr  _psg_write
        lda  #$7F
        ldy  #7
        jsr  _psg_write

        pla
        sta  VIA_DDRA
        cli
        rts

;-----------------------------------------------------------------
; _irq_handler — handler IRQ T1 (50 Hz)
;-----------------------------------------------------------------
_irq_handler:
        pha
        txa
        pha
        tya
        pha

        lda  VIA_IFR
        and  #$40
        beq  @not_t1
        lda  VIA_T1CL       ; clear T1 flag
        jsr  _sound_tick
        inc  _frame_cnt
@not_t1:
        pla
        tay
        pla
        tax
        pla
        rti

;-----------------------------------------------------------------
; _irq_install — installe handler + active T1 IRQ
;-----------------------------------------------------------------
_irq_install:
        sei
        lda  #$7F
        sta  VIA_IER        ; disable toutes sources
        lda  #$7F
        sta  VIA_IFR        ; clear tous flags

        lda  #$4C           ; JMP abs
        sta  $0228          ; vecteur Oric-1
        lda  #<_irq_handler
        sta  $0229
        lda  #>_irq_handler
        sta  $022A

        lda  #$4C
        sta  $0244          ; vecteur Atmos
        lda  #<_irq_handler
        sta  $0245
        lda  #>_irq_handler
        sta  $0246

        lda  #$C0           ; enable T1 IRQ (bit 7=set, bit 6=T1)
        sta  VIA_IER

        lda  VIA_T1CL       ; clear T1 flag initial
        lda  #0
        sta  _frame_cnt

        cli
        rts
