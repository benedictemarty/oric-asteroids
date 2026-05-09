/*
 * asteroids.c — Astéroïdes Phase 4
 *
 * Tableau d'astéroïdes en BSS. Chacun = (x, y, vx, vy, shape, size, active).
 * Rendu : 8 segments par astéroïde via _draw_line_xor.
 * Mouvement : intégration entière + wraparound zone safe (pas encore de
 * duplication d'instance — Phase 4b).
 * Fragmentation : grand → 2 moyens, moyen → 2 petits, petit → disparu.
 *
 * Le tableau est exposé à game.c via les fonctions ci-dessous.
 */

#include "asteroids.h"

extern unsigned char lx0, ly0, lx1, ly1;
#pragma zpsym ("lx0")
#pragma zpsym ("ly0")
#pragma zpsym ("lx1")
#pragma zpsym ("ly1")

void draw_line_xor(void);

/* Tables de sommets générées par tools/gen_shapes.py — Phase 10b N variable.
 *   shape_off[size*4+shape]  : offset (uint8) dans shape_x/y
 *   shape_len[size*4+shape]  : N sommets pour cette shape
 *   shape_x/y[total]         : 147 sommets (4 shapes × 3 tailles, N variable)
 */
extern const signed char shape_x[];
extern const signed char shape_y[];
extern const unsigned char shape_off[12];
extern const unsigned char shape_len[12];
extern const unsigned char shape_radii[3];

/* État des astéroïdes (BSS, zéro à l'init via bullets_init logique) */
Asteroid asteroids[MAX_ASTEROIDS];

/* RNG : LFSR 8-bit Galois (polynôme x^8 + x^6 + x^5 + x^4 + 1) */
static unsigned char rng_state;

unsigned char rng8(void)
{
    unsigned char lsb = rng_state & 1;
    rng_state >>= 1;
    if (lsb) rng_state ^= 0xB8;
    return rng_state;
}

void asteroids_init(unsigned char seed)
{
    unsigned char i;
    rng_state = seed ? seed : 0x42;
    for (i = 0; i < MAX_ASTEROIDS; i++) asteroids[i].active = 0;
}

/* Spawn la vague initiale : 4 grands aux 4 coins de la zone safe.
 * Vélocités fixes orientées vers le centre (varie selon la forme). */
void asteroids_spawn_wave(void)
{
    static const unsigned char sx[4] = { 30, 210,  30, 210};
    static const unsigned char sy[4] = { 30,  30, 170, 170};
    static const signed char   sv[4][2] = {
        {  1,  1 },     /* coin haut-gauche → vers ↘ */
        { -1,  1 },     /* coin haut-droit  → vers ↙ */
        {  1, -1 },     /* coin bas-gauche  → vers ↗ */
        { -1, -1 },     /* coin bas-droit   → vers ↖ */
    };
    unsigned char i;
    for (i = 0; i < 4; i++) {
        asteroids[i].x      = sx[i];
        asteroids[i].y      = sy[i];
        asteroids[i].vx     = sv[i][0];
        asteroids[i].vy     = sv[i][1];
        asteroids[i].shape  = i;
        asteroids[i].size   = SIZE_LARGE;
        asteroids[i].active = 1;
    }
    /* Marquer les autres comme libres */
    for (; i < MAX_ASTEROIDS; i++) asteroids[i].active = 0;
}

/* Mise à jour : intégration + wraparound zone safe.
 * Marge = 16 px (≥ rayon max grand 14 + tolérance) pour éviter overflow
 * vertices hors HIRES. La duplication d'instance (Phase 4b) supprimera
 * cette restriction. */
#define WRAP_X_MIN  16
#define WRAP_X_MAX  224
#define WRAP_Y_MIN  16
#define WRAP_Y_MAX  184
#define WRAP_X_SPAN (WRAP_X_MAX - WRAP_X_MIN)
#define WRAP_Y_SPAN (WRAP_Y_MAX - WRAP_Y_MIN)

void asteroids_update(void)
{
    unsigned char i;
    int nx, ny;
    for (i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) continue;
        nx = (int)asteroids[i].x + (int)asteroids[i].vx;
        ny = (int)asteroids[i].y + (int)asteroids[i].vy;
        if (nx < WRAP_X_MIN) nx += WRAP_X_SPAN;
        if (nx > WRAP_X_MAX) nx -= WRAP_X_SPAN;
        if (ny < WRAP_Y_MIN) ny += WRAP_Y_SPAN;
        if (ny > WRAP_Y_MAX) ny -= WRAP_Y_SPAN;
        asteroids[i].x = (unsigned char)nx;
        asteroids[i].y = (unsigned char)ny;
    }
}

