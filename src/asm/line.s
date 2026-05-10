;=================================================================
; line.s — Traceur Bresenham XOR + init HIRES pour Oric-1
; Phase 2 — version optimisee (normalisation sx=+1, init unique)
;
; Exports C  : _hires_init, _draw_line_xor
; Params ZP  : _lx0, _ly0, _lx1, _ly1  (ecrits avant l'appel)
;
; Encodage HIRES Oric (confirme Phosphoric video.c) :
;   (byte & 0x60)==0 -> attribut  ; sinon -> pixel
;   bit7=1 -> inverse video       ; bit6=1 -> "safety bit" non-attribut
;   Init vierge = 0x40  (bit6=1, aucun pixel, non-attribut)
;=================================================================

        .export  _hires_init, _draw_line_xor, _plot_dot
        .exportzp _lx0, _ly0, _lx1, _ly1
        .macpack longbranch

;-----------------------------------------------------------------
; Variables ZP
;-----------------------------------------------------------------
        .zeropage

_lx0:       .res 1
_ly0:       .res 1
_lx1:       .res 1
_ly1:       .res 1
l_dx:       .res 1
l_dy:       .res 1
l_sx:       .res 1          ; pas x (non utilise dans boucle, conserve pour compat.)
l_sy:       .res 1          ; pas y : 1 ou $FF
l_err_lo:   .res 1          ; erreur Bresenham 16-bit signe (octet bas)
l_err_hi:   .res 1          ; erreur (octet haut, extension de signe)
l_ptr:      .res 2          ; pointeur ZP vers l'octet HIRES courant
l_e2lo:     .res 1          ; e2 temporaire bas  (2 * err)
l_e2hi:     .res 1          ; e2 temporaire haut
l_mask:     .res 1          ; masque de bit du pixel courant
l_pix:      .res 1          ; Phase 17 — compteur pixels = max(dx,dy)+1

;-----------------------------------------------------------------
; Tables precalculees (RODATA, 200+200+240+240 = 880 octets)
;-----------------------------------------------------------------
        .segment "RODATA"

; Adresse bas de debut de chaque ligne HIRES ($A000 + y*40)
hires_lo:
        .repeat 200, I
            .byte <($A000 + I * 40)
        .endrepeat

; Adresse haut de debut de chaque ligne HIRES
hires_hi:
        .repeat 200, I
            .byte >($A000 + I * 40)
        .endrepeat

; Colonne d'un pixel : x_col[x] = x / 6  (x dans [0,239])
x_col:
        .repeat 240, I
            .byte I / 6
        .endrepeat

; Masque de bit d'un pixel : x_msk[x] = $20 >> (x mod 6)
x_msk:
        .repeat 240, I
            .byte $20 .SHR (I .MOD 6)
        .endrepeat

;-----------------------------------------------------------------
; _hires_init — declenche le mode HIRES et efface l'ecran
;-----------------------------------------------------------------
        .segment "CODE"

_hires_init:
        ; Declencher HIRES : ecrire attribut vid_mode=4 ($1C) a $BB80
        lda  #$1C
        sta  $BB80

        ; Remplir $A000-$BF3F avec 0x40 (octet pixel vierge, noir)
        ; IMPORTANT : recharger A=$40 a chaque tour — 'lda l_ptr+1' ecrase A.
        lda  #0
        sta  l_ptr
        lda  #$A0
        sta  l_ptr+1

@clr_outer:
        lda  #$40
        ldy  #0
@clr_inner:
        sta  (l_ptr),y
        iny
        bne  @clr_inner
        inc  l_ptr+1
        lda  l_ptr+1
        cmp  #$BF
        bne  @clr_outer

        ; 64 derniers octets : $BF00-$BF3F
        lda  #$40
        ldy  #0
@clr_tail:
        sta  (l_ptr),y
        iny
        cpy  #64
        bne  @clr_tail
        rts

;-----------------------------------------------------------------
; _draw_line_xor — Bresenham XOR entre (_lx0,_ly0) et (_lx1,_ly1)
;
; Optimisations vs version naive :
;   - l_ptr et l_mask calcules UNE SEULE FOIS (via tables), puis mis a
;     jour incrementalement : mask>>1 sur pas x, ptr+/-40 sur pas y.
;   - Normalisation sx=+1 : si lx0>lx1, echanger (lx0,ly0)<->(lx1,ly1).
;     La boucle interne ne teste jamais la direction de x (economise 6 c/pix).
;   - sy reste variable, teste uniquement au moment du pas y.
;
; Invariant XOR : un pixel trace deux fois revient a l'etat initial.
; Invariant non-attribut : bit6=1 maintenu par ORA #$40.
;-----------------------------------------------------------------
;-----------------------------------------------------------------
; _plot_dot — XOR 1 pixel à (_lx0, _ly0). Phase 16 — rapide (~40 c).
;
; Évite le setup Bresenham complet pour le cas dégénéré 1 pixel
; (replots de sommets après segments XOR, fragments éphémères).
; lx1/ly1 ignorés. Y détruit. Invariant bit6=1 maintenu via ORA #$40.
;-----------------------------------------------------------------
_plot_dot:
        ldy  _lx0             ; X coord pour x_col / x_msk
        ldx  _ly0             ; Y coord pour hires_lo / hires_hi
        lda  hires_lo,x
        sta  l_ptr
        lda  hires_hi,x
        sta  l_ptr+1
        ; Ajouter colonne X
        lda  l_ptr
        clc
        adc  x_col,y
        sta  l_ptr
        bcc  @nc
        inc  l_ptr+1
@nc:
        ; XOR le pixel — bit6 préservé (même raisonnement que la boucle :
        ; x_msk[i] a toujours bit6=0, donc l'XOR ne touche pas bit6).
        lda  x_msk,y
        ldy  #0
        eor  (l_ptr),y
        sta  (l_ptr),y
        rts

_draw_line_xor:
        ;--- Normalisation sx=+1 : si lx0 > lx1, echanger les extremites ---
        lda  _lx0
        cmp  _lx1
        bcc  @no_swap
        beq  @no_swap
        ldy  _lx1
        sta  _lx1
        sty  _lx0
        ldy  _ly1
        lda  _ly0
        sta  _ly1
        sty  _ly0
@no_swap:

        ;--- dx = x1 - x0 (>=0 garanti) ---
        lda  _lx1
        sec
        sbc  _lx0
        sta  l_dx

        ;--- dy, sy ---
        lda  _ly1
        sec
        sbc  _ly0
        bcs  @sy_pos
        lda  #$FF
        sta  l_sy
        lda  _ly0
        sec
        sbc  _ly1
        sta  l_dy
        jmp  @init_ptr
@sy_pos:
        lda  #1
        sta  l_sy
        lda  _ly1
        sec
        sbc  _ly0
        sta  l_dy

        ;--- Init l_ptr et l_mask une seule fois via tables ---
@init_ptr:
        ldx  _ly0
        lda  hires_hi,x
        sta  l_ptr+1
        lda  hires_lo,x
        ldy  _lx0
        clc
        adc  x_col,y
        sta  l_ptr
        bcc  @init_mask
        inc  l_ptr+1
@init_mask:
        lda  x_msk,y
        sta  l_mask

        ;--- Phase 18 : main-axis split ---
        ; err 8-bit non-signé, biais 0. Choix axe principal selon dx vs dy.
        ; Algo : err += axe_secondaire ; si err >= axe_principal → step
        ;        secondaire, err -= axe_principal.
        lda  #0
        sta  l_err_lo
        ldy  #0                 ; Y=0 fixe pour (l_ptr),y

        lda  l_dx
        cmp  l_dy
        bcs  @h_init             ; dx >= dy → axe horizontal
        ; dy > dx → axe vertical
        lda  l_dy
        clc
        adc  #1
        sta  l_pix
        jmp  @v_loop

@h_init:
        lda  l_dx
        clc
        adc  #1
        sta  l_pix
        ; tomber sur @h_loop

;==================================================================
; Boucle HORIZONTALE (dx >= dy) : step x toujours (sx=+1 garanti par
; swap), step y conditionnel.
;==================================================================
@h_loop:
        ; XOR pixel (14c)
        lda  l_mask
        eor  (l_ptr),y
        sta  (l_ptr),y
        ; Compteur fin : BNE court vers le corps, sinon JMP @done.
        dec  l_pix
        bne  @h_body
        jmp  @done
@h_body:
        ; err += dy ; if err >= dx : step y, err -= dx (11+6c)
        lda  l_err_lo
        clc
        adc  l_dy
        cmp  l_dx
        bcc  @h_no_stepy
        sbc  l_dx                ; carry set par cmp BCS
        sta  l_err_lo
        ; step y conditionnel selon sy
        lda  l_sy
        bmi  @h_sy_neg
        lda  l_ptr
        clc
        adc  #40
        sta  l_ptr
        bcc  @h_step_x
        inc  l_ptr+1
        jmp  @h_step_x
@h_sy_neg:
        lda  l_ptr
        sec
        sbc  #40
        sta  l_ptr
        bcs  @h_step_x
        dec  l_ptr+1
        jmp  @h_step_x
@h_no_stepy:
        sta  l_err_lo
@h_step_x:
        ; step x toujours (sx=+1) : lsr mask, increment ptr si nouvelle colonne
        lsr  l_mask
        bcs  @h_newcol
        jmp  @h_loop
@h_newcol:
        lda  #$20
        sta  l_mask
        inc  l_ptr
        bne  @h_loop
        inc  l_ptr+1
        jmp  @h_loop

;==================================================================
; Boucle VERTICALE (dy > dx) : step y toujours (sy quelconque),
; step x conditionnel.
;==================================================================
@v_loop:
        ; XOR pixel
        lda  l_mask
        eor  (l_ptr),y
        sta  (l_ptr),y
        ; Compteur fin : BNE court vers le corps, sinon JMP @done.
        dec  l_pix
        bne  @v_body
        jmp  @done
@v_body:
        ; err += dx ; if err >= dy : step x, err -= dy
        lda  l_err_lo
        clc
        adc  l_dx
        cmp  l_dy
        bcc  @v_no_stepx
        sbc  l_dy
        sta  l_err_lo
        ; step x : lsr mask, ptr++ si newcol
        lsr  l_mask
        bcs  @v_newcol
        jmp  @v_step_y
@v_newcol:
        lda  #$20
        sta  l_mask
        inc  l_ptr
        bne  @v_step_y
        inc  l_ptr+1
        jmp  @v_step_y
@v_no_stepx:
        sta  l_err_lo
@v_step_y:
        ; step y toujours selon sy
        lda  l_sy
        bmi  @v_sy_neg
        lda  l_ptr
        clc
        adc  #40
        sta  l_ptr
        bcc  @v_loop
        inc  l_ptr+1
        jmp  @v_loop
@v_sy_neg:
        lda  l_ptr
        sec
        sbc  #40
        sta  l_ptr
        bcs  @v_loop
        dec  l_ptr+1
        jmp  @v_loop

@done:
        rts
