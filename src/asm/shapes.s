; ===============================================================
; shapes.s — auto-généré par tools/gen_shapes.py (Phase 10b)
; PORTAGE ATARI ARCADE rev 4 — N sommets variables (11-13)
; Source : 6502disassembly.com/va-asteroids/Asteroids.html
; ===============================================================
; 4 shapes Atari × 3 tailles, longueurs : [11, 13, 12, 13]
; Total sommets : 147 (par taille : 49)
; Échelles : [0.9, 1.6, 2.6]  Rayons collision : [4, 8, 13]

        .segment "RODATA"

        .export _shape_x, _shape_y, _shape_radii
        .export _shape_off, _shape_len

_shape_off:
        ; offset (uint8) dans _shape_x/y, index = size*4+shape
        .byte $00, $0B, $18, $24, $31, $3C, $49, $55, $62, $6D, $7A, $86

_shape_len:
        ; nombre de sommets, index = size*4+shape
        .byte $0B, $0D, $0C, $0D, $0B, $0D, $0C, $0D, $0B, $0D, $0C, $0D

_shape_x:
        ; X des sommets, indexé via _shape_off + vertex
        .byte $00, $01, $02, $01, $02, $FF, $FC, $FC, $FC, $FC, $FD, $02
        .byte $04, $03, $01, $FF, $FE, $FF, $FE, $FF, $00, $03, $04, $04
        .byte $FF, $FD, $FF, $01, $01, $02, $04, $04, $02, $FF, $FC, $FE
        .byte $01, $04, $04, $01, $FE, $FF, $FC, $FC, $FE, $01, $02, $03
        .byte $00, $00, $02, $03, $02, $03, $FE, $FA, $F8, $F8, $FA, $FB
        .byte $03, $06, $05, $02, $FE, $FD, $FE, $FD, $FE, $00, $05, $08
        .byte $06, $FE, $FB, $FE, $02, $02, $03, $06, $06, $03, $FE, $FA
        .byte $FD, $02, $06, $06, $02, $FD, $FE, $FA, $FA, $FD, $02, $03
        .byte $05, $00, $00, $03, $05, $03, $05, $FD, $F6, $F3, $F3, $F6
        .byte $F8, $05, $0A, $08, $03, $FD, $FB, $FD, $FB, $FD, $00, $08
        .byte $0D, $0A, $FD, $F8, $FD, $03, $03, $05, $0A, $0A, $05, $FD
        .byte $F6, $FB, $03, $0A, $0A, $03, $FB, $FD, $F6, $F6, $FB, $03
        .byte $05, $08, $00

_shape_y:
        ; Y des sommets, indexé via _shape_off + vertex
        .byte $01, $02, $01, $FF, $FD, $FC, $FC, $FC, $FE, $FF, $FE, $01
        .byte $02, $03, $02, $03, $02, $00, $FE, $FD, $FE, $FD, $00, $01
        .byte $00, $FF, $FC, $FF, $FC, $FC, $FF, $00, $03, $03, $00, $FF
        .byte $00, $01, $02, $04, $04, $02, $02, $FF, $FC, $FD, $FC, $FD
        .byte $FF, $02, $03, $02, $FE, $FB, $F8, $F8, $FA, $FD, $FE, $FD
        .byte $02, $03, $05, $03, $05, $03, $00, $FD, $FB, $FD, $FB, $00
        .byte $02, $00, $FE, $FA, $FE, $FA, $FA, $FE, $00, $05, $05, $00
        .byte $FE, $00, $02, $03, $06, $06, $03, $03, $FE, $FA, $FB, $FA
        .byte $FB, $FE, $03, $05, $03, $FD, $F8, $F3, $F3, $F6, $FB, $FD
        .byte $FB, $03, $05, $08, $05, $08, $05, $00, $FB, $F8, $FB, $F8
        .byte $00, $03, $00, $FD, $F6, $FD, $F6, $F6, $FD, $00, $08, $08
        .byte $00, $FD, $00, $03, $05, $0A, $0A, $05, $05, $FD, $F6, $F8
        .byte $F6, $F8, $FD

_shape_radii:  ; rayon collision par taille (Phase 5)
        .byte 4, 8, 13