/* Tracé d'un astéroïde — Phase 10b : N sommets variables.
 *   shape_idx = size * 4 + shape
 *   base      = shape_off[shape_idx]
 *   n         = shape_len[shape_idx]
 *   trace n segments vertex_i → vertex_((i+1) mod n) puis replot des
 *   n sommets (Phase 9g — polygone fermé, sommets partagés). */
static void asteroid_draw_one(unsigned char idx)
{
    unsigned char i, base, n, next;
    unsigned char shape_idx;
    unsigned char ax, ay;
    signed char dx0, dy0, dx1, dy1;

    ax = asteroids[idx].x;
    ay = asteroids[idx].y;
    shape_idx = (asteroids[idx].size << 2) + asteroids[idx].shape;
    base = shape_off[shape_idx];
    n    = shape_len[shape_idx];

    for (i = 0; i < n; i++) {
        next = (i + 1 == n) ? 0 : (i + 1);
        dx0 = shape_x[base + i];
        dy0 = shape_y[base + i];
        dx1 = shape_x[base + next];
        dy1 = shape_y[base + next];
        lx0 = (unsigned char)((signed char)ax + dx0);
        ly0 = (unsigned char)((signed char)ay + dy0);
        lx1 = (unsigned char)((signed char)ax + dx1);
        ly1 = (unsigned char)((signed char)ay + dy1);
        draw_line_xor();
    }
    /* Phase 9g : replot des N sommets (polygone fermé) */
    for (i = 0; i < n; i++) {
        dx0 = shape_x[base + i];
        dy0 = shape_y[base + i];
        lx0 = (unsigned char)((signed char)ax + dx0);
        ly0 = (unsigned char)((signed char)ay + dy0);
        lx1 = lx0;
        ly1 = ly0;
        draw_line_xor();
    }
}

void asteroids_draw(void)
{
    unsigned char i;
    for (i = 0; i < MAX_ASTEROIDS; i++) {
        if (asteroids[i].active) asteroid_draw_one(i);
    }
}

/* Fragmentation (Phase 5 utilisera : tir+collision → fragment).
 * Indice idx vers un astéroïde actif :
 *   size > 0 → désactive et crée 2 enfants taille-1 avec vélocités
 *              perturbées dans le RNG (angles ±θ approximation).
 *   size = 0 → désactive simplement (petit détruit). */
void asteroids_fragment(unsigned char idx)
{
    unsigned char child_size;
    unsigned char i, found;
    signed char vx, vy;
    unsigned char ax, ay, sh;

    if (!asteroids[idx].active) return;

    /* Sauver les attributs avant désactivation */
    ax = asteroids[idx].x;
    ay = asteroids[idx].y;
    vx = asteroids[idx].vx;
    vy = asteroids[idx].vy;
    sh = asteroids[idx].shape;
    asteroids[idx].active = 0;

    if (asteroids[idx].size == SIZE_SMALL) return;
    child_size = asteroids[idx].size - 1;

    /* Créer 2 enfants : enfant[0] perturbe (vx, vy) → (vx', vy')
     *                  enfant[1] perturbe → (vy, -vx) (rotation 90°)
     * Les enfants sont plus rapides : multiplier vx/vy par 2 (max ±2). */
    found = 0;
    for (i = 0; i < MAX_ASTEROIDS && found < 2; i++) {
        if (asteroids[i].active) continue;
        asteroids[i].active = 1;
        asteroids[i].size   = child_size;
        asteroids[i].shape  = (sh + found + 1) & 3;
        asteroids[i].x      = ax;
        asteroids[i].y      = ay;
        if (found == 0) {
            asteroids[i].vx =  (signed char)(vy);
            asteroids[i].vy = -(signed char)(vx);
        } else {
            asteroids[i].vx = -(signed char)(vy);
            asteroids[i].vy =  (signed char)(vx);
        }
        /* Accélérer un peu (taille -1 = plus rapide) en ajoutant un peu de RNG */
        if (rng8() & 1) {
            asteroids[i].vx = (signed char)(asteroids[i].vx * 2);
            asteroids[i].vy = (signed char)(asteroids[i].vy * 2);
        }
        found++;
    }
}

unsigned char asteroids_count(void)
{
    unsigned char i, n = 0;
    for (i = 0; i < MAX_ASTEROIDS; i++) if (asteroids[i].active) n++;
    return n;
}
