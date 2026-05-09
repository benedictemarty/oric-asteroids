; ===============================================================
; shapes.s — auto-généré par tools/gen_shapes.py
; PORTAGE ATARI ARCADE rev 4 (Phase 10a)
; Source : 6502disassembly.com/va-asteroids/Asteroids.html
;          AstPtrnPtrTbl → 4 shapes ($51E6, $51FE, $521A, $5234)
; ===============================================================
; 4 shapes Atari × 3 tailles × 8 sommets (décimés depuis 11-13).
; Index : (size * 4 + shape) * 8 + vertex
; Échelles : [0.9, 1.6, 2.6]  Rayons collision : [4, 8, 13]

        .segment "RODATA"

        .export _shape_x, _shape_y, _shape_radii

_shape_x:  ; 96 octets, index = (size*4+shape)*8 + vertex
        .byte $01, $02, $01, $FF, $FC, $FC, $FC, $FD ; size=0 shape=0
        .byte $02, $04, $01, $FF, $FF, $FF, $03, $04 ; size=0 shape=1
        .byte $FF, $FD, $FF, $02, $04, $02, $FF, $FC ; size=0 shape=2
        .byte $01, $04, $01, $FE, $FC, $FC, $FE, $03 ; size=0 shape=3
        .byte $02, $03, $02, $FE, $FA, $F8, $FA, $FB ; size=1 shape=0
        .byte $03, $06, $02, $FE, $FE, $FE, $05, $08 ; size=1 shape=1
        .byte $FE, $FB, $FE, $03, $06, $03, $FE, $FA ; size=1 shape=2
        .byte $02, $06, $02, $FD, $FA, $FA, $FD, $05 ; size=1 shape=3
        .byte $03, $05, $03, $FD, $F6, $F3, $F6, $F8 ; size=2 shape=0
        .byte $05, $0A, $03, $FD, $FD, $FD, $08, $0D ; size=2 shape=1
        .byte $FD, $F8, $FD, $05, $0A, $05, $FD, $F6 ; size=2 shape=2
        .byte $03, $0A, $03, $FB, $F6, $F6, $FB, $08 ; size=2 shape=3

_shape_y:  ; 96 octets, index = (size*4+shape)*8 + vertex
        .byte $02, $01, $FF, $FC, $FC, $FE, $FF, $FE ; size=0 shape=0
        .byte $01, $02, $02, $03, $00, $FD, $FD, $00 ; size=0 shape=1
        .byte $00, $FF, $FC, $FC, $00, $03, $03, $00 ; size=0 shape=2
        .byte $00, $02, $04, $04, $02, $FF, $FC, $FD ; size=0 shape=3
        .byte $03, $02, $FE, $F8, $F8, $FD, $FE, $FD ; size=1 shape=0
        .byte $02, $03, $03, $05, $00, $FB, $FB, $00 ; size=1 shape=1
        .byte $00, $FE, $FA, $FA, $00, $05, $05, $00 ; size=1 shape=2
        .byte $00, $03, $06, $06, $03, $FE, $FA, $FB ; size=1 shape=3
        .byte $05, $03, $FD, $F3, $F3, $FB, $FD, $FB ; size=2 shape=0
        .byte $03, $05, $05, $08, $00, $F8, $F8, $00 ; size=2 shape=1
        .byte $00, $FD, $F6, $F6, $00, $08, $08, $00 ; size=2 shape=2
        .byte $00, $05, $0A, $0A, $05, $FD, $F6, $F8 ; size=2 shape=3

_shape_radii:  ; rayon collision par taille (Phase 5)
        .byte 4, 8, 13
