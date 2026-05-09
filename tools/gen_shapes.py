#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_shapes.py — Génère src/asm/shapes.s : 4 shapes asteroids ATARI ARCADE rev 4.

Source des shapes : 6502disassembly.com/va-asteroids/Asteroids.html
- AstPtrnPtrTbl à $51DE pointe vers 4 shapes : $51E6, $51FE, $521A, $5234.
- Format DVG SVEC (Short VECTOR) : déplacements relatifs cumulés.
- Décodage manuel des SVEC depuis les commentaires du désassemblage.

Notre format Oric impose 8 sommets fixes par shape (cohérent avec
asteroid_draw_one). Les shapes Atari ont 11-13 sommets ; on **décime**
en gardant 8 sommets représentatifs équirépartis.

Échelle Oric :
- grand : ×2.5 → max ~14 px (rayon visuel)
- moyen : ×1.6 → max ~9 px
- petit : ×0.9 → max ~5 px

Phase 10a — portage authentique des shapes arcade.
"""

import math

# Sommets cumulés (en unités DVG arcade) — décodés depuis les SVEC
# Origine = (0, 0) au centre de l'asteroid.

# Shape 0 (rev 4, $51E6 — 11 sommets) — décimé à 8
ATARI_SHAPE_0 = [
    (1, 2), (2, 1), (1, -1), (-1, -5),
    (-4, -5), (-5, -2), (-4, -1), (-3, -2),
]

# Shape 1 (rev 4, $51FE — 13 sommets) — décimé à 8
ATARI_SHAPE_1 = [
    (2, 1), (4, 2), (1, 2), (-1, 3),
    (-1, 0), (-1, -3), (3, -3), (5, 0),
]

# Shape 2 (rev 4, $521A — 12 sommets) — décimé à 8
ATARI_SHAPE_2 = [
    (-1, 0), (-3, -1), (-1, -4), (2, -4),
    (4, 0), (2, 3), (-1, 3), (-4, 0),
]

# Shape 3 (rev 4, $5234 — 13 sommets) — décimé à 8
ATARI_SHAPE_3 = [
    (1, 0), (4, 2), (1, 4), (-2, 4),
    (-4, 2), (-4, -1), (-2, -4), (3, -3),
]

ATARI_SHAPES = [ATARI_SHAPE_0, ATARI_SHAPE_1, ATARI_SHAPE_2, ATARI_SHAPE_3]

# Échelles pour les 3 tailles Oric (pour atteindre rayons visuels 14/9/5)
# Les magnitudes Atari max ≈ 5 unités, donc ×2.5 → ~12.5 px
SCALES = [0.9, 1.6, 2.6]   # petit, moyen, grand
SIZES_TARGET = [5, 9, 14]   # rayons visuels cibles (info)
N_VERTS = 8
N_SHAPES = 4
N_SIZES = 3

# Rayons de collision (Phase 5) : max distance origine-sommet à chaque taille
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


def to_byte(v):
    v = max(-127, min(127, v))
    return v & 0xFF


def main():
    radii = compute_radii()
    print("; ===============================================================")
    print("; shapes.s — auto-généré par tools/gen_shapes.py")
    print("; PORTAGE ATARI ARCADE rev 4 (Phase 10a)")
    print("; Source : 6502disassembly.com/va-asteroids/Asteroids.html")
    print(";          AstPtrnPtrTbl → 4 shapes ($51E6, $51FE, $521A, $5234)")
    print("; ===============================================================")
    print("; 4 shapes Atari × 3 tailles × 8 sommets (décimés depuis 11-13).")
    print("; Index : (size * 4 + shape) * 8 + vertex")
    print(f"; Échelles : {SCALES}  Rayons collision : {radii}")
    print()
    print('        .segment "RODATA"')
    print()
    print("        .export _shape_x, _shape_y, _shape_radii")
    print()

    xs, ys = [], []
    for size_id in range(N_SIZES):
        scale = SCALES[size_id]
        for shape in ATARI_SHAPES:
            for (x, y) in shape:
                xs.append(to_byte(int(round(x * scale))))
                ys.append(to_byte(int(round(y * scale))))

    def emit(name, vals):
        print(f"{name}:  ; {len(vals)} octets, index = (size*4+shape)*8 + vertex")
        for i in range(0, len(vals), 8):
            comment = f" ; size={i//32} shape={(i//8)%4}"
            print("        .byte " + ", ".join(f"${v:02X}" for v in vals[i:i+8]) + comment)
        print()

    emit("_shape_x", xs)
    emit("_shape_y", ys)

    print("_shape_radii:  ; rayon collision par taille (Phase 5)")
    print("        .byte " + ", ".join(str(r) for r in radii))


if __name__ == "__main__":
    main()
