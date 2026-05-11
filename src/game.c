/*
 * game.c — Boucle de jeu Phases 3 → 7
 *
 * Phase 3 : timing 25 Hz (Timer 1), input clavier, ship physics, 4 tirs.
 * Phase 4 : asteroids (spawn, draw, update, fragment).
 * Phase 5 : collisions (L∞), score 7-segments, vies, respawn invincible.
 * Phase 6 : UFO grande/petite + IA tir + collisions UFO.
 * Phase 7 : hyperespace (DOWN), game state PLAY/GAMEOVER + restart,
 *           high scores top 5 affiché en game over.
 *
 * État ZP : ship.s. RAM/BSS : bullets, asteroids, hud, ufo, hi-scores.
 */

#include "asteroids.h"
#include "hud.h"
#include "ufo.h"
#include "sound.h"
#include "title.h"
/* hiscores_label_draw exposé via title.h depuis Phase 15 */

/* ------------------------------------------------------------------ */
/* Symboles importés                                                   */
/* ------------------------------------------------------------------ */

extern unsigned char ship_x, ship_y;
extern unsigned char ship_x_frac, ship_y_frac;   /* 8.8 fixed-point partie basse */
extern int           ship_vx, ship_vy;            /* signed 16-bit (8.8 fixed-point) */
extern unsigned char ship_angle;
extern unsigned char key_state;
#pragma zpsym ("ship_x")
#pragma zpsym ("ship_y")
#pragma zpsym ("ship_x_frac")
#pragma zpsym ("ship_y_frac")
#pragma zpsym ("ship_vx")
#pragma zpsym ("ship_vy")
#pragma zpsym ("ship_angle")
#pragma zpsym ("key_state")

extern unsigned char lx0, ly0, lx1, ly1;
#pragma zpsym ("lx0")
#pragma zpsym ("ly0")
#pragma zpsym ("lx1")
#pragma zpsym ("ly1")

extern const signed char ship_thrx[32];
extern const signed char ship_thry[32];

extern const unsigned char shape_radii[3];

void hires_init(void);
void draw_line_xor(void);
void plot_dot(void);    /* Phase 16 — XOR 1 pixel rapide */
void ship_init(void);
void ship_draw(void);
void ship_erase(void);
void ship_rotate(signed char delta);
void key_scan(void);

/* sound.s — tune player pour la mélodie titre (port Mine Storm Vectrex) */
void tune_play_note(unsigned char idx);
void tune_stop(void);

/* ------------------------------------------------------------------ */
/* Constantes                                                          */
/* ------------------------------------------------------------------ */

#define BULLETS         4
#define BULLET_TTL      35
#define FIRE_COOLDOWN   2     /* ~12 tirs/s à 25 Hz */

/* 8.8 fixed-point pour ship_vx/vy (16 bits signed).
 *   - V_MAX_FIXED = 2048 = 8.00 px/frame max (à 25 Hz = 200 px/s).
 *   - Thrust accel : ship_thrx[angle] << 6 (= ×64 ; max ±6*64=384/frame).
 *   - Decay /16 par frame (idem signed shift, conserve l'équilibre arcade). */
#define V_MAX_FIXED     2048
#define THRUST_SHIFT    6

#define WX_MIN          14
#define WX_MAX          226
#define WY_MIN          14
#define WY_MAX          186
#define WX_SPAN         (WX_MAX - WX_MIN)
#define WY_SPAN         (WY_MAX - WY_MIN)

#define SHIP_RADIUS         7
/* Ship mort → invisible pendant DEBRIS_TTL frames (animation explosion),
 * puis clignote pendant SHIP_BLINK_FRAMES (invincibilité). */
#define SHIP_BLINK_FRAMES   20
#define INVINCIBLE_FRAMES   (DEBRIS_TTL + SHIP_BLINK_FRAMES)

/* Phase 7 — hyperespace */
#define HYPER_COOLDOWN      35      /* ~2 s, edge-trigger sur DOWN */
#define HYPER_DEATH_CHANCE  64      /* 64/256 = 25% chance de mort */

/* Phase 7 — high scores */
#define HISCORE_COUNT       5

/* Phase 8 — cadence du thump (frames) — diminue avec moins d'asteroids */
#define THUMP_PERIOD_BASE   30
#define THUMP_PERIOD_MIN    6

/* Phase 10n — cadence UFO bip-bip */
#define UFO_SOUND_PERIOD    16

/* Score asteroid + UFO arcade */
static const unsigned int score_by_size[3] = { 100, 50, 20 };
#define UFO_SCORE_LARGE     200U
#define UFO_SCORE_SMALL     1000U

#define VIA_ACR         (*(volatile unsigned char*)0x030B)
#define VIA_PCR         (*(volatile unsigned char*)0x030C)
#define VIA_IFR         (*(volatile unsigned char*)0x030D)
#define VIA_IER         (*(volatile unsigned char*)0x030E)
#define VIA_T1CL        (*(volatile unsigned char*)0x0304)
#define VIA_T1CH        (*(volatile unsigned char*)0x0305)
#define FRAME_LO        0x3F
#define FRAME_HI        0x9C

