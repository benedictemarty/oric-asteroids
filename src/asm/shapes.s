; ===============================================================
; shapes.s — auto-généré par tools/gen_shapes.py
; NE PAS ÉDITER MANUELLEMENT — régénérer avec : make gen_shapes
; ===============================================================
; 4 formes × 3 tailles × 8 sommets, polygones fermés.
; Index : (size * 4 + shape) * 8 + vertex
; Tailles : [5, 9, 14]  Rayons collision : [4, 8, 12]

        .segment "RODATA"

        .export _shape_x, _shape_y, _shape_radii

_shape_x:  ; 96 octets, index = (size*4+shape)*8 + vertex
        .byte $05, $03, $00, $FC, $FC, $FC, $00, $03 ; size=0 shape=0
        .byte $06, $04, $00, $FC, $FA, $FC, $00, $04 ; size=0 shape=1
        .byte $04, $04, $00, $FC, $FC, $FC, $00, $04 ; size=0 shape=2
        .byte $05, $03, $00, $FD, $FB, $FD, $00, $04 ; size=0 shape=3
        .byte $09, $05, $00, $FA, $F8, $FA, $00, $05 ; size=1 shape=0
        .byte $0A, $06, $00, $FA, $F6, $FA, $00, $06 ; size=1 shape=1
        .byte $08, $08, $00, $F9, $F8, $F8, $00, $07 ; size=1 shape=2
        .byte $09, $06, $00, $FB, $F7, $FA, $00, $07 ; size=1 shape=3
        .byte $0E, $08, $00, $F6, $F4, $F6, $00, $08 ; size=2 shape=0
        .byte $0F, $0A, $00, $F6, $F1, $F6, $00, $0A ; size=2 shape=1
        .byte $0D, $0C, $00, $F5, $F3, $F4, $00, $0B ; size=2 shape=2
        .byte $0E, $09, $00, $F8, $F1, $F7, $00, $0B ; size=2 shape=3

_shape_y:  ; 96 octets, index = (size*4+shape)*8 + vertex
        .byte $00, $03, $05, $04, $00, $FC, $FB, $FD ; size=0 shape=0
        .byte $00, $04, $04, $04, $00, $FC, $FC, $FC ; size=0 shape=1
        .byte $00, $04, $04, $04, $00, $FC, $FC, $FC ; size=0 shape=2
        .byte $00, $03, $05, $03, $00, $FD, $FB, $FC ; size=0 shape=3
        .byte $00, $05, $09, $06, $00, $FA, $F7, $FB ; size=1 shape=0
        .byte $00, $06, $07, $06, $00, $FA, $F9, $FA ; size=1 shape=1
        .byte $00, $08, $08, $07, $00, $F8, $F8, $F9 ; size=1 shape=2
        .byte $00, $06, $09, $05, $00, $FA, $F7, $F9 ; size=1 shape=3
        .byte $00, $08, $0E, $0A, $00, $F6, $F2, $F8 ; size=2 shape=0
        .byte $00, $0A, $0B, $0A, $00, $F6, $F5, $F6 ; size=2 shape=1
        .byte $00, $0C, $0D, $0B, $00, $F4, $F3, $F5 ; size=2 shape=2
        .byte $00, $09, $0F, $08, $00, $F7, $F2, $F5 ; size=2 shape=3

_shape_radii:  ; rayon collision par taille (Phase 5)
        .byte 4, 8, 12
