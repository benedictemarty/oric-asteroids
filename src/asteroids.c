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
void plot_dot(void);    /* Phase 16 — XOR 1 pixel rapide */

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

/* Phase 10h — ScrSpeedup arcade ($02FD) : seuil d'asteroids count en
 * dessous duquel le saucer apparaît plus souvent. Init à 5, +1 par
 * vague, max 11 (cf. InitGameVars $690E + InitWaveVars $717A-$7184). */
unsigned char scr_speedup;

/* Phase 10k — AstBreakTimer arcade ($02F9) : compteur frames après
 * destruction d'un asteroid. Pendant ce délai, le saucer ne peut pas
 * spawn. Reload à 80 dans BreakAsteroid ($75EE), décrément 1/frame
 * dans DoScrTmrUpdate ($6BAF). */
unsigned char ast_break_timer;

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
    scr_speedup = 5;            /* arcade : init à 5 */
    ast_break_timer = 0;        /* arcade : aucun hit en attente */
    for (i = 0; i < MAX_ASTEROIDS; i++) {
        asteroids[i].active = 0;
        asteroids[i].drawn  = 0;
    }
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
        ast_per_wave = 3;          /* réduit pour cap MAX_ASTEROIDS=6 (fragmentation × 3) */
    } else {
        ast_per_wave++;
        if (ast_per_wave > 5) ast_per_wave = 5;
        current_wave++;
        /* Phase 10h : ScrSpeedup += 1 (max 11) — saucer plus pressant */
        if (scr_speedup < 11) scr_speedup++;
    }
    n = ast_per_wave;
    if (n > MAX_ASTEROIDS) n = MAX_ASTEROIDS;

    for (i = 0; i < n; i++) {
        asteroids[i].active = 1;
        asteroids[i].drawn  = 0;            /* pas encore tracé : skip erase au 1er render */
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
        asteroids[i].prev_x = asteroids[i].x;
        asteroids[i].prev_y = asteroids[i].y;
        asteroids[i].x_frac = 0;
        asteroids[i].y_frac = 0;

        /* Vélocité 8.8 (scale ×128) : base RNG (1 ou 2 px) × 128
         * → 128 ou 256 = 0.5 ou 1.0 px/frame initial. */
        r = rng8();
        asteroids[i].vx = ((r & 1) ? 1 : -1) * ((r & 2) ? 256 : 128);
        asteroids[i].vy = ((r & 4) ? 1 : -1) * ((r & 8) ? 256 : 128);
    }
    /* Marquer les autres comme libres */
    for (; i < MAX_ASTEROIDS; i++) {
        asteroids[i].active = 0;
        asteroids[i].drawn  = 0;
    }
}

/* Mise à jour : intégration + wraparound écran complet (Phase 10l).
 * Le wraparound utilise désormais l'écran complet [0, 239] × [0, 199] ;
 * les asteroids près des bords dessinent leur instance fantôme à l'autre
 * extrémité (duplication d'instance arcade-fidèle).  Le clipping segment
 * par segment dans asteroid_draw_one_at évite l'overflow HIRES si des
 * vertices dépassent. */
#define WRAP_X_MIN  0
#define WRAP_X_MAX  239
#define WRAP_Y_MIN  0
#define WRAP_Y_MAX  199
#define WRAP_X_SPAN 240
#define WRAP_Y_SPAN 200

void asteroids_update(void)
{
    unsigned char i;
    unsigned int pos16;
    for (i = 0; i < MAX_ASTEROIDS; i++) {
        if (!asteroids[i].active) continue;
        /* Sauver pos courante comme prev (= pos où l'astéroïde est tracé en
         * sortie de frame précédente) avant de calculer la nouvelle pos. */
        asteroids[i].prev_x = asteroids[i].x;
        asteroids[i].prev_y = asteroids[i].y;

        /* 8.8 fixed-point : recompose pos16 = (x<<8)|x_frac, ajoute vx
         * (signed → unsigned cast modulaire), reséparer. Mouvement
         * sub-pixel lisse à 25 Hz. */
        pos16 = ((unsigned int)asteroids[i].x << 8) | asteroids[i].x_frac;
        pos16 += (unsigned int)asteroids[i].vx;
        asteroids[i].x      = (unsigned char)(pos16 >> 8);
        asteroids[i].x_frac = (unsigned char)(pos16 & 0xFF);
        if (asteroids[i].x < WRAP_X_MIN) asteroids[i].x += WRAP_X_SPAN;
        if (asteroids[i].x > WRAP_X_MAX) asteroids[i].x -= WRAP_X_SPAN;

        pos16 = ((unsigned int)asteroids[i].y << 8) | asteroids[i].y_frac;
        pos16 += (unsigned int)asteroids[i].vy;
        asteroids[i].y      = (unsigned char)(pos16 >> 8);
        asteroids[i].y_frac = (unsigned char)(pos16 & 0xFF);
        if (asteroids[i].y < WRAP_Y_MIN) asteroids[i].y += WRAP_Y_SPAN;
        if (asteroids[i].y > WRAP_Y_MAX) asteroids[i].y -= WRAP_Y_SPAN;
    }
}

