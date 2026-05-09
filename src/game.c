/*
 * game.c — Boucle de jeu Phase 3 + Phase 4
 *
 * Phase 3 :
 *   - timing 25 Hz via VIA Timer 1 one-shot (40 000 cycles)
 *   - physique vaisseau (friction + intégration + wraparound)
 *   - 4 tirs simultanés (edge-trigger sur SPACE)
 *   - dispatch input → actions
 *
 * Phase 4 :
 *   - intégration asteroids (spawn vague initiale, draw/update XOR)
 *
 * L'état du vaisseau est en ZP (déclaré dans ship.s).
 * Les tirs et asteroids sont en RAM/BSS.
 */

#include "asteroids.h"

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

/* Tables de thrust générées par tools/gen_ship.py — RODATA */
extern const signed char ship_thrx[32];
extern const signed char ship_thry[32];

/* Routines asm */
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
#define BULLET_TTL      35      /* ~1.4 s à 25 Hz */
#define FIRE_COOLDOWN   4       /* frames entre deux tirs */
#define V_MAX           16      /* vitesse vaisseau max (px/frame) */

/* Marges anti-overflow vertices (vaisseau = ±12 px du centre) */
#define WX_MIN          14
#define WX_MAX          226
#define WY_MIN          14
#define WY_MAX          186
#define WX_SPAN         (WX_MAX - WX_MIN)
#define WY_SPAN         (WY_MAX - WY_MIN)

/* VIA 6522 (Phase 3 : timing 25 Hz via Timer 1) */
#define VIA_ACR         (*(volatile unsigned char*)0x030B)
#define VIA_IFR         (*(volatile unsigned char*)0x030D)
#define VIA_IER         (*(volatile unsigned char*)0x030E)
#define VIA_T1CL        (*(volatile unsigned char*)0x0304)
#define VIA_T1CH        (*(volatile unsigned char*)0x0305)

/* 1 frame 25 Hz = 40 000 cycles → latch 39 999 = $9C3F */
#define FRAME_LO        0x3F
#define FRAME_HI        0x9C

/* ------------------------------------------------------------------ */
/* État local (BSS)                                                    */
/* ------------------------------------------------------------------ */

static unsigned char blt_x[BULLETS];
static unsigned char blt_y[BULLETS];
static signed char   blt_vx[BULLETS];
static signed char   blt_vy[BULLETS];
static unsigned char blt_ttl[BULLETS];

static unsigned char prev_fire;
static unsigned char fire_cd;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void plot(unsigned char x, unsigned char y)
{
    lx0 = x; ly0 = y; lx1 = x; ly1 = y;
    draw_line_xor();
}

static void timer_init(void)
{
    /* ACR bits 7-6 = 00 → Timer 1 one-shot sans toggle PB7 */
    VIA_ACR = VIA_ACR & 0x3F;
    /* Désactiver IRQ Timer 1 (on poll IFR) — bit 7 = 0 → disable */
    VIA_IER = 0x40;
    /* Effacer un flag éventuel */
    VIA_IFR = 0x40;
}

static void frame_wait(void)
{
    /* Charger latch et démarrer (l'écriture sur T1C-H déclenche) */
    VIA_T1CL = FRAME_LO;
    VIA_T1CH = FRAME_HI;
    while (!(VIA_IFR & 0x40)) { }
    /* Lecture T1C-L = clear IFR bit 6 */
    (void)VIA_T1CL;
}

/* ------------------------------------------------------------------ */
/* Ship physics                                                        */
/* ------------------------------------------------------------------ */

static void ship_update(void)
{
    signed char d;
    int nx, ny;

    /* Friction × 15/16 par frame ≈ 0.9375 (cc65 : >>4 sur signé = ASR) */
    d = ship_vx >> 4;
    ship_vx -= d;
    d = ship_vy >> 4;
    ship_vy -= d;

    /* Clamp vitesse */
    if (ship_vx >  V_MAX) ship_vx =  V_MAX;
    if (ship_vx < -V_MAX) ship_vx = -V_MAX;
    if (ship_vy >  V_MAX) ship_vy =  V_MAX;
    if (ship_vy < -V_MAX) ship_vy = -V_MAX;

    /* Intégrer en 16-bit pour gérer le signe */
    nx = (int)ship_x + (int)ship_vx;
    ny = (int)ship_y + (int)ship_vy;

    /* Wraparound dans la zone sûre (vertices ne débordent pas) */
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
        /* Sortie d'écran → expiration immédiate */
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
/* Game loop                                                          */
/* ------------------------------------------------------------------ */

void game_run(void)
{
    unsigned char fire_now;

    hires_init();
    timer_init();
    ship_init();
    bullets_init();
    asteroids_init(0x42);
    asteroids_spawn_wave();

    /* Première frame : tout dessiner */
    ship_draw();
    asteroids_draw();

    for (;;) {
        key_scan();

        /* Effacer (XOR avec l'état précédent) — ordre inverse du tracé */
        bullets_render();
        asteroids_draw();
        ship_erase();

        /* Input → actions */
        if (key_state & 0x01) ship_rotate((signed char)-1);
        if (key_state & 0x02) ship_rotate((signed char)+1);
        if (key_state & 0x04) ship_apply_thrust();

        fire_now = key_state & 0x08;
        if (fire_now && !prev_fire) bullet_fire();
        prev_fire = fire_now;

        /* Mise à jour physique */
        ship_update();
        bullets_update();
        asteroids_update();

        /* Redessiner aux nouvelles positions */
        ship_draw();
        asteroids_draw();
        bullets_render();

        /* Synchro frame 25 Hz */
        frame_wait();
    }
}