/* Phase 9 — synchro VSync ULA via CB1.
 * Sur Oric-1, CB1 est connecté au signal VSync de l'ULA (50 Hz PAL).
 * IFR bit 4 = flag CB1, set sur transition. À 25 Hz = 2 VSync par frame. */
#define VSYNC_FLAG      0x10        /* IFR bit 4 = CB1 */
#define VSYNCS_PER_FRAME 2          /* 50 Hz / 2 = 25 Hz */

/* ------------------------------------------------------------------ */
/* État local                                                          */
/* ------------------------------------------------------------------ */

static unsigned char blt_x[BULLETS];
static unsigned char blt_y[BULLETS];
static signed char   blt_vx[BULLETS];
static signed char   blt_vy[BULLETS];
static unsigned char blt_ttl[BULLETS];

static unsigned char prev_fire;
static unsigned char fire_cd;
static unsigned char prev_hyper;
static unsigned char hyper_cd;

static unsigned char ship_invincible;
static unsigned char ship_was_drawn;

/* Phase 7 — high scores en RAM (persistance .tap reportée Phase 9) */
static unsigned int  hiscores[HISCORE_COUNT];
static unsigned char hiscores_drawn;     /* dernier état affiché du tableau */
static unsigned char gameover_text_drawn; /* Phase 9d : "GAME OVER" affiché */

/* Phase 8 — cadence thump : décrémente sur le timer, déclenche sound_play_fx */
static unsigned char thump_timer;
/* Phase 10n — cadence UFO sound */
static unsigned char ufo_sound_timer;
/* Phase 9f — détecter extra life (lives++) pour déclencher FX_LIFE */
static unsigned char lives_prev;
/* Phase 10d — affichage "WAVE n" en haut-centre */
static unsigned char wave_displayed;
#define WAVE_HUD_Y  16

/* Phase 12 — debris ship/UFO arcade-fidèle (port DoShipExplsn $7465 +
 * ShipExpVelTbl $50EC). 6 fragments avec vélocités prédéfinies fixes
 * (pas RNG comme Phase 10i), durée 50 frames avec disparition
 * SÉQUENTIELLE — les fragments disparaissent un par un dans le temps,
 * conformément au comportement arcade rev 4. */
#define DEBRIS_COUNT      6
#define DEBRIS_TTL        30    /* arcade tronqué : 30/25/20/15/10/5 frames @ 25 Hz */

/* Vélocités exactes ShipExpVelTbl arcade rev 4 ($50EC), nibbles haut
 * sign-extended. Chaque pair = (vx, vy) pour un fragment. */
static const signed char ship_debris_vx[DEBRIS_COUNT] = { -3, +3,  0, +3,  0, -3 };
static const signed char ship_debris_vy[DEBRIS_COUNT] = { +1, -2, -4, +1, +4, -3 };

/* Phase 13 — formes des 6 fragments arcade depuis ShipExpPtrTbl ($50E0).
 * Chaque debris est un mini-segment SVEC avec orientation propre (pas
 * juste un point dans la direction du mouvement comme Phase 10i/12).
 *
 * Source désasm rev 4 :
 *   50e0: SVEC x=-2 y=-3 sc=1 b=12   (debris 0 — pointe haut-gauche)
 *   50e2: SVEC x=+1 y=-2 sc=1 b=12   (debris 1)
 *   50e4: SVEC x=+3 y=+1 sc=0 b=12   (debris 2)
 *   50e6: SVEC x=-1 y=+1 sc=2 b=12   (debris 3)
 *   50e8: SVEC x=-3 y=+1 sc=0 b=12   (debris 4)
 *   50ea: SVEC x=+1 y=-1 sc=1 b=12   (debris 5)
 *
 * Le scale `sc` arcade est ignoré (notre Bresenham n'a pas d'échelle).
 * Les valeurs sont déjà adaptées à notre échelle pixel. */
static const signed char ship_debris_shape_dx[DEBRIS_COUNT] = { -2, +1, +3, -1, -3, +1 };
static const signed char ship_debris_shape_dy[DEBRIS_COUNT] = { -3, -2, +1, +1, +1, -1 };

static unsigned char  dbr_x[DEBRIS_COUNT];
static unsigned char  dbr_y[DEBRIS_COUNT];
static signed char    dbr_vx[DEBRIS_COUNT];
static signed char    dbr_vy[DEBRIS_COUNT];
static unsigned char  dbr_ttl[DEBRIS_COUNT];

/* Phase 14b — pool asteroid explosion : port direct du pattern 1 arcade
 * (SharpPatPtrTbl[0] = $5100, désasm rev 4). 8 dots cumulés depuis les
 * SVEC arcade :
 *
 *   $5100 SVEC x=-1 y=+0    cumul (-1, 0)
 *   $5104 SVEC x=-1 y=-1    cumul (-2, -1)
 *   $5108 SVEC x=+1 y=-1    cumul (-1, -2)
 *   $510C SVEC x=+3 y=+1    cumul (+2, -1)
 *   $5110 SVEC x=+2 y=-1    cumul (+4, -2)
 *   $5114 SVEC x=+0 y=+1    cumul (+4, -1)
 *   $5118 SVEC x=+1 y=+3    cumul (+5, +2)
 *   $511C SVEC x=-1 y=+3    cumul (+4, +5)
 *   ($5120 VCTR sc=5 ignoré — trop large pour notre échelle Oric)
 *
 * 8 dots fixes (rayon de 1 à 5 px du centre), durée 10 frames. */