/* Phase 10l — vérifie qu'une coord (x, y) en int signé est dans
 * la zone HIRES valide [0, 239] × [0, 199]. Permet le clipping
 * segment par segment lors de la duplication d'instance. */
#define IN_BOUNDS(x, y)  ((x) >= 0 && (x) <= 239 && (y) >= 0 && (y) <= 199)

/* Tracé d'un astéroïde à un centre (cx, cy) avec offset (ox, oy).
 * Le centre est passé en paramètre (au lieu de lire asteroids[idx].x/y)
 * pour permettre erase à prev_x/prev_y et draw à x/y dans la même frame
 * (pattern erase+draw consécutif anti-flicker).
 * Permet la duplication d'instance : (0, 0) pour l'instance principale,
 * (±240, 0) ou (0, ±200) pour les instances fantômes près des bords.
 * Clipping segment-par-segment : skip si une extrémité hors HIRES. */
static void asteroid_draw_at(unsigned char idx, unsigned char cx, unsigned char cy,
                             int ox, int oy)
{
    unsigned char i, base, n, next;
    unsigned char shape_idx;
    int ax, ay;
    int x0, y0, x1, y1;
    signed char dx0, dy0, dx1, dy1;

    ax = (int)(unsigned int)cx + ox;
    ay = (int)(unsigned int)cy + oy;
    shape_idx = (asteroids[idx].size << 2) + asteroids[idx].shape;
    base = shape_off[shape_idx];
    n    = shape_len[shape_idx];

    for (i = 0; i < n; i++) {
        next = (i + 1 == n) ? 0 : (i + 1);
        dx0 = shape_x[base + i];
        dy0 = shape_y[base + i];
        dx1 = shape_x[base + next];
        dy1 = shape_y[base + next];
        x0 = ax + (int)dx0;
        y0 = ay + (int)dy0;
        x1 = ax + (int)dx1;
        y1 = ay + (int)dy1;
        if (IN_BOUNDS(x0, y0) && IN_BOUNDS(x1, y1)) {
            lx0 = (unsigned char)x0;
            ly0 = (unsigned char)y0;
            lx1 = (unsigned char)x1;
            ly1 = (unsigned char)y1;
            draw_line_xor();
        }
    }
    /* Phase 9g : replot des N sommets (polygone fermé) — clippés pareil.
     * Phase 16 : utilise plot_dot (40 c/plot vs ~80 c via draw_line_xor). */
    for (i = 0; i < n; i++) {
        dx0 = shape_x[base + i];
        dy0 = shape_y[base + i];
        x0 = ax + (int)dx0;
        y0 = ay + (int)dy0;
        if (IN_BOUNDS(x0, y0)) {
            lx0 = (unsigned char)x0;
            ly0 = (unsigned char)y0;
            plot_dot();
        }
    }
}

/* Phase 10l — duplication d'instance : asteroid près d'un bord est
 * dessiné aussi à son emplacement "fantôme" de l'autre côté de l'écran.
 * Bords (rayon ~14 px) :
 *   - x ≤ 14 : copie à x + 240
 *   - x ≥ 226 : copie à x - 240
 *   - idem Y avec 200
 *   - coins : 4× (1 + dx + dy + dx_dy)
 * Le centre (cx, cy) est passé en paramètre pour permettre le pattern
 * erase à prev_pos puis draw à curr_pos sans modifier la struct. */
static void asteroid_draw_one(unsigned char idx, unsigned char cx, unsigned char cy)
{
    /* Rayon max conservateur : 14 (= rayon grand asteroid) */
    int dup_x = 0, dup_y = 0;

    asteroid_draw_at(idx, cx, cy, 0, 0);

    if (cx <= 14)        dup_x = +240;
    else if (cx >= 226)  dup_x = -240;
    if (cy <= 14)        dup_y = +200;
    else if (cy >= 186)  dup_y = -200;

    if (dup_x) asteroid_draw_at(idx, cx, cy, dup_x, 0);
    if (dup_y) asteroid_draw_at(idx, cx, cy, 0, dup_y);
    if (dup_x && dup_y) asteroid_draw_at(idx, cx, cy, dup_x, dup_y);
}

/* Trace XOR à pos courante (x, y) pour chaque actif.
 * Met à jour prev_x/prev_y/drawn pour amorcer le pattern erase+draw
 * de la frame suivante (asteroids_render). Utilisé pour init / game_reset
 * / nettoyage : appel en paire (draw puis re-draw) = annule l'XOR. */
void asteroids_draw(void)
{
    unsigned char i;
    for (i = 0; i < MAX_ASTEROIDS; i++) {
        if (asteroids[i].active) {
            asteroid_draw_one(i, asteroids[i].x, asteroids[i].y);
            asteroids[i].prev_x = asteroids[i].x;
            asteroids[i].prev_y = asteroids[i].y;
            asteroids[i].drawn  = !asteroids[i].drawn;  /* toggle XOR */
        }
    }
}

