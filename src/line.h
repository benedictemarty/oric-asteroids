/*
 * line.h — API partagée du tracé de ligne (src/asm/line.s)
 *
 * Centralise les déclarations ZP / fonctions exportées par line.s
 * pour éviter la duplication des `extern` + `#pragma zpsym` dans
 * chaque .c (asteroids, ufo, hud, game, title — cf. revue senior
 * 2026-05-13 qualité C #2).
 *
 * Convention coordonnées :
 *   lx0/ly0 = point de départ (origine)
 *   lx1/ly1 = point d'arrivée (utilisé seulement par draw_line_xor,
 *             ignoré par plot_dot)
 *
 * Toutes les variables sont en zero page (cf. cfg/oric1.cfg).
 */

#ifndef LINE_H
#define LINE_H

/* ── Arguments des routines de tracé (zero page) ── */
extern unsigned char lx0, ly0, lx1, ly1;
#pragma zpsym ("lx0")
#pragma zpsym ("ly0")
#pragma zpsym ("lx1")
#pragma zpsym ("ly1")

/* ── Routines asm exportées par line.s ── */

/* Active le mode HIRES (240x200) + copie charset $B400 -> $9800 +
 * nettoie zone TEXT du bas. Appel unique au boot. */
void hires_init(void);

/* Trace une ligne XOR de (lx0, ly0) à (lx1, ly1) inclus.
 * Bresenham 6502 optimisé. Coordonnées doivent être dans [0..239] x [0..199].
 * Phase 2b : SMC sur step y direction + opérandes dx/dy.
 * Idempotent : appel pair = état initial (efface ce qui a été dessiné). */
void draw_line_xor(void);

/* Phase 24 — variante SEMI-OUVERTE : trace ](lx0,ly0), (lx1,ly1)], i.e.
 * tous les pixels SAUF le point de départ. Un segment dégénéré (P0 == P1)
 * ne trace rien. Pour les polygones fermés / chaînes orientées en
 * in-degree 1 : chaque sommet est XOR-é exactement une fois (visible),
 * sans replot compensatoire. Utilisé par ship.s et asteroids.c. */
void draw_line_xor_open(void);

/* XOR un seul pixel à (lx0, ly0). Plus rapide que draw_line_xor pour
 * un point isolé (~40c vs ~80c). Utilisé pour les replots de sommets
 * et les fragments. */
void plot_dot(void);

/* ── Phase 26 (P3) — blit sprite XOR ─────────────────────────────── */

/* Paramètres ZP du blit (alias des scratch du traceur — ne jamais
 * appeler draw_line entre leur écriture et spr_blit()). Le rectangle
 * passé doit être entièrement visible : le clipping rangées/colonnes
 * incombe à l'appelant (cf. asteroid_blit_at, asteroids.c). */
extern unsigned char *blt_sptr;       /* bitmap source (RODATA) */
extern unsigned char blt_row;         /* première rangée écran 0..199 */
extern unsigned char blt_col;         /* première colonne octet 0..39 */
extern unsigned char blt_stride;      /* octets/rangée du bitmap */
extern unsigned char blt_cnt;         /* octets visibles/rangée (>= 1) */
extern unsigned char blt_nrows;       /* rangées visibles (>= 1) */
#pragma zpsym ("blt_sptr")
#pragma zpsym ("blt_row")
#pragma zpsym ("blt_col")
#pragma zpsym ("blt_stride")
#pragma zpsym ("blt_cnt")
#pragma zpsym ("blt_nrows")

/* XOR-blit le rectangle décrit par les params ci-dessus. Idempotent
 * (2 blits identiques = effacement), bit 6 écran préservé. */
void spr_blit(void);

/* Colonne octet (x/6) d'une coordonnée pixel — table RODATA line.s. */
extern const unsigned char x_col[240];

#endif /* LINE_H */
