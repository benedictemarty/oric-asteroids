;=================================================================
; ship.s — État + rendu du vaisseau Asteroids sur Oric-1 HIRES
; Phase 3
;
; Exports C  : _ship_init, _ship_draw, _ship_erase, _ship_rotate
; ZP exportés : _ship_x, _ship_y, _ship_vx, _ship_vy
;               _ship_angle, _key_state
;
; Convention :
;   _ship_x, _ship_y : position pixel (centre du vaisseau)
;   _ship_vx, _ship_vy : vitesse (signée, pixels/frame)
;   _ship_angle : 0..31 (32 angles, +11.25° par pas, 0 = pointe haut)
;
; XOR : _ship_draw == _ship_erase (idempotent : appel pair = état initial).
;=================================================================

        .export   _ship_init, _ship_draw, _ship_erase, _ship_rotate
        .exportzp _ship_x, _ship_y, _ship_vx, _ship_vy
        .exportzp _ship_angle, _key_state

        .importzp _lx0, _ly0, _lx1, _ly1
        .import   _draw_line_xor
        .import   _plot_dot                ; Phase 16 — replot rapide 1 pixel
        .import   ship_pt0x, ship_pt0y
        .import   ship_pt1x, ship_pt1y
        .import   ship_pt2x, ship_pt2y

;-----------------------------------------------------------------
; Variables ZP
;-----------------------------------------------------------------
        .zeropage

_ship_x:     .res 1     ; position X (0..239)
_ship_y:     .res 1     ; position Y (0..199)
_ship_vx:    .res 1     ; vitesse X signée
_ship_vy:    .res 1     ; vitesse Y signée
_ship_angle: .res 1     ; orientation 0..31
_key_state:  .res 1     ; bit0=←  bit1=→  bit2=↑(thrust)  bit3=SPACE(tir)

; Coordonnées calculées des 3 sommets après rotation+translation
sh_tx0:      .res 1
sh_ty0:      .res 1
sh_tx1:      .res 1
sh_ty1:      .res 1
sh_tx2:      .res 1
sh_ty2:      .res 1

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
        sta  _ship_vx
        sta  _ship_vy
        sta  _ship_angle
        sta  _key_state
        rts

;-----------------------------------------------------------------
; compute_verts — calcule les 3 sommets dans sh_tx0..sh_ty2
;
; ptN[angle] est en complément à 2 (8 bits signés, plage [-12,+12]).
; clc+adc avec _ship_x (unsigned, 0..239) donne la coord pixel ; le
; débordement n'est pas géré ici (ship_update doit garder le centre
; loin des bords ; sinon : pixels parasites en RAM hors HIRES).
;-----------------------------------------------------------------
compute_verts:
        ldx  _ship_angle

        ; Vertex 0 (pointe avant)
        lda  ship_pt0x,x
        clc
        adc  _ship_x
        sta  sh_tx0
        lda  ship_pt0y,x
        clc
        adc  _ship_y
        sta  sh_ty0

        ; Vertex 1 (arrière gauche)
        lda  ship_pt1x,x
        clc
        adc  _ship_x
        sta  sh_tx1
        lda  ship_pt1y,x
        clc
        adc  _ship_y
        sta  sh_ty1

        ; Vertex 2 (arrière droite)
        lda  ship_pt2x,x
        clc
        adc  _ship_x
        sta  sh_tx2
        lda  ship_pt2y,x
        clc
        adc  _ship_y
        sta  sh_ty2
        rts

;-----------------------------------------------------------------
; draw_three_lines — trace les 3 segments du triangle
;-----------------------------------------------------------------
draw_three_lines:
        ; Segment P0 -> P1
        lda  sh_tx0
        sta  _lx0
        lda  sh_ty0
        sta  _ly0
        lda  sh_tx1
        sta  _lx1
        lda  sh_ty1
        sta  _ly1
        jsr  _draw_line_xor

        ; Segment P1 -> P2
        lda  sh_tx1
        sta  _lx0
        lda  sh_ty1
        sta  _ly0
        lda  sh_tx2
        sta  _lx1
        lda  sh_ty2
        sta  _ly1
        jsr  _draw_line_xor

        ; Segment P0 -> P2
        lda  sh_tx0
        sta  _lx0
        lda  sh_ty0
        sta  _ly0
        lda  sh_tx2
        sta  _lx1
        lda  sh_ty2
        sta  _ly1
        jsr  _draw_line_xor

        ; Replot des 3 sommets via _plot_dot rapide (Phase 16) :
        ; setup réduit (pas de Bresenham), gain ~50 c/plot × 3 = 150 c.
        lda  sh_tx0
        sta  _lx0
        lda  sh_ty0
        sta  _ly0
        jsr  _plot_dot         ; plot P0
        lda  sh_tx1
        sta  _lx0
        lda  sh_ty1
        sta  _ly0
        jsr  _plot_dot         ; plot P1
        lda  sh_tx2
        sta  _lx0
        lda  sh_ty2
        sta  _ly0
        jmp  _plot_dot         ; plot P2 (tail call)

;-----------------------------------------------------------------
; _ship_draw / _ship_erase — XOR idempotent : même routine
;-----------------------------------------------------------------
_ship_draw:
_ship_erase:
        jsr  compute_verts
        jmp  draw_three_lines

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
