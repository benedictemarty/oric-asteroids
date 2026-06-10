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

        .export  _hires_init, _draw_line_xor, _draw_line_xor_open, _plot_dot
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
l_sy:       .res 1          ; pas y : 1 ou $FF
l_err_lo:   .res 1          ; erreur Bresenham 8-bit (Phase 18 main-axis split)
l_ptr:      .res 2          ; pointeur ZP vers l'octet HIRES courant
l_mask:     .res 1          ; masque de bit du pixel courant
l_pix:      .res 1          ; Phase 17 — compteur pixels = max(dx,dy)+1
l_open:     .res 1          ; Phase 24 — 1 = segment semi-ouvert (exclut le
                            ; pixel de départ ORIGINAL, avant swap)
l_swap:     .res 1          ; Phase 24 — 1 = extrémités échangées (sx norm.)
cs_src:     .res 2          ; ptr source pour la copie charset $B400 -> $9800
; Phase 24 : l_sx / l_err_hi / l_e2lo / l_e2hi supprimés (reliquats
; pré-Phase 18, jamais référencés) → place ZP pour l_open / l_swap.

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
        ; ----------------------------------------------------------------
        ; 1. Copie du charset $B400 → $9800 (8 pages = 2048 octets)
        ;
        ; OBLIGATOIRE avant STA $BB80,$1C : l'ULA Oric utilise deux zones
        ; charset distinctes selon le mode vidéo :
        ;   - Mode TEXT  : charset à $B400 (mappé spécial / RAM système)
        ;   - Mode HIRES : charset à $9800 (RAM utilisateur)
        ; Le BASIC ROM copie $B400 → $9800 quand on utilise sa commande
        ; HIRES ; comme on bypasse le BASIC en écrivant directement
        ; l'attribut $1C, on doit faire la copie nous-mêmes — sinon
        ; les 3 lignes texte du bas affichent des glyphs aléatoires
        ; (RAM $9800-$9FFF non initialisée au boot).
        ;
        ; Note (Phosphoric src/video/video.c:61-65) :
        ;   base = vid->hires_mode ? 0x9800 : 0xB400;
        ;   return mem[base + char_idx * 8 + row];
        ; ----------------------------------------------------------------
        lda  #$00
        sta  cs_src
        sta  l_ptr               ; dst lo
        lda  #$B4
        sta  cs_src+1            ; src hi = $B4 → $B400
        lda  #$98
        sta  l_ptr+1             ; dst hi = $98 → $9800
        ldx  #8                  ; 8 pages à copier
@cs_pg:
        ldy  #0
@cs_by:
        lda  (cs_src),y
        sta  (l_ptr),y
        iny
        bne  @cs_by
        inc  cs_src+1
        inc  l_ptr+1
        dex
        bne  @cs_pg

        ; ----------------------------------------------------------------
        ; 2. Bascule en mode HIRES : écrire attribut vid_mode=4 ($1C)
        ;    à $BB80 (l'ULA lit ce premier octet à chaque scanline 0).
        ; ----------------------------------------------------------------
        lda  #$1C
        sta  $BB80

        ; ----------------------------------------------------------------
        ; 3. Remplir $A000-$BF3F avec $40 (octet "pixel vierge" HIRES) :
        ;    bit 6=1 → sécurité non-attribut, bits 5-0=0 → aucun pixel.
        ;    IMPORTANT : recharger A=$40 à chaque tour — 'lda l_ptr+1'
        ;    écrase A.
        ; ----------------------------------------------------------------
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

        ; ----------------------------------------------------------------
        ; 4. Nettoyer la zone TEXT du bas $BF68-$BFDF (lignes texte 25-27
        ;    du screen TEXT) avec $20 (espace). Sans cette étape, on
        ;    hérite des codes laissés par le BASIC (CALL, prompt…) et la
        ;    zone TEXT du bas affiche du texte parasite.
        ; ----------------------------------------------------------------
        ldx  #119                ; 120 octets (3 × 40)
        lda  #$20
@txt_clr:
        sta  $BF68,x
        dex
        bpl  @txt_clr
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

;-----------------------------------------------------------------
; _draw_line_xor_open — Phase 24 : variante SEMI-OUVERTE.
;
; Trace ]P0, P1] : tous les pixels du segment SAUF le pixel de départ
; ORIGINAL (lx0, ly0) tel que passé par l'appelant (indépendant du
; swap de normalisation). Un segment dégénéré (P0 == P1) ne trace rien.
;
; Usage : polygones fermés / graphes orientés en in-degree 1. Chaque
; sommet étant le départ d'exactement un segment et l'arrivée d'un
; autre, il est XOR-é exactement UNE fois → plus de sommets éteints
; (2 toggles) ni besoin de replots compensatoires (3 toggles).
; Remplace le compromis Phase 19 pour ship et asteroids.
;-----------------------------------------------------------------
_draw_line_xor_open:
        lda  #1
        sta  l_open
        jmp  dlx_common

