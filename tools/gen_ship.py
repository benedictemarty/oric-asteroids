#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_ship.py — Générateur de tables précalculées pour le vaisseau Asteroids.

Produit src/asm/ship_verts.s contenant :
  - ship_pt0x[32], ship_pt0y[32] : pointe avant (apex) après rotation
  - ship_pt1x[32], ship_pt1y[32] : arrière gauche extérieur
  - ship_pt2x[32], ship_pt2y[32] : arrière droit extérieur
  - ship_pt3x[32], ship_pt3y[32] : cockpit gauche (sur flanc, mi-hauteur)
  - ship_pt4x[32], ship_pt4y[32] : cockpit droit (sur flanc, mi-hauteur)
  - ship_thrx[32], ship_thry[32] : vecteur de poussée (delta vélocité)

Convention : angle 0 = pointe vers le HAUT (y vers le bas en HIRES).
             angle croissant = rotation horaire visuelle (+11.25° par pas).
             32 angles = compromis taille (192 octets) / précision visuelle.
"""

import math
import sys

N = 32      # nombre d'angles
TMAG = 6    # amplitude max du thrust (px/frame)

# Sommets du vaisseau dans son repère local.
# (y positif = bas, donc pointe en y négatif = haut)
# Phase 19 : forme arcade-fidèle 5 segments. Triangle ouvert à l'arrière
# (engine port) avec une barre cockpit interne. Les segments tracés sont :
#   P0→P3, P3→P1   (flanc gauche en deux tronçons = "encoche")
#   P0→P4, P4→P2   (flanc droit en deux tronçons = "encoche")
#   P3→P4          (barre cockpit horizontale)
# Pas de segment P1↔P2 → l'arrière reste ouvert (style arcade).
VERTS = [
    (0,  -5),    # P0: pointe avant (apex)
    (-4,  4),    # P1: arrière gauche extérieur
    (4,   4),    # P2: arrière droit extérieur
    (-2,  0),    # P3: cockpit gauche (sur flanc, mi-hauteur)
    (2,   0),    # P4: cockpit droit (sur flanc, mi-hauteur)
]


def rotate(px, py, i):
    """Rotation antihoraire mathématique = horaire visuelle en y-vers-bas."""
    theta = 2.0 * math.pi * i / N
    rx = px * math.cos(theta) - py * math.sin(theta)
    ry = px * math.sin(theta) + py * math.cos(theta)
    return (int(round(rx)), int(round(ry)))


def to_byte(v):
    """Convertit en octet signé (complément à 2 sur 8 bits)."""
    v = max(-127, min(127, v))
    return v & 0xFF


def emit_table(name, vals):
    print(f"{name}:")
    for j in range(0, N, 8):
        print("        .byte " + ", ".join(f"${v:02X}" for v in vals[j:j+8]))
    print()


def main():
    print("; ===============================================================")
    print("; ship_verts.s — auto-généré par tools/gen_ship.py")
    print("; NE PAS ÉDITER MANUELLEMENT — régénérer avec : make gen_ship")
    print("; ===============================================================")
    print(f"; {N} angles, résolution {360.0/N:.3f}° par pas")
    print(f"; Sommets initiaux : {VERTS}")
    print(f"; Amplitude thrust : ±{TMAG} px/frame")
    print()
    print('        .segment "RODATA"')
    print()
    # Tables internes asm (préfixe ship_) — utilisées par ship.s
    print("        .export ship_pt0x, ship_pt0y")
    print("        .export ship_pt1x, ship_pt1y")
    print("        .export ship_pt2x, ship_pt2y")
    print("        .export ship_pt3x, ship_pt3y")
    print("        .export ship_pt4x, ship_pt4y")
    # Tables exposées au C (cc65 préfixe _) — utilisées par game.c
    print("        .export _ship_thrx, _ship_thry")
    print()

    for vi, ((px, py), name) in enumerate(zip(VERTS, ("pt0", "pt1", "pt2", "pt3", "pt4"))):
        xs = [to_byte(rotate(px, py, i)[0]) for i in range(N)]
        ys = [to_byte(rotate(px, py, i)[1]) for i in range(N)]
        emit_table(f"ship_{name}x", xs)
        emit_table(f"ship_{name}y", ys)

    # Vecteurs de thrust : direction de la pointe normalisée à TMAG.
    # Le préfixe '_' rend ces tables accessibles depuis C via cc65.
    thrx = [to_byte(round(TMAG * math.sin(2.0 * math.pi * i / N))) for i in range(N)]
    thry = [to_byte(round(-TMAG * math.cos(2.0 * math.pi * i / N))) for i in range(N)]
    emit_table("_ship_thrx", thrx)
    emit_table("_ship_thry", thry)


if __name__ == "__main__":
    main()
