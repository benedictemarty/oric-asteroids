#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
gen_shapes.py — Génère src/asm/shapes.s : 4 shapes asteroids ATARI ARCADE rev 4.

Source des shapes : 6502disassembly.com/va-asteroids/Asteroids.html
- AstPtrnPtrTbl à $51DE pointe vers 4 shapes : $51E6, $51FE, $521A, $5234.
- Format DVG SVEC (Short VECTOR) : déplacements relatifs cumulés.

Phase 26 (P3) : **sprites pré-rendus XOR**. Les asteroids ne tournent
jamais → chaque silhouette (4 shapes × 3 tailles) est rasterisée ICI
(Bresenham identique à line.s) puis émise en bitmap HIRES dans ses
6 phases de décalage intra-octet (6 px/octet). Le runtime blitte
l'octet entier en un seul EOR/STA (cf. _spr_blit dans line.s), au
lieu de retracer 11-13 segments par frame.

Conséquence : le coût de rendu ne dépend plus du nombre de sommets →
la décimation Phase 18h (11-13 → 6-7 sommets) est SUPPRIMÉE, les
shapes arcade authentiques sont de retour.

Tables émises :
  _spr_w[12]     : largeur en octets (stride), index = size*4+shape
  _spr_h[12]     : hauteur en rangées
  _spr_oy[12]    : offset y signé du coin haut du sprite vs centre
  _spr_cold[72]  : delta colonne signé, index = id*6 + (cx mod 6)
  _spr_lo/hi[72] : pointeurs vers le bitmap de la phase (cx mod 6)
  _x_phase[240]  : table cx → cx mod 6
  _shape_radii[3]: rayon collision par taille (inchangé : max|v| = 5
                   présent dans les shapes décimées ET complètes)

Géométrie : px_gauche = cx + ox (ox = min x rasterisé). En écrivant
cx = 6c + p : px = 6(c + (p+ox) div 6) + ((p+ox) mod 6). Le bitmap de
phase p est pré-décalé de q = (p+ox) mod 6 bits, et le runtime ajoute
cold = (p+ox) div 6 (∈ [-3..0]) à la colonne c. L'instance fantôme du
wraparound (±240 px) garde la MÊME phase (240 = 40×6) : ±40 colonnes.
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

# Phase 26 : plus de décimation — le blit rend le coût indépendant du
# nombre de sommets. Shapes arcade rev 4 authentiques (11-13 sommets).
ATARI_SHAPES = [ATARI_SHAPE_0, ATARI_SHAPE_1, ATARI_SHAPE_2, ATARI_SHAPE_3]

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


def raster_line(pixels, x0, y0, x1, y1):
    """Bresenham main-axis split, sémantique identique à line.s :
    err = 0 ; par pixel : err += axe_mineur ; si err >= axe_majeur →
    step mineur, err -= majeur ; step majeur. Pixels en OR (un sommet
    partagé reste allumé — pas de double-XOR dans un bitmap figé)."""
    if x0 > x1:
        x0, y0, x1, y1 = x1, y1, x0, y0
    dx = x1 - x0
    dy = abs(y1 - y0)
    sy = 1 if y1 >= y0 else -1
    x, y = x0, y0
    err = 0
    if dx >= dy:
        for _ in range(dx + 1):
            pixels.add((x, y))
            err += dy
            if err >= dx and dx > 0:
                err -= dx
                y += sy
            x += 1
    else:
        for _ in range(dy + 1):
            pixels.add((x, y))
            err += dx
            if err >= dy:
                err -= dy
                x += 1
            y += sy


def rasterize_shape(shape, scale):
    """Polygone fermé → set de pixels (coords centrées sur (0,0))."""
    verts = [(int(round(x * scale)), int(round(y * scale))) for (x, y) in shape]
    pixels = set()
    n = len(verts)
    for i in range(n):
        x0, y0 = verts[i]
        x1, y1 = verts[(i + 1) % n]
        raster_line(pixels, x0, y0, x1, y1)
    return pixels


def build_sprite(pixels):
    """Pixels centrés → (ox, oy, W, H, grille bool [H][W])."""
    xs = [p[0] for p in pixels]
    ys = [p[1] for p in pixels]
    ox, oy = min(xs), min(ys)
    W = max(xs) - ox + 1
    H = max(ys) - oy + 1
    grid = [[False] * W for _ in range(H)]
    for (x, y) in pixels:
        grid[y - oy][x - ox] = True
    return ox, oy, W, H, grid


