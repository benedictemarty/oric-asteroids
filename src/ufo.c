/*
 * ufo.c — Soucoupe Phase 6
 *
 * Une seule soucoupe simultanée. Apparition cyclique sur un bord (gauche
 * ou droit selon RNG), traverse l'écran horizontalement avec petites
 * inflexions verticales, disparaît à l'autre bord.
 *
 * Forme dessinée en 5 segments (corps trapézoïdal + dôme + ligne médiane) :
 *
 *           __                   delta = (-3, -3)→(+3, -3)
 *         _/  \_                 (-7, -1)→(-3, -3) ; (+3, -3)→(+7, -1)
 *        |______|                ligne médiane : (-7, -1)→(+7, -1)
 *        \______/                base ouverte : (-7, -1)→(-3, +3) ;
 *                                                (+3, +3)→(+7, -1) ;
 *                                                (-3, +3)→(+3, +3)
 *
 * 2 tailles : grande (multiplicateur ×1) et petite (×0.6 ≈ /1.667).
 *
 * IA :
 *   - UFO_LARGE : tir dans une direction aléatoire (8 dirs) à intervalles.
 *   - UFO_SMALL : visée approximative vers le ship + bruit RNG.
 *     Précision augmentée par un seuil sur le score (Asteroids original
 *     calque cette logique sur des tables Atari, ici simplifié en
 *     "score ≥ 5000 → bruit /2").
 */

#include "ufo.h"
#include "asteroids.h"      /* rng8, scr_speedup, asteroids_count */

extern unsigned char lx0, ly0, lx1, ly1;
#pragma zpsym ("lx0")
#pragma zpsym ("ly0")
#pragma zpsym ("lx1")
#pragma zpsym ("ly1")

void draw_line_xor(void);

/* ------------------------------------------------------------------ */
/* État UFO (BSS)                                                      */
/* ------------------------------------------------------------------ */

unsigned char ufo_active;
unsigned char ufo_x, ufo_y;
signed char   ufo_vx, ufo_vy;
unsigned char ufo_type;
unsigned char ufo_bullet_active;
unsigned char ufo_bullet_x, ufo_bullet_y;
signed char   ufo_bullet_vx, ufo_bullet_vy;
unsigned char ufo_bullet_ttl;

static unsigned int  ufo_spawn_timer;     /* frames avant prochain spawn */
static unsigned char ufo_fire_timer;      /* frames avant prochain tir */
static unsigned char ufo_drift_timer;     /* alterne vy ±1 */

#define UFO_BULLET_TTL      30
#define UFO_BULLET_SPEED    4
#define UFO_FIRE_PERIOD     35    /* ~2 s à 17 Hz */
#define UFO_SPAWN_PERIOD    300U  /* ~18 s normal */
#define UFO_SPAWN_FAST      120U  /* ~7 s quand asteroids_count ≤ ScrSpeedup */
#define UFO_DRIFT_PERIOD    20

/* Segments delta — grande UFO (rayon ~7 px). Format : x0, y0, x1, y1 */
#define N_SEGS 7
static const signed char seg_large[N_SEGS][4] = {
    /* ligne médiane (corps haut) */
    { -7, -1,  +7, -1 },
    /* base ouverte */
    { -3, +3,  +3, +3 },
    /* corps trapézoïdal */
    { -7, -1,  -3, +3 },
    {  3, +3,  +7, -1 },
    /* dôme */
    { -3, -3,  +3, -3 },
    { -3, -3,  -1, -1 },
    {  3, -3,  +1, -1 },
};

/* Segments delta — petite UFO (×0.6, arrondi) */
static const signed char seg_small[N_SEGS][4] = {
    { -4, -1,  +4, -1 },
    { -2, +2,  +2, +2 },
    { -4, -1,  -2, +2 },
    {  2, +2,  +4, -1 },
    { -2, -2,  +2, -2 },
    { -2, -2,   0, -1 },
    {  2, -2,   0, -1 },
};

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void line(unsigned char x0, unsigned char y0,
                 unsigned char x1, unsigned char y1)
{
    lx0 = x0; ly0 = y0; lx1 = x1; ly1 = y1;
    draw_line_xor();
}

unsigned char ufo_radius(void)
{
    return (ufo_type == UFO_LARGE) ? 7 : 4;
}

