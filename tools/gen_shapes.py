#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_shapes.py — Génère src/asm/shapes.s : 4 formes d'astéroïdes × 3 tailles.

Chaque astéroïde est un polygone fermé de 8 sommets stockés sous forme
de coordonnées signées 8-bit (∆ par rapport au centre).

Tables flat indexées par : (size * 4 + shape) * 8 + vertex
  _shape_x[96] : composantes X (12 polygones × 8 sommets)
  _shape_y[96] : composantes Y
  _shape_radii[3] : rayon de collision par taille (Phase 5)

Tailles (rayon de base) : petit=5, moyen=9, grand=14.
Total RODATA : 96 + 96 + 3 = 195 octets.
"""

import math

# 4 formes via perturbations radiales (8 sommets : 0°, 45°, ..., 315°)
PERTURB = [
    [1.00, 0.85, 1.00, 1.00, 0.85, 1.00, 1.00, 0.85],   # forme 0 — rocheuse
    [1.10, 1.00, 0.80, 1.00, 1.10, 1.00, 0.80, 1.00],   # forme 1 — diamant
    [0.90, 1.20, 0.90, 1.10, 0.90, 1.20, 0.90, 1.10],   # forme 2 — étoile 4
    [1.00, 0.95, 1.05, 0.85, 1.05, 0.95, 1.00, 1.10],   # forme 3 — irrégulière
]

SIZES = [5, 9, 14]
N_VERTS = 8
N_SHAPES = 4
N_SIZES = 3

# Rayons de collision (Phase 5) : ~0.85 × rayon visuel
RADII = [round(s * 0.85) for s in SIZES]


def to_byte(v):
    v = max(-127, min(127, v))
    return v & 0xFF


def main():
    print("; ===============================================================")
    print("; shapes.s — auto-généré par tools/gen_shapes.py")
    print("; NE PAS ÉDITER MANUELLEMENT — régénérer avec : make gen_shapes")
    print("; ===============================================================")
    print("; 4 formes × 3 tailles × 8 sommets, polygones fermés.")
    print("; Index : (size * 4 + shape) * 8 + vertex")
    print(f"; Tailles : {SIZES}  Rayons collision : {RADII}")
    print()
    print('        .segment "RODATA"')
    print()
    print("        .export _shape_x, _shape_y, _shape_radii")
    print()

    xs = []
    ys = []
    for size_id in range(N_SIZES):
        for shape_id in range(N_SHAPES):
            r = SIZES[size_id]
            for i in range(N_VERTS):
                theta = 2.0 * math.pi * i / N_VERTS
                rr = r * PERTURB[shape_id][i]
                xs.append(to_byte(round(rr * math.cos(theta))))
                ys.append(to_byte(round(rr * math.sin(theta))))

    def emit(name, vals):
        print(f"{name}:  ; {len(vals)} octets, index = (size*4+shape)*8 + vertex")
        for i in range(0, len(vals), 8):
            comment = f" ; size={i//32} shape={(i//8)%4}"
            print("        .byte " + ", ".join(f"${v:02X}" for v in vals[i:i+8]) + comment)
        print()

    emit("_shape_x", xs)
    emit("_shape_y", ys)

    print("_shape_radii:  ; rayon collision par taille (Phase 5)")
    print("        .byte " + ", ".join(str(r) for r in RADII))


if __name__ == "__main__":
    main()
