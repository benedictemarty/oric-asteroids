#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_shapes.py — Génère src/asm/shapes.s : 4 shapes asteroids ATARI ARCADE rev 4.

Source des shapes : 6502disassembly.com/va-asteroids/Asteroids.html
- AstPtrnPtrTbl à $51DE pointe vers 4 shapes : $51E6, $51FE, $521A, $5234.
- Format DVG SVEC (Short VECTOR) : déplacements relatifs cumulés.

Phase 10b : **N sommets variables** par shape (11-13 sommets) — plus de
décimation à 8.

Format de sortie :
  _shape_off[12]   : offset (uint8) dans _shape_x/y, par (size*4 + shape)
  _shape_len[12]   : nombre de sommets (uint8)
  _shape_x[~144]   : sommets X cumulés
  _shape_y[~144]   : sommets Y cumulés
  _shape_radii[3]  : rayon collision par taille
"""

import math

# Sommets cumulés (en unités DVG arcade), tels que décodés depuis les SVEC
# Origine = (0, 0) au centre de l'asteroid. Polygone fermé par convention.

# Shape 0 ($51E6) — 11 sommets
ATARI_SHAPE_0 = [
    (0, 1), (1, 2), (2, 1), (1, -1), (2, -3),
    (-1, -5), (-4, -5), (-5, -4), (-5, -2),
    (-4, -1), (-3, -2),
]

# Shape 1 ($51FE) — 13 sommets
ATARI_SHAPE_1 = [
    (2, 1), (4, 2), (3, 3), (1, 2), (-1, 3),
    (-2, 2), (-1, 0), (-2, -2), (-1, -3),
    (0, -2), (3, -3), (5, 0), (4, 1),
]

# Shape 2 ($521A) — 12 sommets
ATARI_SHAPE_2 = [
    (-1, 0), (-3, -1), (-1, -4), (1, -1), (1, -4),
    (2, -4), (4, -1), (4, 0), (2, 3),
    (-1, 3), (-4, 0), (-2, -1),
]

# Shape 3 ($5234) — 13 sommets
ATARI_SHAPE_3 = [
    (1, 0), (4, 1), (4, 2), (1, 4), (-2, 4),
    (-1, 2), (-4, 2), (-4, -1), (-2, -4),
    (1, -3), (2, -4), (3, -3), (0, -1),
]

# Phase 18h : décimation step=2 pour réduire le coût de tracé d'environ
# 50% (11-13 sommets → 6-7). Visuellement on perd les "creux" fins de
# l'arcade authentique mais on gagne en framerate effectif (passage de
# ~2 Hz à ~5 Hz quand chargé, suffisant pour rotation/jeu fluides).
DECIMATE_STEP = 2

def _decimate(shape, step):
    return shape[::step]

ATARI_SHAPES = [
    _decimate(ATARI_SHAPE_0, DECIMATE_STEP),
    _decimate(ATARI_SHAPE_1, DECIMATE_STEP),
    _decimate(ATARI_SHAPE_2, DECIMATE_STEP),
    _decimate(ATARI_SHAPE_3, DECIMATE_STEP),
]

SCALES = [0.9, 1.6, 2.6]   # petit, moyen, grand → rayons cibles 5/9/14
N_SHAPES = 4
N_SIZES = 3


def to_byte(v):
    v = max(-127, min(127, v))
    return v & 0xFF


def compute_radii():
    radii = []
    for size_id in range(N_SIZES):
        max_r = 0
        s = SCALES[size_id]
        for shape in ATARI_SHAPES:
            for (x, y) in shape:
                d = max(abs(x), abs(y)) * s
                if d > max_r:
                    max_r = d
        radii.append(int(round(max_r)))
    return radii


def main():
    radii = compute_radii()

    # Calcul offsets et lengths
    offsets = []
    lengths = []
    xs = []
    ys = []
    cur_off = 0
    for size_id in range(N_SIZES):
        scale = SCALES[size_id]
        for shape in ATARI_SHAPES:
            offsets.append(cur_off)
            lengths.append(len(shape))
            for (x, y) in shape:
                xs.append(to_byte(int(round(x * scale))))
                ys.append(to_byte(int(round(y * scale))))
            cur_off += len(shape)

    total = cur_off
    if total > 255:
        raise SystemExit("Total sommets > 255 — offset 8-bit insuffisant")

    print("; ===============================================================")
    print("; shapes.s — auto-généré par tools/gen_shapes.py (Phase 10b)")
    print("; PORTAGE ATARI ARCADE rev 4 — N sommets variables (11-13)")
    print("; Source : 6502disassembly.com/va-asteroids/Asteroids.html")
    print("; ===============================================================")
    print(f"; 4 shapes Atari × 3 tailles, longueurs : {[len(s) for s in ATARI_SHAPES]}")
    print(f"; Total sommets : {total} (par taille : {sum(len(s) for s in ATARI_SHAPES)})")
    print(f"; Échelles : {SCALES}  Rayons collision : {radii}")
    print()
    print('        .segment "RODATA"')
    print()
    print("        .export _shape_x, _shape_y, _shape_radii")
    print("        .export _shape_off, _shape_len")
    print()

    def emit_bytes(name, vals, comment=None):
        print(f"{name}:")
        if comment:
            print(f"        ; {comment}")
        for i in range(0, len(vals), 12):
            print("        .byte " + ", ".join(f"${v:02X}" for v in vals[i:i+12]))
        print()

    emit_bytes("_shape_off", offsets, "offset (uint8) dans _shape_x/y, index = size*4+shape")
    emit_bytes("_shape_len", lengths, "nombre de sommets, index = size*4+shape")
    emit_bytes("_shape_x", xs, "X des sommets, indexé via _shape_off + vertex")
    emit_bytes("_shape_y", ys, "Y des sommets, indexé via _shape_off + vertex")

    print("_shape_radii:  ; rayon collision par taille (Phase 5)")
    print("        .byte " + ", ".join(str(r) for r in radii))


if __name__ == "__main__":
    main()
