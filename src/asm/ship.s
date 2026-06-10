;=================================================================
; ship.s — État du vaisseau Asteroids (ZP) + init/rotation
;
; Phase 27 (G2/G3) : le RENDU du ship est désormais en C (game.c,
; ship_render) — clip par segment + duplication d'instance aux bords
; + flamme de thrust. compute_verts / draw_five_lines / _ship_draw /
; _ship_erase supprimés ; les 10 octets ZP sh_tx0..sh_ty4 libérés.
;
; Exports C  : _ship_init, _ship_rotate
; ZP exportés : _ship_x, _ship_y, _ship_vx, _ship_vy
;               _ship_angle, _key_state
;
; Convention :
;   _ship_x, _ship_y : position pixel (centre du vaisseau, 0..239/0..199)
;   _ship_vx, _ship_vy : vitesse 8.8 signée
;   _ship_angle : 0..31 (32 angles, +11.25° par pas, 0 = pointe haut)
;=================================================================

        .export   _ship_init, _ship_rotate
        .exportzp _ship_x, _ship_y, _ship_x_frac, _ship_y_frac
        .exportzp _ship_vx, _ship_vy
        .exportzp _ship_angle, _key_state

;-----------------------------------------------------------------
; Variables ZP
;-----------------------------------------------------------------
        .zeropage

_ship_x:      .res 1     ; position X entière (0..239)
_ship_y:      .res 1     ; position Y entière (0..199)
_ship_x_frac: .res 1     ; sous-pixel X (8.8 fixed-point partie basse)
_ship_y_frac: .res 1     ; sous-pixel Y idem
_ship_vx:     .res 2     ; vitesse X signed int 16 bits (8.8 fixed-point)
_ship_vy:     .res 2     ; vitesse Y idem
_ship_angle:  .res 1     ; orientation 0..31
_key_state:   .res 1     ; bit0=←  bit1=→  bit2=↑(thrust)  bit3=SPACE  bit4=↓

;-----------------------------------------------------------------
; _ship_init — vaisseau au centre, immobile, angle 0
;-----------------------------------------------------------------
        .segment "CODE"

_ship_init:
        lda  #120
        sta  _ship_x
        lda  #100
        sta  _ship_y
        lda  #0
        sta  _ship_x_frac
        sta  _ship_y_frac
        sta  _ship_vx
        sta  _ship_vx+1
        sta  _ship_vy
        sta  _ship_vy+1
        sta  _ship_angle
        sta  _key_state
        rts

;-----------------------------------------------------------------
; _ship_rotate — A = delta (signé : -1 = gauche, +1 = droite)
;
; cc65 passe l'argument signed char dans A.
; angle = (angle + delta) mod 32.
;-----------------------------------------------------------------
_ship_rotate:
        clc
        adc  _ship_angle
        and  #$1F
        sta  _ship_angle
        rts
