; ===============================================================
; shapes.s — auto-généré par tools/gen_shapes.py (Phase 10b)
; PORTAGE ATARI ARCADE rev 4 — N sommets variables (11-13)
; Source : 6502disassembly.com/va-asteroids/Asteroids.html
; ===============================================================
; 4 shapes Atari × 3 tailles, longueurs : [6, 7, 6, 7]
; Total sommets : 78 (par taille : 26)
; Échelles : [0.9, 1.6, 2.6]  Rayons collision : [4, 8, 13]

        .segment "RODATA"

        .export _shape_x, _shape_y, _shape_radii
        .export _shape_off, _shape_len

_shape_off:
        ; offset (uint8) dans _shape_x/y, index = size*4+shape
        .byte $00, $06, $0D, $13, $1A, $20, $27, $2D, $34, $3A, $41, $47

_shape_len:
        ; nombre de sommets, index = size*4+shape
        .byte $06, $07, $06, $07, $06, $07, $06, $07, $06, $07, $06, $07

_shape_x:
        ; X des sommets, indexé via _shape_off + vertex
        .byte $00, $02, $02, $FC, $FC, $FD, $02, $03, $FF, $FF, $FF, $03
        .byte $04, $FF, $FF, $01, $04, $02, $FC, $01, $04, $FE, $FC, $FE
        .byte $02, $00, $00, $03, $03, $FA, $F8, $FB, $03, $05, $FE, $FE
        .byte $FE, $05, $06, $FE, $FE, $02, $06, $03, $FA, $02, $06, $FD
        .byte $FA, $FD, $03, $00, $00, $05, $05, $F6, $F3, $F8, $05, $08
        .byte $FD, $FD, $FD, $08, $0A, $FD, $FD, $03, $0A, $05, $F6, $03
        .byte $0A, $FB, $F6, $FB, $05, $00

_shape_y:
        ; Y des sommets, indexé via _shape_off + vertex
        .byte $01, $01, $FD, $FC, $FE, $FE, $01, $03, $03, $00, $FD, $FD
        .byte $01, $00, $FC, $FC, $FF, $03, $00, $00, $02, $04, $02, $FC
        .byte $FC, $FF, $02, $02, $FB, $F8, $FD, $FD, $02, $05, $05, $00
        .byte $FB, $FB, $02, $00, $FA, $FA, $FE, $05, $00, $00, $03, $06
        .byte $03, $FA, $FA, $FE, $03, $03, $F8, $F3, $FB, $FB, $03, $08
        .byte $08, $00, $F8, $F8, $03, $00, $F6, $F6, $FD, $08, $00, $00
        .byte $05, $0A, $05, $F6, $F6, $FD

_shape_radii:  ; rayon collision par taille (Phase 5)
        .byte 4, 8, 13
