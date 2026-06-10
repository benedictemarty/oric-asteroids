; ===============================================================
; ship_verts.s — auto-généré par tools/gen_ship.py
; NE PAS ÉDITER MANUELLEMENT — régénérer avec : make gen_ship
; ===============================================================
; 32 angles, résolution 11.250° par pas
; Sommets initiaux : [(0, -5), (-4, 4), (4, 4), (-2, 0), (2, 0)]
; Amplitude thrust : ±6 px/frame

        .segment "RODATA"

        .export ship_pt0x, ship_pt0y
        .export ship_pt1x, ship_pt1y
        .export ship_pt2x, ship_pt2y
        .export ship_pt3x, ship_pt3y
        .export ship_pt4x, ship_pt4y
        .export _ship_thrx, _ship_thry

_ship_pt0x = ship_pt0x
        .export _ship_pt0x
_ship_pt0y = ship_pt0y
        .export _ship_pt0y
_ship_pt1x = ship_pt1x
        .export _ship_pt1x
_ship_pt1y = ship_pt1y
        .export _ship_pt1y
_ship_pt2x = ship_pt2x
        .export _ship_pt2x
_ship_pt2y = ship_pt2y
        .export _ship_pt2y
_ship_pt3x = ship_pt3x
        .export _ship_pt3x
_ship_pt3y = ship_pt3y
        .export _ship_pt3y
_ship_pt4x = ship_pt4x
        .export _ship_pt4x
_ship_pt4y = ship_pt4y
        .export _ship_pt4y

ship_pt0x:
        .byte $00, $01, $02, $03, $04, $04, $05, $05
        .byte $05, $05, $05, $04, $04, $03, $02, $01
        .byte $00, $FF, $FE, $FD, $FC, $FC, $FB, $FB
        .byte $FB, $FB, $FB, $FC, $FC, $FD, $FE, $FF

ship_pt0y:
        .byte $FB, $FB, $FB, $FC, $FC, $FD, $FE, $FF
        .byte $00, $01, $02, $03, $04, $04, $05, $05
        .byte $05, $05, $05, $04, $04, $03, $02, $01
        .byte $00, $FF, $FE, $FD, $FC, $FC, $FB, $FB

ship_pt1x:
        .byte $FC, $FB, $FB, $FA, $FA, $FA, $FB, $FB
        .byte $FC, $FD, $FE, $FF, $00, $01, $02, $03
        .byte $04, $05, $05, $06, $06, $06, $05, $05
        .byte $04, $03, $02, $01, $00, $FF, $FE, $FD

ship_pt1y:
        .byte $04, $03, $02, $01, $00, $FF, $FE, $FD
        .byte $FC, $FB, $FB, $FA, $FA, $FA, $FB, $FB
        .byte $FC, $FD, $FE, $FF, $00, $01, $02, $03
        .byte $04, $05, $05, $06, $06, $06, $05, $05

ship_pt2x:
        .byte $04, $03, $02, $01, $00, $FF, $FE, $FD
        .byte $FC, $FB, $FB, $FA, $FA, $FA, $FB, $FB
        .byte $FC, $FD, $FE, $FF, $00, $01, $02, $03
        .byte $04, $05, $05, $06, $06, $06, $05, $05

ship_pt2y:
        .byte $04, $05, $05, $06, $06, $06, $05, $05
        .byte $04, $03, $02, $01, $00, $FF, $FE, $FD
        .byte $FC, $FB, $FB, $FA, $FA, $FA, $FB, $FB
        .byte $FC, $FD, $FE, $FF, $00, $01, $02, $03

ship_pt3x:
        .byte $FE, $FE, $FE, $FE, $FF, $FF, $FF, $00
        .byte $00, $00, $01, $01, $01, $02, $02, $02
        .byte $02, $02, $02, $02, $01, $01, $01, $00
        .byte $00, $00, $FF, $FF, $FF, $FE, $FE, $FE

ship_pt3y:
        .byte $00, $00, $FF, $FF, $FF, $FE, $FE, $FE
        .byte $FE, $FE, $FE, $FE, $FF, $FF, $FF, $00
        .byte $00, $00, $01, $01, $01, $02, $02, $02
        .byte $02, $02, $02, $02, $01, $01, $01, $00

ship_pt4x:
        .byte $02, $02, $02, $02, $01, $01, $01, $00
        .byte $00, $00, $FF, $FF, $FF, $FE, $FE, $FE
        .byte $FE, $FE, $FE, $FE, $FF, $FF, $FF, $00
        .byte $00, $00, $01, $01, $01, $02, $02, $02

ship_pt4y:
        .byte $00, $00, $01, $01, $01, $02, $02, $02
        .byte $02, $02, $02, $02, $01, $01, $01, $00
        .byte $00, $00, $FF, $FF, $FF, $FE, $FE, $FE
        .byte $FE, $FE, $FE, $FE, $FF, $FF, $FF, $00

_ship_thrx:
        .byte $00, $01, $02, $03, $04, $05, $06, $06
        .byte $06, $06, $06, $05, $04, $03, $02, $01
        .byte $00, $FF, $FE, $FD, $FC, $FB, $FA, $FA
        .byte $FA, $FA, $FA, $FB, $FC, $FD, $FE, $FF

_ship_thry:
        .byte $FA, $FA, $FA, $FB, $FC, $FD, $FE, $FF
        .byte $00, $01, $02, $03, $04, $05, $06, $06
        .byte $06, $06, $06, $05, $04, $03, $02, $01
        .byte $00, $FF, $FE, $FD, $FC, $FB, $FA, $FA

