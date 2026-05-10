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
        ; XOR le pixel + bit6 toujours 1
        lda  x_msk,y
        ldy  #0
        eor  (l_ptr),y
        ora  #$40
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

        ;--- err = dx - dy (16-bit signe) ---
        lda  l_dx
        sec
        sbc  l_dy
        sta  l_err_lo
        bcs  @err_pos
        lda  #$FF
        bne  @err_set
@err_pos:
        lda  #0
@err_set:
        sta  l_err_hi
        ldy  #0                 ; Y=0 fixe pour (l_ptr),y

;-----------------------------------------------------------------
; Boucle principale
;-----------------------------------------------------------------
@loop:
        ;--- XOR du pixel (Phase 16 : 14 cycles, ORA #$40 retiré) ---
        ; L'invariant bit6=1 est préservé : x_msk[i] a TOUJOURS bit6=0,
        ; donc l'XOR ne touche pas bit6. Init HIRES = $40 → bit6 reste 1.
        ; Gain ~2 cycles par pixel × ~150 pixels/frame = ~300 c/frame.
        lda  l_mask
        eor  (l_ptr),y
        sta  (l_ptr),y

        ;--- Test de fin (12 cycles) ---
        lda  _lx0
        cmp  _lx1
        bne  @not_done
        lda  _ly0
        cmp  _ly1
        bne  @not_done
        jmp  @done
@not_done:

        ;--- e2 = 2 * err (16 cycles) ---
        lda  l_err_lo
        asl
        sta  l_e2lo
        lda  l_err_hi
        rol
        sta  l_e2hi

        ;--- Test : e2 > -dy (step x) ---
        lda  l_e2lo
        clc
        adc  l_dy
        tax
        lda  l_e2hi
        adc  #0
        bmi  @no_stepx
        bne  @stepx
        txa
        beq  @no_stepx
@stepx:
        ; err -= dy
        lda  l_err_lo
        sec
        sbc  l_dy
        sta  l_err_lo
        bcs  @sx_ok
        dec  l_err_hi
@sx_ok:
        ; Avancer x (sx=+1 toujours) : LSR masque, incr adresse si nouvelle colonne
        lsr  l_mask             ; 5 cycles (ZP)
        bcs  @sx_newcol         ; 3 (pris 1/6)
        inc  _lx0               ; 5
        jmp  @no_stepx          ; 3
@sx_newcol:
        lda  #$20               ; 2 nouveau masque = bit5
        sta  l_mask             ; 3
        inc  l_ptr              ; 5
        bne  @sx_xpp
        inc  l_ptr+1            ; 5 (rare : page boundary)
@sx_xpp:
        inc  _lx0               ; 5
@no_stepx:

        ;--- Test : e2 < dx (step y) ---
        lda  l_e2hi
        bmi  @stepy
        bne  @no_stepy
        lda  l_e2lo
        cmp  l_dx
        bcs  @no_stepy
@stepy:
        ; err += dx
        lda  l_err_lo
        clc
        adc  l_dx
        sta  l_err_lo
        bcc  @sy_ok
        inc  l_err_hi
@sy_ok:
        ; Avancer y : l_ptr +/- 40 selon sy
        lda  l_sy
        bmi  @sy_neg
        lda  l_ptr
        clc
        adc  #40
        sta  l_ptr
        bcc  @sy_done
        inc  l_ptr+1
        jmp  @sy_done
@sy_neg:
        lda  l_ptr
        sec
        sbc  #40
        sta  l_ptr
        bcs  @sy_done
        dec  l_ptr+1
@sy_done:
        lda  _ly0
        clc
        adc  l_sy
        sta  _ly0
@no_stepy:
        jmp  @loop

@done:
        rts