/* Pattern par-frame anti-flicker : pour chaque asteroid, erase à prev
 * (= pos en sortie de frame précédente) puis draw à curr consécutivement.
 * La fenêtre pendant laquelle l'astéroïde est éteint à l'écran tombe à
 * quelques centaines de cycles (les opérations entre les deux appels à
 * asteroid_draw_one), au lieu de plusieurs ms si le couple était dispersé.
 * Pré-conditions :
 *   - asteroids_update doit avoir tourné cette frame (sauve prev_x/y).
 *   - drawn = 1 ssi l'asteroid est actuellement tracé à (prev_x, prev_y).
 * Post-conditions :
 *   - chaque actif est tracé à (x, y), prev = (x, y), drawn = 1.
 *   - chaque inactif (juste désactivé par collision) est effacé,
 *     drawn = 0. */
void asteroids_render(void)
{
    unsigned char i;
    for (i = 0; i < MAX_ASTEROIDS; i++) {
        if (asteroids[i].drawn) {
            asteroid_draw_one(i, asteroids[i].prev_x, asteroids[i].prev_y);
            asteroids[i].drawn = 0;
        }
        if (asteroids[i].active) {
            asteroid_draw_one(i, asteroids[i].x, asteroids[i].y);
            asteroids[i].prev_x = asteroids[i].x;
            asteroids[i].prev_y = asteroids[i].y;
            asteroids[i].drawn  = 1;
        }
    }
}

/* 8.8 fixed-point pour asteroid velocities — scale ×128 (= 0.5 px/unit) :
 *   V_MAX_AST = 15 × 128 = 1920 ≈ 7.5 px/frame max
 *   V_MIN_AST =  3 × 128 =  384 ≈ 1.5 px/frame min (asteroid ne peut pas s'arrêter)
 */
#define V_MAX_AST   1920
#define V_MIN_AST    384

/* RNG signé en 8.8 — port de SetAstVel ($7203) :
 *   AND #$8F garde sign + 4 bits magnitude, scale ×128.  */
static int rand_offset(void)
{
    unsigned char r = rng8() & 0x8F;
    signed char s;
    if (r & 0x80) r |= 0xF0;
    s = (signed char)r;
    return ((int)s) << 7;     /* ×128 = scale 8.8 (matches V_MAX/MIN_AST) */
}

/* Clamp arcade GetAstVelocity ($7233) en 8.8 fixed-point. */
static int clamp_vel(int v)
{
    if (v >  V_MAX_AST)        v =  V_MAX_AST;
    if (v < -V_MAX_AST)        v = -V_MAX_AST;
    if (v > 0 && v <  V_MIN_AST) v =  V_MIN_AST;
    if (v < 0 && v > -V_MIN_AST) v = -V_MIN_AST;
    return v;
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
    int vx_p, vy_p;                  /* 8.8 fixed-point : int 16-bit */
    unsigned char ax, ay;

    if (!asteroids[idx].active) return;

    /* Phase 10k : reload AstBreakTimer = 80 (arcade $75EE).  Tout hit
     * d'asteroid retarde le prochain spawn saucer de 80 frames. */
    ast_break_timer = 80;

    /* Petit détruit complètement (cf. arcade : size devient 0, asteroid disparu).
     * Le tracé existant à prev_x/prev_y sera effacé par asteroids_render
     * (drawn=1, active=0 → erase puis drawn=0). */
    if (asteroids[idx].size == SIZE_SMALL) {
        asteroids[idx].active = 0;
        return;
    }

    /* L'astéroïde parent était tracé à prev_x/prev_y avec sa size/shape
     * actuelles (= attrs OLD). On va modifier size/shape ci-dessous, ce
     * qui changerait le shape utilisé par asteroids_render lors de l'erase.
     * Pour préserver l'invariant, on efface immédiatement le tracé OLD
     * avant de modifier les attrs. drawn=0 → render skip erase, draw à
     * x/y avec NEW attrs. */
    if (asteroids[idx].drawn) {
        asteroid_draw_one(idx, asteroids[idx].prev_x, asteroids[idx].prev_y);
        asteroids[idx].drawn = 0;
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
    asteroids[idx].vx    = clamp_vel(vx_p + rand_offset());
    asteroids[idx].vy    = clamp_vel(vy_p + rand_offset());

    /* 2. Créer 2 NOUVEAUX asteroids dans des slots libres */
    found = 0;
    for (i = 0; i < MAX_ASTEROIDS && found < 2; i++) {
        if (i == idx || asteroids[i].active) continue;
        asteroids[i].active = 1;
        asteroids[i].drawn  = 0;            /* jamais tracés : skip erase au render */
        asteroids[i].size   = child_size;
        asteroids[i].shape  = (sh + 2 + found) & 3;
        asteroids[i].x      = ax;
        asteroids[i].y      = ay;
        asteroids[i].prev_x = ax;
        asteroids[i].prev_y = ay;
        asteroids[i].x_frac = 0;
        asteroids[i].y_frac = 0;
        asteroids[i].vx     = clamp_vel(vx_p + rand_offset());
        asteroids[i].vy     = clamp_vel(vy_p + rand_offset());
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