_draw_line_xor:
        lda  #0
        sta  l_open
dlx_common:
        lda  #0
        sta  l_swap
        ;--- Normalisation sx=+1 : si lx0 > lx1, echanger les extremites ---
        lda  _lx0
        cmp  _lx1
        bcc  @no_swap
        beq  @no_swap
        inc  l_swap          ; Phase 24 — mémoriser le swap (ne touche pas A)
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

        ; ── Phase 2b — SMC : patcher la direction step y dans la boucle h
        ; (et plus tard la boucle v). Élimine le test `lda l_sy; bmi ...`
        ; à chaque step y, économise ~5-6 c/px sur les pixels concernés.
        ;
        ; Opcodes par défaut (sy = +1) : clc/adc/bcc/inc
        ; Opcodes pour sy = -1 :          sec/sbc/bcs/dec
        lda  l_sy
        bpl  @smc_sy_pos
        ; sy = -1 : patcher
        lda  #$38                ; sec
        sta  @h_sy_carry
        sta  @v_sy_carry
        lda  #$E9                ; sbc
        sta  @h_sy_op
        sta  @v_sy_op
        lda  #$B0                ; bcs
        sta  @h_sy_branch
        sta  @v_sy_branch
        lda  #$C6                ; dec
        sta  @h_sy_inc
        sta  @v_sy_inc
        jmp  @smc_done
@smc_sy_pos:
        ; sy = +1 : restaurer opcodes par défaut (au cas où un appel
        ; précédent a patché à sec/sbc/bcs/dec).
        lda  #$18                ; clc
        sta  @h_sy_carry
        sta  @v_sy_carry
        lda  #$69                ; adc
        sta  @h_sy_op
        sta  @v_sy_op
        lda  #$90                ; bcc
        sta  @h_sy_branch
        sta  @v_sy_branch
        lda  #$E6                ; inc
        sta  @h_sy_inc
        sta  @v_sy_inc
@smc_done:

        ; ── Phase 2b SMC : patcher les opérandes immédiates dx / dy ──
        ; `adc l_dy` / `cmp l_dx` / `sbc l_dx` (boucle h) et symétrique
        ; pour la boucle v. Patch l'octet opérande (+1 après l'opcode).
        lda  l_dy
        sta  @h_adc_dy+1
        sta  @v_cmp_dy+1
        sta  @v_sbc_dy+1
        lda  l_dx
        sta  @h_cmp_dx+1
        sta  @h_sbc_dx+1
        sta  @v_adc_dx+1

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
        ; dy > dx → axe vertical (dy >= 1 garanti : pas de dégénéré ici)
        ldx  l_open
        beq  @v_incl
        ; Phase 24 — semi-ouvert : N = dy pixels (au lieu de dy+1).
        ;   swap   → le départ original est l'extrémité FINALE post-swap :
        ;            boucle normale, le compteur réduit l'exclut.
        ;   direct → le départ original est le 1er pixel post-swap :
        ;            entrer à @v_body (1er step sans XOR ni dec).
        lda  l_dy
        sta  l_pix
        ldx  l_swap
        bne  @v_go
        jmp  @v_body
@v_incl:
        lda  l_dy
        clc
        adc  #1
        sta  l_pix
@v_go:
        jmp  @v_loop

