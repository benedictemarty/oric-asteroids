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
        .exportzp _ship_x, _ship_y, _ship_x_frac, _ship_y_frac
        .exportzp _ship_vx, _ship_vy
        .exportzp _ship_angle, _key_state

        .importzp _lx0, _ly0, _lx1, _ly1
        .import   _draw_line_xor_open      ; Phase 24 — segments semi-ouverts
        .import   ship_pt0x, ship_pt0y
        .import   ship_pt1x, ship_pt1y
        .import   ship_pt2x, ship_pt2y
        .import   ship_pt3x, ship_pt3y
        .import   ship_pt4x, ship_pt4y

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

; Coordonnées calculées des 5 sommets après rotation+translation
sh_tx0:      .res 1
sh_ty0:      .res 1
sh_tx1:      .res 1
sh_ty1:      .res 1
sh_tx2:      .res 1
sh_ty2:      .res 1
sh_tx3:      .res 1
sh_ty3:      .res 1
sh_tx4:      .res 1
sh_ty4:      .res 1

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
; compute_verts — calcule les 5 sommets dans sh_tx0..sh_ty4
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

        ; Vertex 1 (arrière gauche extérieur)
        lda  ship_pt1x,x
        clc
        adc  _ship_x
        sta  sh_tx1
        lda  ship_pt1y,x
        clc
        adc  _ship_y
        sta  sh_ty1

        ; Vertex 2 (arrière droit extérieur)
        lda  ship_pt2x,x
        clc
        adc  _ship_x
        sta  sh_tx2
        lda  ship_pt2y,x
        clc
        adc  _ship_y
        sta  sh_ty2

        ; Vertex 3 (cockpit gauche)
        lda  ship_pt3x,x
        clc
        adc  _ship_x
        sta  sh_tx3
        lda  ship_pt3y,x
        clc
        adc  _ship_y
        sta  sh_ty3

        ; Vertex 4 (cockpit droit)
        lda  ship_pt4x,x
        clc
        adc  _ship_x
        sta  sh_tx4
        lda  ship_pt4y,x
        clc
        adc  _ship_y
        sta  sh_ty4
        rts

;-----------------------------------------------------------------
; draw_five_lines — trace les 5 segments arcade-fidèles
;
; Phase 24 — segments SEMI-OUVERTS (_draw_line_xor_open : exclut le
; pixel de départ original). Les directions sont choisies pour que
; chaque sommet soit l'ARRIVÉE d'exactement un segment (in-degree 1) :
;
;   P3→P0   peint P0 (apex)
;   P3→P1   peint P1 (arrière gauche)
;   P0→P4   peint P4 (cockpit droit)
;   P4→P2   peint P2 (arrière droit)
;   P4→P3   peint P3 (cockpit gauche, barre inversée)
;
; Chaque sommet est donc XOR-é exactement UNE fois (impair = visible),
; y compris P3/P4 partagés par 3 segments — ce qui clôt le compromis
; Phase 19 (sommets éteints) sans replot compensatoire ni trace
; fantôme (erase/draw passent par le même chemin, XOR symétrique).
;-----------------------------------------------------------------
draw_five_lines:
        ; Segment P3 -> P0 (haut flanc gauche, peint P0)
        lda  sh_tx3
        sta  _lx0
        lda  sh_ty3
        sta  _ly0
        lda  sh_tx0
        sta  _lx1
        lda  sh_ty0
        sta  _ly1
        jsr  _draw_line_xor_open

        ; Segment P3 -> P1 (bas flanc gauche, peint P1)
        lda  sh_tx3
        sta  _lx0
        lda  sh_ty3
        sta  _ly0
        lda  sh_tx1
        sta  _lx1
        lda  sh_ty1
        sta  _ly1
        jsr  _draw_line_xor_open

        ; Segment P0 -> P4 (haut flanc droit, peint P4)
        lda  sh_tx0
        sta  _lx0
        lda  sh_ty0
        sta  _ly0
        lda  sh_tx4
        sta  _lx1
        lda  sh_ty4
        sta  _ly1
        jsr  _draw_line_xor_open

        ; Segment P4 -> P2 (bas flanc droit, peint P2)
        lda  sh_tx4
        sta  _lx0
        lda  sh_ty4
        sta  _ly0
        lda  sh_tx2
        sta  _lx1
        lda  sh_ty2
        sta  _ly1
        jsr  _draw_line_xor_open

        ; Segment P4 -> P3 (barre cockpit, peint P3)
        lda  sh_tx4
        sta  _lx0
        lda  sh_ty4
        sta  _ly0
        lda  sh_tx3
        sta  _lx1
        lda  sh_ty3
        sta  _ly1
        jsr  _draw_line_xor_open
        rts

;-----------------------------------------------------------------
; _ship_draw / _ship_erase — XOR idempotent : même routine
;-----------------------------------------------------------------
_ship_draw:
_ship_erase:
        jsr  compute_verts
        jmp  draw_five_lines

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
