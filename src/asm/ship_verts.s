; ===============================================================
; ship_verts.s — auto-généré par tools/gen_ship.py
; NE PAS ÉDITER MANUELLEMENT — régénérer avec : make gen_ship
; ===============================================================
; 32 angles, résolution 11.250° par pas
; Sommets initiaux : [(0, -6), (-4, 4), (4, 4)]
; Amplitude thrust : ±6 px/frame

        .segment "RODATA"

        .export ship_pt0x, ship_pt0y
        .export ship_pt1x, ship_pt1y
        .export ship_pt2x, ship_pt2y
        .export _ship_thrx, _ship_thry

ship_pt0x:
        .byte $00, $01, $02, $03, $04, $05, $06, $06
        .byte $06, $06, $06, $05, $04, $03, $02, $01
        .byte $00, $FF, $FE, $FD, $FC, $FB, $FA, $FA
        .byte $FA, $FA, $FA, $FB, $FC, $FD, $FE, $FF

ship_pt0y:
        .byte $FA, $FA, $FA, $FB, $FC, $FD, $FE, $FF
        .byte $00, $01, $02, $03, $04, $05, $06, $06
        .byte $06, $06, $06, $05, $04, $03, $02, $01
        .byte $00, $FF, $FE, $FD, $FC, $FB, $FA, $FA

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

