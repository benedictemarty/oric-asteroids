/*
 * game.c — Boucle de jeu Phases 3 → 5
 *
 * Phase 3 : timing 25 Hz (Timer 1), input clavier, ship physics, 4 tirs.
 * Phase 4 : asteroids (spawn, draw, update, fragment).
 * Phase 5 : collisions cercle-cercle (L∞), score 7-segments, vies,
 *           respawn vaisseau invincible, vague suivante, game over.
 *
 * État ZP : ship.s. RAM/BSS : bullets, asteroids, hud (score/lives).
 */

#include "asteroids.h"
#include "hud.h"

/* ------------------------------------------------------------------ */
/* Symboles importés depuis l'asm                                      */
/* ------------------------------------------------------------------ */

extern unsigned char ship_x, ship_y;
extern signed char   ship_vx, ship_vy;
extern unsigned char ship_angle;
extern unsigned char key_state;
#pragma zpsym ("ship_x")
#pragma zpsym ("ship_y")
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
void ship_init(void);
void ship_draw(void);
void ship_erase(void);
void ship_rotate(signed char delta);
void key_scan(void);

/* ------------------------------------------------------------------ */
/* Constantes                                                          */
/* ------------------------------------------------------------------ */

#define BULLETS         4
#define BULLET_TTL      35
#define FIRE_COOLDOWN   4
#define V_MAX           16

#define WX_MIN          14
#define WX_MAX          226
#define WY_MIN          14
#define WY_MAX          186
#define WX_SPAN         (WX_MAX - WX_MIN)
#define WY_SPAN         (WY_MAX - WY_MIN)

/* Rayon collision approximatif du vaisseau (Phase 5) */
#define SHIP_RADIUS     7
/* Invincibilité après respawn (~1.6 s à 25 Hz) */
#define INVINCIBLE_FRAMES   40

/* Score arcade Atari : grand = 20, moyen = 50, petit = 100 */
static const unsigned int score_by_size[3] = { 100, 50, 20 };

#define VIA_ACR         (*(volatile unsigned char*)0x030B)
#define VIA_IFR         (*(volatile unsigned char*)0x030D)
#define VIA_IER         (*(volatile unsigned char*)0x030E)
#define VIA_T1CL        (*(volatile unsigned char*)0x0304)
#define VIA_T1CH        (*(volatile unsigned char*)0x0305)

#define FRAME_LO        0x3F
#define FRAME_HI        0x9C

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

static unsigned char ship_invincible;       /* compteur frames invincibilité */
static unsigned char ship_was_drawn;        /* dernière frame : XOR set ou non */

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void plot(unsigned char x, unsigned char y)
{
    lx0 = x; ly0 = y; lx1 = x; ly1 = y;
    draw_line_xor();
}

static unsigned char abs_diff(unsigned char a, unsigned char b)
{
    return (a > b) ? (a - b) : (b - a);
}

/* Collision distance L∞ (max des |∆x|, |∆y|) — rapide, légèrement plus
 * permissive que la vraie distance euclidienne, suffisant pour Phase 5. */
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
    VIA_ACR = VIA_ACR & 0x3F;
    VIA_IER = 0x40;
    VIA_IFR = 0x40;
}

static void frame_wait(void)
{
    VIA_T1CL = FRAME_LO;
    VIA_T1CH = FRAME_HI;
    while (!(VIA_IFR & 0x40)) { }
    (void)VIA_T1CL;
}

/* ------------------------------------------------------------------ */
/* Ship physics                                                        */
/* ------------------------------------------------------------------ */

static void ship_update(void)
{
    signed char d;
    int nx, ny;

    d = ship_vx >> 4;  ship_vx -= d;
    d = ship_vy >> 4;  ship_vy -= d;
    if (ship_vx >  V_MAX) ship_vx =  V_MAX;
    if (ship_vx < -V_MAX) ship_vx = -V_MAX;
    if (ship_vy >  V_MAX) ship_vy =  V_MAX;
    if (ship_vy < -V_MAX) ship_vy = -V_MAX;

    nx = (int)ship_x + (int)ship_vx;
    ny = (int)ship_y + (int)ship_vy;
    if (nx < WX_MIN) nx += WX_SPAN;
    if (nx > WX_MAX) nx -= WX_SPAN;
    if (ny < WY_MIN) ny += WY_SPAN;
    if (ny > WY_MAX) ny -= WY_SPAN;
    ship_x = (unsigned char)nx;
    ship_y = (unsigned char)ny;
}