@h_init:
        ldx  l_open
        beq  @h_incl
        ; Phase 24 — semi-ouvert : N = dx pixels ; dégénéré (dx=dy=0,
        ; segment-point) → ne rien tracer (les appelants qui veulent un
        ; point utilisent plot_dot ou draw_line_xor inclusif).
        lda  l_dx
        jeq  @done
        sta  l_pix
        ldx  l_swap
        bne  @h_go
        jmp  @h_body
@h_incl:
        lda  l_dx
        clc
        adc  #1
        sta  l_pix
@h_go:
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
        ; err += dy ; if err >= dx : step y, err -= dx
        ; Phase 2b — SMC : `cmp l_dx`, `adc l_dy`, `sbc l_dx` (3c chacun)
        ; remplacés par `cmp #dx`, `adc #dy`, `sbc #dx` (2c chacun) avec
        ; opérandes patchées au démarrage. Gain ~3c/px.
        lda  l_err_lo
        clc
@h_adc_dy:
        adc  #$00                ; opérande patchée = l_dy
        ; Note : pas de clc avant cmp car cmp ne dépend pas du carry.
@h_cmp_dx:
        cmp  #$00                ; opérande patchée = l_dx
        bcc  @h_no_stepy
@h_sbc_dx:
        sbc  #$00                ; opérande patchée = l_dx
        sta  l_err_lo
        ; ── Phase 2b — step y via SMC ────────────────────────────────
        ; L'instruction `clc` (op $18) ou `sec` ($38), `adc #40` ($69)
        ; ou `sbc #40` ($E9), `bcc` ($90) ou `bcs` ($B0), `inc` ($E6)
        ; ou `dec` ($C6) sont patchées au démarrage selon le signe de sy.
        ; Élimine le test `lda l_sy; bmi ...` à chaque pixel (~5c).
        lda  l_ptr
@h_sy_carry:
        clc                      ; patché $18→$38 si sy<0
@h_sy_op:
        adc  #40                 ; patché $69→$E9 si sy<0
        sta  l_ptr
@h_sy_branch:
        bcc  @h_step_x           ; patché $90→$B0 si sy<0
@h_sy_inc:
        inc  l_ptr+1             ; patché $E6→$C6 si sy<0
        jmp  @h_step_x
@h_no_stepy:
        sta  l_err_lo
@h_step_x:
        ; step x toujours (sx=+1) : lsr mask. Si pas de nouvelle colonne
        ; (~80 % des pixels), `bcc @h_loop` économise 1c vs ancien
        ; `bcs @h_newcol / jmp @h_loop` (revue senior optim #2).
        lsr  l_mask
        bcc  @h_loop
        ; Fallthrough = newcol : reset mask + inc ptr.
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
        ; Phase 2b — SMC : 3 opérandes ZP → immédiat, gain ~3c/px.
        lda  l_err_lo
        clc
@v_adc_dx:
        adc  #$00                ; opérande patchée = l_dx
@v_cmp_dy:
        cmp  #$00                ; opérande patchée = l_dy
        bcc  @v_no_stepx
@v_sbc_dy:
        sbc  #$00                ; opérande patchée = l_dy
        sta  l_err_lo
        ; step x : lsr mask. Si pas newcol → branch direct vers step_y
        ; (économie 1c vs ancien bcs+jmp).
        lsr  l_mask
        bcc  @v_step_y
        ; Fallthrough = newcol.
        lda  #$20
        sta  l_mask
        inc  l_ptr
        bne  @v_step_y
        inc  l_ptr+1
        jmp  @v_step_y
@v_no_stepx:
        sta  l_err_lo
@v_step_y:
        ; Phase 2b — step y via SMC : 4 opcodes patchés selon sy.
        lda  l_ptr
@v_sy_carry:
        clc                      ; patché $18→$38 si sy<0
@v_sy_op:
        adc  #40                 ; patché $69→$E9 si sy<0
        sta  l_ptr
@v_sy_branch:
        bcc  @v_loop             ; patché $90→$B0 si sy<0
@v_sy_inc:
        inc  l_ptr+1             ; patché $E6→$C6 si sy<0
        jmp  @v_loop

@done:
        rts