/* ------------------------------------------------------------------ */
/* État                                                                */
/* ------------------------------------------------------------------ */

void ufo_init(void)
{
    ufo_active = 0;
    ufo_bullet_active = 0;
    ufo_spawn_timer = UFO_SPAWN_PERIOD;
    ufo_fire_timer = UFO_FIRE_PERIOD;
    ufo_drift_timer = UFO_DRIFT_PERIOD;
}

void ufo_kill(void)
{
    ufo_active = 0;
    ufo_bullet_active = 0;
}

/* Spawn : choix taille (RNG biaisé), bord et direction */
static void ufo_spawn(unsigned int score_in)
{
    unsigned char r;
    /* Probabilité de small UFO augmente avec le score (cf. arcade) :
     *   score < 1000 : 0% small ; >= 5000 : 30% ; >= 10000 : 50%. */
    r = rng8();
    if (score_in >= 10000U)      ufo_type = (r < 128) ? UFO_SMALL : UFO_LARGE;
    else if (score_in >= 5000U)  ufo_type = (r <  77) ? UFO_SMALL : UFO_LARGE;
    else                         ufo_type = UFO_LARGE;

    /* Bord et direction */
    r = rng8();
    if (r & 1) {
        ufo_x = 8;
        ufo_vx = 1;
    } else {
        ufo_x = 232;
        ufo_vx = -1;
    }
    /* Y aléatoire dans la zone safe (en dessous du HUD) */
    ufo_y = 30 + (rng8() & 0x7F);     /* 30 à 157 */
    if (ufo_y > 170) ufo_y = 170;
    ufo_vy = 0;
    ufo_active = 1;
    ufo_fire_timer = UFO_FIRE_PERIOD;
    ufo_drift_timer = UFO_DRIFT_PERIOD;
}

/* Tir UFO : grande = direction aléatoire ; petite = vers le ship + bruit */
static void ufo_fire(unsigned char ship_x_in, unsigned char ship_y_in,
                     unsigned int score_in)
{
    signed char dx, dy;
    unsigned char r;
    if (ufo_bullet_active) return;
    if (ufo_type == UFO_LARGE) {
        /* Direction aléatoire 8-way */
        r = rng8() & 7;
        switch (r) {
            case 0: dx = +UFO_BULLET_SPEED; dy = 0; break;
            case 1: dx = +UFO_BULLET_SPEED; dy = +UFO_BULLET_SPEED; break;
            case 2: dx = 0;                 dy = +UFO_BULLET_SPEED; break;
            case 3: dx = -UFO_BULLET_SPEED; dy = +UFO_BULLET_SPEED; break;
            case 4: dx = -UFO_BULLET_SPEED; dy = 0; break;
            case 5: dx = -UFO_BULLET_SPEED; dy = -UFO_BULLET_SPEED; break;
            case 6: dx = 0;                 dy = -UFO_BULLET_SPEED; break;
            default: dx = +UFO_BULLET_SPEED; dy = -UFO_BULLET_SPEED; break;
        }
    } else {
        /* Visée approximative vers le ship.
         * Phase 10g — port arcade rev 4 (CalcScrShotDir + ScrShotAddOffset
         * à $6CA5+).  L'arcade utilise un seuil unique à 35000 :
         *   - score < 35000 : précision faible (mask $8F → bruit ±15)
         *   - score ≥ 35000 : précision élevée (mask $87 → bruit ±7)
         * En dessous on utilise des seuils adaptés à notre échelle 8-dir. */
        signed char ddx = (signed char)(ship_x_in - ufo_x);
        signed char ddy = (signed char)(ship_y_in - ufo_y);
        if (ddx > 4)       dx = +UFO_BULLET_SPEED;
        else if (ddx < -4) dx = -UFO_BULLET_SPEED;
        else               dx = 0;
        if (ddy > 4)       dy = +UFO_BULLET_SPEED;
        else if (ddy < -4) dy = -UFO_BULLET_SPEED;
        else               dy = 0;
        /* Précision indexée sur le score (port arcade $6CB1) :
         *   - score < 35000 : 50% chance de mauvaise direction
         *   - score ≥ 35000 : 12% chance (rare) — petit UFO devient "tueur" */
        if (score_in < 35000U) {
            if (rng8() & 1) {           /* 50% de bruit */
                r = rng8() & 1;
                if (r) dx = -dx;
                else   dy = -dy;
            }
        } else {
            if ((rng8() & 7) == 0) {    /* 12% de bruit */
                r = rng8() & 1;
                if (r) dx = -dx;
                else   dy = -dy;
            }
        }
        /* Si visée nulle (ship juste sur l'UFO), tir vers le bas */
        if (dx == 0 && dy == 0) dy = +UFO_BULLET_SPEED;
    }
    ufo_bullet_x = ufo_x;
    ufo_bullet_y = ufo_y;
    ufo_bullet_vx = dx;
    ufo_bullet_vy = dy;
    ufo_bullet_ttl = UFO_BULLET_TTL;
    ufo_bullet_active = 1;
}