def shift_bitmap(grid, W, H, q, wb):
    """Bitmap HIRES décalé de q bits (0..5), wb octets/rangée.
    Bit 5 = pixel de gauche dans l'octet ; bits 6/7 jamais utilisés
    (invariant non-attribut préservé par l'EOR du blit)."""
    rows = []
    for r in range(H):
        row = [0] * wb
        for c in range(W):
            if grid[r][c]:
                bit = q + c
                row[bit // 6] |= 0x20 >> (bit % 6)
        rows.extend(row)
    return rows


def main():
    radii = compute_radii()

    sprites = []          # par id (size*4+shape) : dict
    for size_id in range(N_SIZES):
        scale = SCALES[size_id]
        for shape_id in range(N_SHAPES):
            pixels = rasterize_shape(ATARI_SHAPES[shape_id], scale)
            ox, oy, W, H, grid = build_sprite(pixels)
            wb = (5 + W + 5) // 6          # ceil((5+W)/6) : pire phase q=5
            phases = []
            for p in range(6):
                q = (p + ox) % 6
                cold = (p + ox) // 6       # floor div python : [-3..0]
                phases.append({
                    'q': q, 'cold': cold,
                    'data': shift_bitmap(grid, W, H, q, wb),
                })
            sprites.append({
                'ox': ox, 'oy': oy, 'w': wb, 'h': H, 'phases': phases,
                'size': size_id, 'shape': shape_id, 'W': W,
            })

    total_bytes = sum(len(ph['data']) for s in sprites for ph in s['phases'])

    print("; ===============================================================")
    print("; shapes.s — auto-généré par tools/gen_shapes.py (Phase 26 / P3)")
    print("; PORTAGE ATARI ARCADE rev 4 — sprites pré-rendus XOR")
    print("; Source : 6502disassembly.com/va-asteroids/Asteroids.html")
    print("; NE PAS ÉDITER MANUELLEMENT — régénérer avec : make gen_shapes")
    print("; ===============================================================")
    print(f"; 4 shapes × 3 tailles, sommets COMPLETS : "
          f"{[len(s) for s in ATARI_SHAPES]} (décimation supprimée)")
    print(f"; Échelles : {SCALES}  Rayons collision : {radii}")
    print(f"; Bitmaps : 12 sprites × 6 phases = {total_bytes} octets")
    print()
    print('        .segment "RODATA"')
    print()
    print("        .export _spr_w, _spr_h, _spr_oy, _spr_cold")
    print("        .export _spr_lo, _spr_hi, _x_phase")
    print("        .export _shape_radii")
    print()

    def emit_bytes(name, vals, comment=None):
        print(f"{name}:")
        if comment:
            print(f"        ; {comment}")
        for i in range(0, len(vals), 12):
            print("        .byte " + ", ".join(f"${v:02X}" for v in vals[i:i+12]))
        print()

    emit_bytes("_spr_w", [s['w'] for s in sprites],
               "largeur en octets (stride), index = size*4+shape")
    emit_bytes("_spr_h", [s['h'] for s in sprites],
               "hauteur en rangées")
    emit_bytes("_spr_oy", [to_byte(s['oy']) for s in sprites],
               "offset y signé (coin haut vs centre)")
    emit_bytes("_spr_cold",
               [to_byte(ph['cold']) for s in sprites for ph in s['phases']],
               "delta colonne signé, index = id*6 + (cx mod 6)")

    print("_spr_lo:")
    print("        ; pointeur bitmap (octet bas), index = id*6 + (cx mod 6)")
    for i, s in enumerate(sprites):
        print("        .byte " + ", ".join(
            f"<spr_{i:02d}_p{p}" for p in range(6)))
    print()
    print("_spr_hi:")
    for i, s in enumerate(sprites):
        print("        .byte " + ", ".join(
            f">spr_{i:02d}_p{p}" for p in range(6)))
    print()

    print("_x_phase:")
    print("        ; x mod 6 — phase intra-octet d'une coordonnée pixel")
    print("        .repeat 240, I")
    print("            .byte I .MOD 6")
    print("        .endrepeat")
    print()

    print("_shape_radii:  ; rayon collision par taille (Phase 5)")
    print("        .byte " + ", ".join(str(r) for r in radii))
    print()

    for i, s in enumerate(sprites):
        print(f"; ── sprite {i:02d} : size {s['size']} shape {s['shape']} "
              f"— {s['W']}x{s['h']} px, {s['w']} o/rangée, "
              f"ox={s['ox']} oy={s['oy']} ──")
        for p, ph in enumerate(s['phases']):
            print(f"spr_{i:02d}_p{p}:   ; q={ph['q']} cold={ph['cold']}")
            data = ph['data']
            for j in range(0, len(data), 12):
                print("        .byte " + ", ".join(
                    f"${v:02X}" for v in data[j:j+12]))
        print()


if __name__ == "__main__":
    main()
