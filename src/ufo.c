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
#include "line.h"
#include "sound.h"          /* Phase 22 : canal C UFO géré ici */

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

/* Phase 36 — état écran du tir, découplé de ufo_bullet_active
 * (cf. ufo_bullet_commit). Jamais réinitialisé par ufo_init : ce qui
 * est en VRAM y reste tant qu'un commit ne l'a pas effacé. */
static unsigned char ufo_blt_drawn;       /* 1 = tir actuellement en VRAM */
static unsigned char ufo_blt_px, ufo_blt_py;  /* position du dernier draw */

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
    sound_stop_ufo();   /* Phase 22 : arrêter canal C immédiatement */
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
    /* Phase 22 : démarrer canal C UFO (auto-restart dans sound_tick) */
    sound_play_fx(ufo_type == UFO_LARGE ? FX_UFO : FX_UFO_SMALL);
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
        /* Phase 10k : décrément AstBreakTimer (arcade $6BAF-$6BB7).
         * Pendant ce délai, on bloque le spawn saucer. */
        if (ast_break_timer) ast_break_timer--;

        if (ufo_spawn_timer == 0) {
            /* Phase 10k : conditions arcade pour empêcher spawn :
             *   - AstBreakTimer > 0 (asteroid détruit récemment)
             *   - 0 asteroids actifs (vague vide, transition imminente)
             *   cf. UpdateScr $6BBF-$6BC9. */
            if (ast_break_timer == 0 && asteroids_count() != 0) {
                ufo_spawn(score_in);
                /* Phase 10h : reload accéléré si asteroids_count ≤ ScrSpeedup */
                if (asteroids_count() <= scr_speedup) {
                    ufo_spawn_timer = UFO_SPAWN_FAST;
                } else {
                    ufo_spawn_timer = UFO_SPAWN_PERIOD;
                }
            }
            /* Si bloqué, on garde le timer à 0 → re-test prochaine frame */
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
        sound_stop_ufo();   /* Phase 22 : stopper canal C */
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
    /* Pas de check ufo_active : permet d'effacer (XOR) l'UFO après
     * ufo_kill() — ufo_x/y/type sont préservés par kill, donc l'erase
     * trace au bon endroit. Caller responsabilité : ne pas appeler
     * avant qu'un UFO ait été drawn au moins une fois (= caller utilise
     * un flag ufo_was_drawn). */
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
    /* Wraparound arcade : dans l'Atari rev, UFO et joueur partagent les
     * mêmes shot slots ($021F-$0222) ⇒ même traitement aux bords. Bullet
     * UFO = bloc 2×2 px (cf. ufo_bullet_commit), donc x ∈ [0..238],
     * y ∈ [0..198]. Vélocité max ±4 ⇒ un seul ajustement par bord
     * suffit. TTL borne la portée totale. */
    if (nx < 0)        nx += 239;
    else if (nx > 238) nx -= 239;
    if (ny < 0)        ny += 199;
    else if (ny > 198) ny -= 199;
    ufo_bullet_x = (unsigned char)nx;
    ufo_bullet_y = (unsigned char)ny;
    ufo_bullet_ttl--;
}

/* Bloc 2×2 px en XOR (plot_dot ne modifie pas lx0/ly0 ; 4 × 40 c,
 * moins cher que 4 line()). L'appelant garantit x ≤ 238, y ≤ 198. */
static void ufo_dot4(unsigned char x, unsigned char y)
{
    lx0 = x;
    ly0 = y;
    plot_dot();
    lx0 = (unsigned char)(x + 1);
    plot_dot();
    ly0 = (unsigned char)(y + 1);
    plot_dot();
    lx0 = x;
    plot_dot();
}

void ufo_bullet_commit(void)
{
    /* Phase 36 — même schéma que bullets_commit (game.c) : erase à la
     * position du dernier draw, immédiatement suivi du draw à la
     * position courante. L'état écran (ufo_blt_drawn) est découplé de
     * ufo_bullet_active : un tir tué n'importe où (collision asteroid,
     * ship, ufo_kill, TTL) est toujours effacé exactement là où il a
     * été dessiné — aucun pixel fantôme.
     *
     * Bloc 2×2 px, aligné sur les torpilles joueur (1 px était encore
     * moins lisible — retour testeur matériel réel 2026-06-12). Spawn à
     * ufo_x/ufo_y ∈ [4..235]×[25..175] et clip update ⇒ x ≤ 238,
     * y ≤ 198, pas d'overflow à +1. */
    if (ufo_blt_drawn) {
        ufo_dot4(ufo_blt_px, ufo_blt_py);           /* erase */
        ufo_blt_drawn = 0;
    }
    if (ufo_bullet_active) {
        ufo_dot4(ufo_bullet_x, ufo_bullet_y);       /* draw */
        ufo_blt_px = ufo_bullet_x;
        ufo_blt_py = ufo_bullet_y;
        ufo_blt_drawn = 1;
    }
}
