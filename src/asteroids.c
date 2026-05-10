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

/* Phase 10c — wave counter arcade */
unsigned char current_wave;
static unsigned char ast_per_wave;

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
    current_wave = 0;
    ast_per_wave = 0;
    for (i = 0; i < MAX_ASTEROIDS; i++) asteroids[i].active = 0;
}

/* Spawn d'une vague — Phase 10c arcade-fidèle.
 * Cf. InitWaveVars à $7168 (`AstPerWave += 2`, max 11).
 *
 * Logique transposée de `InitWaveAsteroids` ($719B-$71D5) :
 *   - Pour chaque asteroid : choisir shape aléatoirement
 *   - Position aléatoire alternant : top/bottom (Y=0/max, X aléa)
 *     ou left/right (X=0/max, Y aléa) selon RNG bit
 *   - Vélocité aléatoire (RNG sur signe + magnitude) */
void asteroids_spawn_wave(void)
{
    unsigned char i, n, r;

    if (current_wave == 0) {
        current_wave = 1;
        ast_per_wave = 4;
    } else {
        ast_per_wave += 2;
        if (ast_per_wave > 11) ast_per_wave = 11;
        current_wave++;
    }
    n = ast_per_wave;
    if (n > MAX_ASTEROIDS) n = MAX_ASTEROIDS;

    for (i = 0; i < n; i++) {
        asteroids[i].active = 1;
        asteroids[i].size   = SIZE_LARGE;
        /* Shape aléatoire 0-3 (cf. arcade : AND #$18 puis ORA #$04 → status) */
        asteroids[i].shape  = (rng8() >> 3) & 3;

        /* Position : RNG bit 0 décide top/bottom ou left/right */
        r = rng8();
        if (r & 1) {
            /* Apparaît sur bord gauche/droit, Y aléatoire */
            asteroids[i].x = (r & 2) ? 12 : 220;
            asteroids[i].y = 24 + (rng8() % 144);     /* zone safe Y */
        } else {
            /* Apparaît sur bord haut/bas, X aléatoire */
            asteroids[i].x = 20 + (rng8() % 192);     /* zone safe X */
            asteroids[i].y = (r & 2) ? 20 : 180;
        }

        /* Vélocité : RNG sur signe + magnitude (1 ou 2) */
        r = rng8();
        asteroids[i].vx = ((r & 1) ? 1 : -1) * ((r & 2) ? 2 : 1);
        asteroids[i].vy = ((r & 4) ? 1 : -1) * ((r & 8) ? 2 : 1);
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

/* RNG signé dans [-15, +15] — port de SetAstVel ($7203) :
 *   AND #$8F garde sign + 4 bits magnitude
 *   Si négatif : ORA #$F0 (sign extension) → range complète.  */
static signed char rand_offset(void)
{
    unsigned char r = rng8() & 0x8F;
    if (r & 0x80) r |= 0xF0;
    return (signed char)r;
}

/* Clamp arcade GetAstVelocity ($7233) adapté au format vx/vy 8-bit Oric :
 *   - max |v| = 15 (arcade : 31, divisé par 2 pour notre échelle)
 *   - min |v| = 3 (arcade : 6, idem)
 *   - signe préservé (asteroid ne peut pas s'arrêter)
 */
static signed char clamp_vel(int v)
{
    if (v > 15)  v = 15;
    if (v < -15) v = -15;
    if (v > 0  && v < 3)  v = 3;
    if (v < 0  && v > -3) v = -3;
    return (signed char)v;
}

/* Fragmentation arcade-fidèle — Phase 10f.
 * Port de BreakAsteroid ($75EC) + SplitAsteroid ($761D) + SetAstVel ($7203).
 *
 * 1. Petit détruit (size==0) : désactivation, fini.
 * 2. Sinon : taille -- en place, parent reste actif (slot conservé).
 * 3. SplitAsteroid : crée 2 nouveaux asteroids même taille à la même position.
 * 4. SetAstVel pour chaque enfant + parent : nouvelle_vel = parent_vel + RNG(-15..+15),
 *    clampé [-15,+15] avec |v|≥3.
 * Total : 1 parent réduit + 2 nouveaux = **3 fragments** par hit (vs 2 avant). */
void asteroids_fragment(unsigned char idx)
{
    unsigned char child_size;
    unsigned char i, found, sh;
    signed char vx_p, vy_p;
    unsigned char ax, ay;

    if (!asteroids[idx].active) return;

    /* Petit détruit complètement (cf. arcade : size devient 0, asteroid disparu) */
    if (asteroids[idx].size == SIZE_SMALL) {
        asteroids[idx].active = 0;
        return;
    }

    /* Conserver les attributs du parent avant modification */
    ax   = asteroids[idx].x;
    ay   = asteroids[idx].y;
    vx_p = asteroids[idx].vx;
    vy_p = asteroids[idx].vy;
    sh   = asteroids[idx].shape;
    child_size = asteroids[idx].size - 1;

    /* 1. Parent : taille réduite, slot conservé, vélocité perturbée */
    asteroids[idx].size  = child_size;
    asteroids[idx].shape = (sh + 1) & 3;
    asteroids[idx].vx    = clamp_vel((int)vx_p + (int)rand_offset());
    asteroids[idx].vy    = clamp_vel((int)vy_p + (int)rand_offset());

    /* 2. Créer 2 NOUVEAUX asteroids dans des slots libres */
    found = 0;
    for (i = 0; i < MAX_ASTEROIDS && found < 2; i++) {
        if (i == idx || asteroids[i].active) continue;
        asteroids[i].active = 1;
        asteroids[i].size   = child_size;
        asteroids[i].shape  = (sh + 2 + found) & 3;
        asteroids[i].x      = ax;
        asteroids[i].y      = ay;
        asteroids[i].vx     = clamp_vel((int)vx_p + (int)rand_offset());
        asteroids[i].vy     = clamp_vel((int)vy_p + (int)rand_offset());
        found++;
    }
    /* Si pas assez de slots libres : pool plein, on accepte la perte
     * (arcade fait pareil via GetFreeAstSlot qui retourne BMI). */
}

unsigned char asteroids_count(void)
{
    unsigned char i, n = 0;
    for (i = 0; i < MAX_ASTEROIDS; i++) if (asteroids[i].active) n++;
    return n;
}
