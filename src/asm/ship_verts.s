; ===============================================================
; ship_verts.s — auto-généré par tools/gen_ship.py
; NE PAS ÉDITER MANUELLEMENT — régénérer avec : make gen_ship
; ===============================================================
; 32 angles, résolution 11.250° par pas
; Sommets initiaux : [(0, -12), (-8, 8), (8, 8)]
; Amplitude thrust : ±6 px/frame

        .segment "RODATA"

        .export ship_pt0x, ship_pt0y
        .export ship_pt1x, ship_pt1y
        .export ship_pt2x, ship_pt2y
        .export _ship_thrx, _ship_thry

ship_pt0x:
        .byte $00, $02, $05, $07, $08, $0A, $0B, $0C
        .byte $0C, $0C, $0B, $0A, $08, $07, $05, $02
        .byte $00, $FE, $FB, $F9, $F8, $F6, $F5, $F4
        .byte $F4, $F4, $F5, $F6, $F8, $F9, $FB, $FE

ship_pt0y:
        .byte $F4, $F4, $F5, $F6, $F8, $F9, $FB, $FE
        .byte $00, $02, $05, $07, $08, $0A, $0B, $0C
        .byte $0C, $0C, $0B, $0A, $08, $07, $05, $02
        .byte $00, $FE, $FB, $F9, $F8, $F6, $F5, $F4

ship_pt1x:
        .byte $F8, $F7, $F6, $F5, $F5, $F5, $F6, $F7
        .byte $F8, $FA, $FC, $FE, $00, $02, $04, $06
        .byte $08, $09, $0A, $0B, $0B, $0B, $0A, $09
        .byte $08, $06, $04, $02, $00, $FE, $FC, $FA

ship_pt1y:
        .byte $08, $06, $04, $02, $00, $FE, $FC, $FA
        .byte $F8, $F7, $F6, $F5, $F5, $F5, $F6, $F7
        .byte $F8, $FA, $FC, $FE, $00, $02, $04, $06
        .byte $08, $09, $0A, $0B, $0B, $0B, $0A, $09

ship_pt2x:
        .byte $08, $06, $04, $02, $00, $FE, $FC, $FA
        .byte $F8, $F7, $F6, $F5, $F5, $F5, $F6, $F7
        .byte $F8, $FA, $FC, $FE, $00, $02, $04, $06
        .byte $08, $09, $0A, $0B, $0B, $0B, $0A, $09

ship_pt2y:
        .byte $08, $09, $0A, $0B, $0B, $0B, $0A, $09
        .byte $08, $06, $04, $02, $00, $FE, $FC, $FA
        .byte $F8, $F7, $F6, $F5, $F5, $F5, $F6, $F7
        .byte $F8, $FA, $FC, $FE, $00, $02, $04, $06

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