#define ADBR_COUNT  8
#define ADBR_TTL    5     /* ~200 ms à 25 Hz — flash bref, pas un nuage qui traîne */
static unsigned char  adbr_x[ADBR_COUNT];
static unsigned char  adbr_y[ADBR_COUNT];
static unsigned char  adbr_ttl[ADBR_COUNT];
static const signed char adbr_dx[ADBR_COUNT] = { -1, -2, -1, +2, +4, +4, +5, +4 };
static const signed char adbr_dy[ADBR_COUNT] = {  0, -1, -2, -1, -2, -1, +2, +5 };

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void plot(unsigned char x, unsigned char y)
{
    /* Phase 16 : plot_dot (40 c) au lieu de draw_line_xor (~80 c) */
    lx0 = x; ly0 = y;
    plot_dot();
}

static unsigned char abs_diff(unsigned char a, unsigned char b)
{
    return (a > b) ? (a - b) : (b - a);
}

static unsigned char collide(unsigned char x1, unsigned char y1,
                             unsigned char x2, unsigned char y2,
                             unsigned char r)
{
    if (abs_diff(x1, x2) > r) return 0;
    if (abs_diff(y1, y2) > r) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Timer / frame                                                       */
/* ------------------------------------------------------------------ */

static void timer_init(void)
{
    /* Phase 9 : VSync ULA via CB1 (preferé, anti-tearing).
     * On laisse PCR tel que la ROM le configure (CB1 = falling edge,
     * géré par la ROM IRQ). Désactivons l'IRQ CB1 pour pouvoir poll
     * IFR sans rejet. */
    VIA_ACR = VIA_ACR & 0x3F;       /* Timer 1 one-shot (au cas où fallback) */
    VIA_IER = 0x40;                  /* Disable T1 IRQ */
    VIA_IFR = 0x50;                  /* Clear T1 + CB1 flags initialement */
}

static void frame_wait(void)
{
    /* Attendre VSYNCS_PER_FRAME transitions CB1 (= 2 VSync = 25 Hz).
     * Le flag CB1 est partagé avec la ROM IRQ : la ROM le clear dans
     * son handler. Pour un polling fiable, on clear nous-mêmes après
     * chaque attente. */
    unsigned char i;
    for (i = 0; i < VSYNCS_PER_FRAME; i++) {
        while (!(VIA_IFR & VSYNC_FLAG)) { }
        VIA_IFR = VSYNC_FLAG;        /* clear par écriture du bit */
    }
}

/* ------------------------------------------------------------------ */
/* Ship physics + hyperespace                                          */
/* ------------------------------------------------------------------ */

static void ship_update(void)
{
    int d;
    unsigned int pos16;

    /* Decay (frottement) — 1/16 par frame en 8.8. */
    d = ship_vx >> 4;  ship_vx -= d;
    d = ship_vy >> 4;  ship_vy -= d;

    /* Clamp à la vitesse max (en 8.8 fixed-point). */
    if (ship_vx >  V_MAX_FIXED) ship_vx =  V_MAX_FIXED;
    if (ship_vx < -V_MAX_FIXED) ship_vx = -V_MAX_FIXED;
    if (ship_vy >  V_MAX_FIXED) ship_vy =  V_MAX_FIXED;
    if (ship_vy < -V_MAX_FIXED) ship_vy = -V_MAX_FIXED;

    /* Position 8.8 : recompose (entier, frac) en 16 bits, ajoute vélocité
     * signed (cast modulaire unsigned). Le wrap est testé selon le signe
     * de la vélocité (sinon arithmétique modulo 65536 mélange overflow
     * et sous-flow → wrap au milieu d'écran au lieu du côté opposé).
     * Note : la zone jouable ship [WX_MIN..WX_MAX] est plus étroite que
     * l'écran complet, donc on n'utilise pas la même constante (X_SPAN_FIX)
     * que les asteroids. */
    {
        unsigned int x_span_fix = (unsigned int)WX_SPAN << 8;
        pos16 = (((unsigned int)(ship_x - WX_MIN)) << 8) | ship_x_frac;
        pos16 += (unsigned int)ship_vx;
        if (ship_vx >= 0) {
            if (pos16 >= x_span_fix) pos16 -= x_span_fix;
        } else {
            if (pos16 >= x_span_fix) pos16 += x_span_fix;
        }
        ship_x      = (unsigned char)((pos16 >> 8) + WX_MIN);
        ship_x_frac = (unsigned char)(pos16 & 0xFF);
    }
    {
        unsigned int y_span_fix = (unsigned int)WY_SPAN << 8;
        pos16 = (((unsigned int)(ship_y - WY_MIN)) << 8) | ship_y_frac;
        pos16 += (unsigned int)ship_vy;
        if (ship_vy >= 0) {
            if (pos16 >= y_span_fix) pos16 -= y_span_fix;
        } else {
            if (pos16 >= y_span_fix) pos16 += y_span_fix;
        }
        ship_y      = (unsigned char)((pos16 >> 8) + WY_MIN);
        ship_y_frac = (unsigned char)(pos16 & 0xFF);
    }
}

static void ship_apply_thrust(void)
{
    /* Accel = ship_thrx[angle] × 64 (en 8.8 = 0.25 px/frame par unit). */
    ship_vx += ((int)ship_thrx[ship_angle]) << THRUST_SHIFT;
    ship_vy += ((int)ship_thry[ship_angle]) << THRUST_SHIFT;
}

static void ship_respawn(void)
{
    ship_x = 120;
    ship_y = 100;
    ship_x_frac = 0;
    ship_y_frac = 0;
    ship_vx = 0;
    ship_vy = 0;
    ship_angle = 0;
    ship_invincible = INVINCIBLE_FRAMES;
}

/* Phase 12 — Spawn de 6 fragments arcade-fidèles à la position passée.
 * Vélocités prédéfinies depuis ShipExpVelTbl arcade ; disparition
 * séquentielle via TTL décroissant (-6 par fragment) → fragment 0
 * disparaît à 50 frames, fragment 5 à 20 frames. Effet "explosion
 * qui se dissout en éclats successifs" reconnaissable de l'arcade. */
static void debris_spawn(unsigned char ax, unsigned char ay)
{
    unsigned char i;
    for (i = 0; i < DEBRIS_COUNT; i++) {
        dbr_x[i]   = ax;
        dbr_y[i]   = ay;
        dbr_vx[i]  = ship_debris_vx[i];
        dbr_vy[i]  = ship_debris_vy[i];
        dbr_ttl[i] = DEBRIS_TTL - i * 5;      /* 30/25/20/15/10/5 séquentielle */
    }
}

static void debris_update(void)
{
    unsigned char i;
    int nx, ny;
    for (i = 0; i < DEBRIS_COUNT; i++) {
        if (dbr_ttl[i] == 0) continue;
        nx = (int)dbr_x[i] + (int)dbr_vx[i];
        ny = (int)dbr_y[i] + (int)dbr_vy[i];
        if (nx < 1 || nx > 238 || ny < 1 || ny > 198) {
            dbr_ttl[i] = 0;
            continue;
        }
        dbr_x[i] = (unsigned char)nx;
        dbr_y[i] = (unsigned char)ny;
        dbr_ttl[i]--;
    }
}

/* Tracé des fragments en XOR — Phase 13 : chaque debris a sa forme
 * propre (mini-segment SVEC arcade), pas juste un point dans la
 * direction du mouvement. La trajectoire (vx, vy) déplace le segment
 * mais sa silhouette reste fixe (= un morceau du ship arcade). */
static void debris_render(void)
{
    unsigned char i;
    for (i = 0; i < DEBRIS_COUNT; i++) {
        if (dbr_ttl[i] == 0) continue;
        lx0 = dbr_x[i];
        ly0 = dbr_y[i];
        lx1 = (unsigned char)((signed char)dbr_x[i] + ship_debris_shape_dx[i]);
        ly1 = (unsigned char)((signed char)dbr_y[i] + ship_debris_shape_dy[i]);
        draw_line_xor();
    }
}

static void debris_init(void)
{
    unsigned char i;
    for (i = 0; i < DEBRIS_COUNT; i++) dbr_ttl[i] = 0;
    for (i = 0; i < ADBR_COUNT; i++) adbr_ttl[i] = 0;
}

/* Phase 14 — spawn explosion dots autour d'un point (asteroid détruit). */
static void asteroid_debris_spawn(unsigned char ax, unsigned char ay)
{
    unsigned char i;
    for (i = 0; i < ADBR_COUNT; i++) {
        adbr_x[i]   = ax;
        adbr_y[i]   = ay;
        adbr_ttl[i] = ADBR_TTL;
    }
}

/* Render des 8 dots en étoile (XOR — appel pair = effacement).
 * Phase 16 : plot_dot rapide (40 c vs 80 c) — gain 320 c quand actif. */
static void asteroid_debris_render(void)
{
    unsigned char i;
    int x, y;
    for (i = 0; i < ADBR_COUNT; i++) {
        if (adbr_ttl[i] == 0) continue;
        x = (int)(unsigned int)adbr_x[i] + (int)adbr_dx[i];
        y = (int)(unsigned int)adbr_y[i] + (int)adbr_dy[i];
        if (x < 0 || x > 239 || y < 0 || y > 199) continue;
        lx0 = (unsigned char)x;
        ly0 = (unsigned char)y;
        plot_dot();
    }
}

/* Décrémenter TTL (statique, pas de mouvement). */
static void asteroid_debris_update(void)
{
    unsigned char i;
    for (i = 0; i < ADBR_COUNT; i++) {
        if (adbr_ttl[i]) adbr_ttl[i]--;
    }
}

/* Hyperespace : téléportation aléatoire, 25% chance de mort.
 * Edge-trigger sur DOWN + cooldown HYPER_COOLDOWN frames. */
static void ship_hyperspace(void)
{
    sound_play_fx(FX_HYPER);
    if (rng8() < HYPER_DEATH_CHANCE) {
        hud_lose_life();
        ship_respawn();
        return;
    }
    /* Survie : téléportation aléatoire dans la zone safe */
    ship_x = WX_MIN + (rng8() % WX_SPAN);
    ship_y = WY_MIN + (rng8() % WY_SPAN);
    ship_x_frac = 0;
    ship_y_frac = 0;
    ship_vx = 0;
    ship_vy = 0;
    ship_invincible = SHIP_BLINK_FRAMES;     /* clignotement court — pas d'animation explosion ici */
}

/* ------------------------------------------------------------------ */
/* Bullets                                                            */
/* ------------------------------------------------------------------ */

static void bullets_init(void)
{
    unsigned char i;
    for (i = 0; i < BULLETS; i++) blt_ttl[i] = 0;
    prev_fire = 0;
    fire_cd = 0;
    prev_hyper = 0;
    hyper_cd = 0;
}

static void bullet_fire(void)
{
    unsigned char i;
    if (fire_cd) return;
    for (i = 0; i < BULLETS; i++) {
        if (blt_ttl[i] == 0) {
            blt_x[i]   = ship_x;
            blt_y[i]   = ship_y;
            /* Vitesse bullet = 2× le thrust ship (~12 px/frame max au
             * lieu de 6) — proche du ratio arcade Atari. */
            blt_vx[i]  = (signed char)(ship_thrx[ship_angle] << 1);
            blt_vy[i]  = (signed char)(ship_thry[ship_angle] << 1);
            blt_ttl[i] = BULLET_TTL;
            fire_cd    = FIRE_COOLDOWN;
            sound_play_fx(FX_FIRE);
            return;
        }
    }
}

static void bullets_update(void)
{
    unsigned char i;
    int nx, ny;
    if (fire_cd) fire_cd--;
    for (i = 0; i < BULLETS; i++) {
        if (blt_ttl[i] == 0) continue;
        nx = (int)blt_x[i] + (int)blt_vx[i];
        ny = (int)blt_y[i] + (int)blt_vy[i];
        if (nx < 1 || nx > 238 || ny < 1 || ny > 198) {
            blt_ttl[i] = 0;
            continue;
        }
        blt_x[i] = (unsigned char)nx;
        blt_y[i] = (unsigned char)ny;
        blt_ttl[i]--;
    }
}

static void bullets_render(void)
{
    unsigned char i;
    for (i = 0; i < BULLETS; i++) {
        if (blt_ttl[i] == 0) continue;
        /* Torpille = 2 pixels adjacents (mini-trait horizontal) pour
         * être lisible à 240×200. Un seul pixel était quasi invisible
         * surtout en mouvement. blt_x reste dans [1..238] (cf. clip
         * bullets_update), donc blt_x+1 ≤ 239, pas d'overflow. */
        plot(blt_x[i],          blt_y[i]);
        plot(blt_x[i] + 1,      blt_y[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Collisions                                                         */
/* ------------------------------------------------------------------ */

static void collisions_bullets_asteroids(void)
{
    unsigned char b, a, r;
    for (b = 0; b < BULLETS; b++) {
        if (blt_ttl[b] == 0) continue;
        if (ufo_active) {
            r = ufo_radius() + 1;
            if (collide(blt_x[b], blt_y[b], ufo_x, ufo_y, r)) {
                hud_add_score((ufo_type == UFO_LARGE) ? UFO_SCORE_LARGE
                                                       : UFO_SCORE_SMALL);
                /* Phase 12 : debris UFO arcade-fidèle à sa position */
                debris_spawn(ufo_x, ufo_y);
                sound_play_fx(FX_EXPLODE);
                ufo_kill();
                blt_ttl[b] = 0;
                continue;
            }
        }
        for (a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;
            r = shape_radii[asteroids[a].size] + 1;
            if (collide(blt_x[b], blt_y[b], asteroids[a].x, asteroids[a].y, r)) {
                hud_add_score(score_by_size[asteroids[a].size]);
                /* Phase 14 : flash explosion à la position du hit */
                asteroid_debris_spawn(asteroids[a].x, asteroids[a].y);
                asteroids_fragment(a);
                sound_play_fx(FX_EXPLODE);
                blt_ttl[b] = 0;
                break;
            }
        }
    }
}

static unsigned char collisions_ship_asteroids(void)
{
    unsigned char a, r;
    if (ship_invincible) return 0;
    for (a = 0; a < MAX_ASTEROIDS; a++) {
        if (!asteroids[a].active) continue;
        r = shape_radii[asteroids[a].size] + SHIP_RADIUS;
        if (collide(ship_x, ship_y, asteroids[a].x, asteroids[a].y, r)) {
            hud_lose_life();
            debris_spawn(ship_x, ship_y);
            asteroid_debris_spawn(asteroids[a].x, asteroids[a].y);
            ship_respawn();
            sound_play_fx(FX_EXPLODE);
            return 1;
        }
    }
    if (ufo_active) {
        r = ufo_radius() + SHIP_RADIUS;
        if (collide(ship_x, ship_y, ufo_x, ufo_y, r)) {
            hud_lose_life();
            debris_spawn(ship_x, ship_y);
            /* Phase 12 : debris UFO aussi (collision mutuelle) */
            debris_spawn(ufo_x, ufo_y);
            sound_play_fx(FX_EXPLODE);
            ship_respawn();
            ufo_kill();
            return 1;
        }
    }
    if (ufo_bullet_active) {
        if (collide(ship_x, ship_y, ufo_bullet_x, ufo_bullet_y, SHIP_RADIUS)) {
            hud_lose_life();
            debris_spawn(ship_x, ship_y);
            sound_play_fx(FX_EXPLODE);
            ship_respawn();
            ufo_bullet_active = 0;
            return 1;
        }
    }
    return 0;
}

static void collisions_ufobullet_asteroids(void)
{
    unsigned char a, r;
    if (!ufo_bullet_active) return;
    for (a = 0; a < MAX_ASTEROIDS; a++) {
        if (!asteroids[a].active) continue;
        r = shape_radii[asteroids[a].size] + 1;
        if (collide(ufo_bullet_x, ufo_bullet_y,
                    asteroids[a].x, asteroids[a].y, r)) {
            asteroids_fragment(a);
            ufo_bullet_active = 0;
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Vagues                                                             */
/* ------------------------------------------------------------------ */

static void check_next_wave(void)
{
    if (asteroids_count() == 0) {
        asteroids_spawn_wave();
    }
}

/* ------------------------------------------------------------------ */
/* High scores (Phase 7)                                              */
/* ------------------------------------------------------------------ */

static void hiscores_init(void)
{
    unsigned char i;
    /* Valeurs par défaut décroissantes */
    hiscores[0] = 1000;
    hiscores[1] = 500;
    hiscores[2] = 200;
    hiscores[3] = 100;
    hiscores[4] = 50;
    hiscores_drawn = 0;
    for (i = 5; i < HISCORE_COUNT; i++) hiscores[i] = 0;
}

/* Insère final_score dans la table triée si éligible.
 * Retourne la position d'insertion (0..HISCORE_COUNT-1) ou 0xFF
 * si le score n'entre pas dans le top. La position sert à déclencher
 * la saisie du pseudo uniquement en cas de nouveau high score. */
static unsigned char hiscores_insert(unsigned int final_score)
{
    unsigned char i, j;
    for (i = 0; i < HISCORE_COUNT; i++) {
        if (final_score > hiscores[i]) {
            /* Décaler vers le bas et insérer */
            for (j = HISCORE_COUNT - 1; j > i; j--) {
                hiscores[j] = hiscores[j - 1];
            }
            hiscores[i] = final_score;
            return i;
        }
    }
    return 0xFF;     /* score insuffisant pour entrer dans le top */
}

/* Dessine la table des high scores en game over (centre écran).
 * Phase 9 : utilise hud_xor_5digits (rendu 7-seg propre).
 * Phase 15 : ajout label "HIGH SCORES" au-dessus à y=55. */
#define HISCORES_LABEL_Y  55
static void hiscores_draw_table(void)
{
    unsigned char i, py;
    hiscores_label_draw(HISCORES_LABEL_Y);
    for (i = 0; i < HISCORE_COUNT; i++) {
        py = 75 + i * 14;
        hud_xor_5digits(hiscores[i], 105, py);
    }
}

/* ------------------------------------------------------------------ */
/* Réinitialisation pour restart                                       */
/* ------------------------------------------------------------------ */

static void game_reset(void)
{
    /* Effacer tous les objets actuels (XOR état ⇒ nettoyage écran) */
    if (ship_was_drawn) ship_erase();
    bullets_render();           /* efface les tirs encore actifs */
    asteroids_draw();           /* efface les asteroids actifs */
    ufo_draw();
    ufo_bullet_draw();

    /* Réinit complète */
    ship_init();
    bullets_init();
    asteroids_init(0x42);
    asteroids_spawn_wave();
    ufo_init();
    hud_init();
    ship_was_drawn = 0;
    ship_invincible = INVINCIBLE_FRAMES;

    /* Première frame visible */
    ship_draw();
    ship_was_drawn = 1;
    asteroids_draw();
    hud_draw();
}

/* ------------------------------------------------------------------ */
/* Boucle principale                                                  */
/* ------------------------------------------------------------------ */

void game_run(void)
{
    unsigned char fire_now, hyper_now;
    unsigned char ship_visible;
    unsigned char prev_gameover;
    unsigned int  final_score;
    unsigned char new_hiscore_pos = 0xFF;    /* 0..5 = nouveau hi-score, 0xFF = non */

    /* Init explicite — crt0 ne clear pas BSS (workaround Phase 6) */
    ship_invincible = 0;
    ship_was_drawn  = 0;
    prev_gameover   = 0;
    final_score     = 0;

    hires_init();
    timer_init();
    sound_init();

    /* Phase 9c/9e/10e/10m — écran titre :
     *   - "ASTEROIDS" statique
     *   - "PRESS SPACE" qui clignote toutes les ~1.5 s (effet arcade pulse)
     *   - démo passive (asteroids animés en arrière-plan).
     * Au démarrage du jeu, on garde l'état asteroids en place. */
    title_draw();
    presspace_draw(110);
    asteroids_init(0x42);
    asteroids_spawn_wave();
    asteroids_draw();
    {
        unsigned char i;
        unsigned char prev_space = 0;
        unsigned char ps_visible = 1;     /* PRESS SPACE actuellement affiché */
        /* Option C : aucune musique en écran titre — silence, comme
         * l'arcade Atari Asteroids originale (le thump cadencé commence
         * uniquement quand le jeu démarre). Le timeout sans appui est
         * réduit à 32 frames (~2 s à 17 Hz) : pour démarrer rapidement
         * il suffit d'appuyer sur SPACE, le double-scan le capte. */
        for (i = 0; i < 32; i++) {
            key_scan();
            if ((key_state & 0x08) && !prev_space) break;
            prev_space = key_state & 0x08;
            asteroids_update();
            asteroids_render();   /* erase à prev + draw à curr per-entity */
            key_scan();
            if ((key_state & 0x08) && !prev_space) break;
            prev_space = key_state & 0x08;
            /* Toggle PRESS SPACE tous les 8 frames (~0.5 s à 17 Hz). */
            if ((i & 0x07) == 0x07) {
                if (ps_visible) {
                    presspace_erase(110);
                    ps_visible = 0;
                } else {
                    presspace_draw(110);
                    ps_visible = 1;
                }
            }
            frame_wait();
        }
        /* Garantir l'état "effacé" en sortie (XOR cohérence) */
        if (ps_visible) presspace_erase(110);
    }
    title_erase();
    /* Phase 10e — current_wave passe à 1 dans asteroids_spawn_wave ci-dessus.
     * Reset à 0 pour que le test wave_displayed != current_wave déclenche
     * l'affichage initial de "WAVE 1" dans la boucle de jeu. */

    ship_init();
    bullets_init();
    /* asteroids_init et spawn déjà faits ; on conserve l'état (démo →
     * jeu sans ré-init pour transition fluide). */
    ufo_init();
    hud_init();
    hiscores_init();
    debris_init();
    thump_timer = THUMP_PERIOD_BASE;
    ufo_sound_timer = 0;
    lives_prev = lives;
    wave_displayed = 0;

    ship_draw();
    ship_was_drawn = 1;
    asteroids_draw();
    hud_draw();

    for (;;) {
        /* Lock 2 s après passage en gameover : GAME OVER + HoF fixes,
         * pas de touches acceptées et pas de prompt affiché pour laisser
         * l'utilisateur lire son score. */
        static unsigned char gameover_lock = 0;
        static unsigned char prompt_drawn = 0;   /* PRESS SPACE + OR ESC */
        key_scan();

        if (gameover && !prev_gameover) {
            gameover_lock = 50;        /* 2 s à 25 Hz */
        }

        if (gameover && gameover_lock) {
            /* Lock actif : ignorer SPACE/ESC. */
        } else if (gameover) {
            if (key_state & 0x08) {
                /* SPACE → rejouer. */
                if (prompt_drawn) {
                    presspace_erase(140);
                    quit_label_erase(155);
                    prompt_drawn = 0;
                }
                if (gameover_text_drawn) {
                    gameover_erase();
                    gameover_text_drawn = 0;
                }
                if (hiscores_drawn) {
                    hiscores_draw_table();   /* XOR : efface */
                    hiscores_drawn = 0;
                }
                game_reset();
                prev_gameover = 0;
                continue;
            }
            if (key_state & 0x20) {
                /* ESC → quitter (crt0 JMP $F800 → BASIC). L'écran sera
                 * réinitialisé en mode TEXT par la ROM. */
                return;
            }
        }

        /* ============================================================
         * BLOC ERASE → LOGIQUE → DRAW (fenêtre flicker compactée)
         *
         * Tout ce qui ne touche pas au tracé d'entité mobile (sound,
         * scoring, hi-scores, HUD statique, wave label) est sorti de
         * cette fenêtre — cf. POST-RENDER plus bas. Cela réduit ~50 %
         * le temps pendant lequel un astéroïde est éteint à l'écran.
         * ============================================================ */

        /* 1. Erase entités mobiles SAUF ship et asteroids (à pos N-1).
         *    Ship a son propre bloc compact (plus bas) pour minimiser
         *    son flicker (très visible vs astéroïdes). Asteroids idem
         *    avec leur bloc per-entity. */
        asteroid_debris_render();
        debris_render();
        ufo_bullet_draw();
        ufo_draw();
        bullets_render();

        /* 2. Bullets : input fire (edge-trigger) puis update.
         *    Hyperespace edge-trigger DOWN. */
        if (!gameover) {
            fire_now = key_state & 0x08;
            if (fire_now && !prev_fire) bullet_fire();
            prev_fire = fire_now;

            hyper_now = key_state & 0x10;
            if (hyper_now && !prev_hyper && hyper_cd == 0) {
                ship_hyperspace();
                hyper_cd = HYPER_COOLDOWN;
            }
            prev_hyper = hyper_now;
            if (hyper_cd) hyper_cd--;
        }

        /* 3. Updates physiques NON-asteroids NON-ship */
        bullets_update();
        if (!gameover) ufo_tick(ship_x, ship_y, score);
        ufo_bullet_update();
        debris_update();
        asteroid_debris_update();

        /* 4. ===== ASTEROIDS — étape B per-entity =====
         * asteroids_render fait erase à prev puis draw à curr consécutivement
         * par entité. Collisions ship-asteroids déplacées dans le bloc ship
         * ci-dessous (utilisent ship_x/y de la frame N-1, décalage 1 frame
         * imperceptible à 25 Hz). */
        asteroids_update();
        collisions_bullets_asteroids();
        collisions_ufobullet_asteroids();
        asteroids_render();
        /* ===== FIN BLOC ASTEROIDS ===== */

        /* 5. ===== SHIP — bloc compact erase → input → update → draw =====
         * Fenêtre pendant laquelle le ship est absent de l'écran réduite
         * à ~10 lignes de code (input apply + ship_update +
         * collisions_ship_asteroids + bookkeeping) au lieu des ~50 du
         * refactor précédent. */
        if (ship_was_drawn) ship_erase();    /* erase à pos/angle N-1 */

        if (!gameover) {
            /* Rotation : 2 angles/frame. Avec MAX_ASTEROIDS=6 + shapes
             * décimées, framerate effectif ~10 Hz → 20 angles/s = tour
             * en 1.6 s. Pas de 22.5° par tick → moins saccadé qu'à 4. */
            if (key_state & 0x01) ship_rotate((signed char)-2);
            if (key_state & 0x02) ship_rotate((signed char)+2);
            if (key_state & 0x04) {
                ship_apply_thrust();
                if (sfx_id == FX_NONE) sound_play_fx(FX_THRUST);
            }
            ship_update();
            collisions_ship_asteroids();
        }

        if (ship_invincible) ship_invincible--;
        check_next_wave();

        /* Tant que ship_invincible > SHIP_BLINK_FRAMES, l'animation des
         * débris est encore en cours → ship invisible. En dessous, le
         * ship clignote (bit 1 du compteur) pendant SHIP_BLINK_FRAMES
         * frames. À 0, ship pleinement visible. */
        ship_visible = !gameover &&
                       (ship_invincible <= SHIP_BLINK_FRAMES) &&
                       (ship_invincible == 0 || (ship_invincible & 2));

        if (ship_visible) {
            ship_draw();                     /* draw à pos/angle N */
            ship_was_drawn = 1;
        } else {
            ship_was_drawn = 0;
        }
        /* ===== FIN BLOC SHIP ===== */

        /* 6. Draw entités mobiles restantes à pos N */
        bullets_render();
        ufo_draw();
        ufo_bullet_draw();
        debris_render();
        asteroid_debris_render();

        /* ============================================================
         * POST-RENDER (hors fenêtre flicker)
         * Tout ce qui suit n'affecte aucune entité mobile : sound,
         * scoring, HUD statique, wave label, textes game over.
         * Le balayage CRT a déjà rencontré les entités mobiles avec
         * leur état correct — d'éventuels glitches ici n'affectent
         * que des zones statiques (pas de flicker perceptible).
         * ============================================================ */

        /* Extra life (FX_LIFE) */
        if (lives > lives_prev && sfx_id == FX_NONE) {
            sound_play_fx(FX_LIFE);
        }
        lives_prev = lives;

        /* Sound thump / UFO bip-bip cadencé */
        if (!gameover && sfx_id == FX_NONE) {
            if (ufo_active) {
                if (ufo_sound_timer == 0) {
                    sound_play_fx(FX_UFO);
                    ufo_sound_timer = UFO_SOUND_PERIOD;
                } else {
                    ufo_sound_timer--;
                }
            } else if (thump_timer == 0) {
                unsigned char n = asteroids_count();
                unsigned char period;
                if (n >= 8)      period = THUMP_PERIOD_BASE;
                else if (n >= 4) period = THUMP_PERIOD_BASE - 8;
                else if (n >= 2) period = THUMP_PERIOD_BASE - 16;
                else             period = THUMP_PERIOD_MIN;
                sound_play_fx(FX_THUMP);
                thump_timer = period;
            } else {
                thump_timer--;
            }
        }
        sound_tick();

        /* Passage en game over → insertion hi-scores + clean UFO.
         * hiscore_pos enregistre le rang d'insertion (0..5) ou 0xFF si
         * le score n'entre pas dans le top — utilisé par la prochaine
         * étape pour décider d'afficher l'écran saisie pseudo. */
        if (gameover && !prev_gameover) {
            final_score = score;
            new_hiscore_pos = hiscores_insert(final_score);
            ufo_kill();
        }
        prev_gameover = gameover;

        /* Décrémenter le timer de lock game over (~2 s sans accepter
         * SPACE/ESC pour laisser le joueur lire son score + HoF). */
        if (gameover_lock) gameover_lock--;

        /* Wave label — erase puis redraw consécutifs si vague changée */
        if (current_wave != wave_displayed) {
            if (wave_displayed != 0) wave_label_erase(WAVE_HUD_Y, wave_displayed);
            wave_label_draw(WAVE_HUD_Y, current_wave);
            wave_displayed = current_wave;
        }

        hud_draw();

        /* Game over text : HoF + "GAME OVER" tracés dès le début ;
         * "PRESS SPACE / OR ESC TO STOP" seulement après le lock 2 s. */
        if (gameover) {
            if (hiscores_drawn) hiscores_draw_table();
            if (gameover_text_drawn) gameover_erase();
            hiscores_draw_table();
            hiscores_drawn = 1;
            gameover_draw();
            gameover_text_drawn = 1;

            if (gameover_lock == 0) {
                if (prompt_drawn) {
                    presspace_erase(140);
                    quit_label_erase(155);
                }
                presspace_draw(140);
                quit_label_draw(155);
                prompt_drawn = 1;
            }
        }

        frame_wait();
    }
}