/* ------------------------------------------------------------------ */
/* Mise à jour de la frame                                             */
/* ------------------------------------------------------------------ */

void ufo_tick(unsigned char ship_x_in, unsigned char ship_y_in,
              unsigned int score_in)
{
    int nx;
    if (!ufo_active) {
        if (ufo_spawn_timer == 0) {
            ufo_spawn(score_in);
            /* Phase 10h : reload accéléré si asteroids_count ≤ ScrSpeedup
             * (cf. arcade $6BC6-$6BCE — saucer pressant en fin de vague). */
            if (asteroids_count() <= scr_speedup) {
                ufo_spawn_timer = UFO_SPAWN_FAST;
            } else {
                ufo_spawn_timer = UFO_SPAWN_PERIOD;
            }
        } else {
            ufo_spawn_timer--;
        }
        return;
    }
    /* Mouvement horizontal */
    nx = (int)ufo_x + (int)ufo_vx;
    if (nx < 4 || nx > 235) {
        /* Sortie d'écran : disparaît, attend prochain spawn */
        ufo_active = 0;
        return;
    }
    ufo_x = (unsigned char)nx;
    /* Drift vertical : tous les UFO_DRIFT_PERIOD frames, change vy */
    if (ufo_drift_timer == 0) {
        ufo_drift_timer = UFO_DRIFT_PERIOD;
        switch (rng8() & 3) {
            case 0:  ufo_vy = -1; break;
            case 1:  ufo_vy =  0; break;
            default: ufo_vy = +1; break;
        }
    } else {
        ufo_drift_timer--;
    }
    if (ufo_vy) {
        nx = (int)ufo_y + (int)ufo_vy;
        if (nx >= 25 && nx <= 175) ufo_y = (unsigned char)nx;
    }
    /* Tir périodique */
    if (ufo_fire_timer == 0) {
        ufo_fire(ship_x_in, ship_y_in, score_in);
        ufo_fire_timer = UFO_FIRE_PERIOD;
    } else {
        ufo_fire_timer--;
    }
}

void ufo_draw(void)
{
    unsigned char i;
    const signed char (*segs)[4];
    if (!ufo_active) return;
    segs = (ufo_type == UFO_LARGE) ? seg_large : seg_small;
    for (i = 0; i < N_SEGS; i++) {
        line((unsigned char)((signed char)ufo_x + segs[i][0]),
             (unsigned char)((signed char)ufo_y + segs[i][1]),
             (unsigned char)((signed char)ufo_x + segs[i][2]),
             (unsigned char)((signed char)ufo_y + segs[i][3]));
    }
}

/* ------------------------------------------------------------------ */
/* Tir UFO                                                             */
/* ------------------------------------------------------------------ */

void ufo_bullet_update(void)
{
    int nx, ny;
    if (!ufo_bullet_active) return;
    if (ufo_bullet_ttl == 0) {
        ufo_bullet_active = 0;
        return;
    }
    nx = (int)ufo_bullet_x + (int)ufo_bullet_vx;
    ny = (int)ufo_bullet_y + (int)ufo_bullet_vy;
    if (nx < 1 || nx > 238 || ny < 1 || ny > 198) {
        ufo_bullet_active = 0;
        return;
    }
    ufo_bullet_x = (unsigned char)nx;
    ufo_bullet_y = (unsigned char)ny;
    ufo_bullet_ttl--;
}

void ufo_bullet_draw(void)
{
    if (!ufo_bullet_active) return;
    line(ufo_bullet_x, ufo_bullet_y, ufo_bullet_x, ufo_bullet_y);
}