static void ship_apply_thrust(void)
{
    ship_vx += ship_thrx[ship_angle];
    ship_vy += ship_thry[ship_angle];
}

static void ship_respawn(void)
{
    ship_x = 120;
    ship_y = 100;
    ship_vx = 0;
    ship_vy = 0;
    ship_angle = 0;
    ship_invincible = INVINCIBLE_FRAMES;
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
}

static void bullet_fire(void)
{
    unsigned char i;
    if (fire_cd) return;
    for (i = 0; i < BULLETS; i++) {
        if (blt_ttl[i] == 0) {
            blt_x[i]   = ship_x;
            blt_y[i]   = ship_y;
            blt_vx[i]  = ship_thrx[ship_angle];
            blt_vy[i]  = ship_thry[ship_angle];
            blt_ttl[i] = BULLET_TTL;
            fire_cd    = FIRE_COOLDOWN;
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
        plot(blt_x[i], blt_y[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Collisions                                                         */
/* ------------------------------------------------------------------ */

/* Pour chaque tir actif, teste contre chaque asteroid. Sur collision :
 *   - tir détruit (TTL = 0)
 *   - asteroid fragmenté + score crédité selon sa taille
 *   - on stoppe le test sur ce tir (un tir = un impact max) */
static void collisions_bullets_asteroids(void)
{
    unsigned char b, a, r;
    for (b = 0; b < BULLETS; b++) {
        if (blt_ttl[b] == 0) continue;
        for (a = 0; a < MAX_ASTEROIDS; a++) {
            if (!asteroids[a].active) continue;
            r = shape_radii[asteroids[a].size] + 1;     /* +1 = rayon tir */
            if (collide(blt_x[b], blt_y[b], asteroids[a].x, asteroids[a].y, r)) {
                hud_add_score(score_by_size[asteroids[a].size]);
                asteroids_fragment(a);
                blt_ttl[b] = 0;
                break;
            }
        }
    }
}

/* Si vaisseau touché et non invincible : respawn + perte d'une vie */
static unsigned char collisions_ship_asteroids(void)
{
    unsigned char a, r;
    if (ship_invincible) return 0;
    for (a = 0; a < MAX_ASTEROIDS; a++) {
        if (!asteroids[a].active) continue;
        r = shape_radii[asteroids[a].size] + SHIP_RADIUS;
        if (collide(ship_x, ship_y, asteroids[a].x, asteroids[a].y, r)) {
            hud_lose_life();
            ship_respawn();
            return 1;
        }
    }
    return 0;
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
/* Boucle principale                                                  */
/* ------------------------------------------------------------------ */

void game_run(void)
{
    unsigned char fire_now;
    unsigned char ship_visible;

    hires_init();
    timer_init();
    ship_init();
    bullets_init();
    asteroids_init(0x42);
    asteroids_spawn_wave();
    hud_init();

    /* Première frame : tout dessiner */
    ship_draw();
    ship_was_drawn = 1;
    asteroids_draw();
    hud_draw();             /* dessine score=0 et 3 vies */

    for (;;) {
        key_scan();

        /* Effacer (XOR) — ordre inverse du tracé */
        bullets_render();
        asteroids_draw();
        if (ship_was_drawn) ship_erase();

        /* Input → actions (ignoré en game over) */
        if (!gameover) {
            if (key_state & 0x01) ship_rotate((signed char)-1);
            if (key_state & 0x02) ship_rotate((signed char)+1);
            if (key_state & 0x04) ship_apply_thrust();

            fire_now = key_state & 0x08;
            if (fire_now && !prev_fire) bullet_fire();
            prev_fire = fire_now;
        }

        /* Mise à jour physique */
        if (!gameover) ship_update();
        bullets_update();
        asteroids_update();

        /* Collisions */
        collisions_bullets_asteroids();
        if (!gameover) collisions_ship_asteroids();

        /* Décompter invincibilité */
        if (ship_invincible) ship_invincible--;

        /* Spawn nouvelle vague si plus aucun asteroid actif */
        check_next_wave();

        /* Vaisseau visible (clignotement pendant invincibilité) */
        ship_visible = !gameover &&
                       (ship_invincible == 0 || (ship_invincible & 2));

        /* Redessiner aux nouvelles positions */
        if (ship_visible) {
            ship_draw();
            ship_was_drawn = 1;
        } else {
            ship_was_drawn = 0;
        }
        asteroids_draw();
        bullets_render();
        hud_draw();             /* redessine seulement si score/vies ont changé */

        /* Synchro frame 25 Hz */
        frame_wait();
    }
}
