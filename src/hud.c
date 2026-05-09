/*
 * hud.c — HUD Asteroids Phase 5
 *
 * Score à 5 chiffres en 7-segments (4×6 px chacun, ~6 segments tracés
 * par chiffre, total ~30 segments XOR par redraw). Vies = mini-triangles
 * en haut-droite. Optimisation : ne redessine que sur changement (le
 * score change rarement comparé au framerate).
 */

#include "hud.h"

extern unsigned char lx0, ly0, lx1, ly1;
#pragma zpsym ("lx0")
#pragma zpsym ("ly0")
#pragma zpsym ("lx1")
#pragma zpsym ("ly1")

void draw_line_xor(void);

unsigned int  score;
unsigned int  score_extra;
unsigned char lives;
unsigned char gameover;

/* Reflet du dernier état affiché (HUD redessiné en XOR si != état courant) */
static unsigned int  score_shown;
static unsigned char lives_shown;
static unsigned char hud_first_frame;

/* 7 segments — bits de poids fort à faible : A B C D E F G */
static const unsigned char digit_segs[10] = {
    0x7E,   /* 0 = A B C D E F   */
    0x30,   /* 1 = B C           */
    0x6D,   /* 2 = A B D E G     */
    0x79,   /* 3 = A B C D G     */
    0x33,   /* 4 = B C F G       */
    0x5B,   /* 5 = A C D F G     */
    0x5F,   /* 6 = A C D E F G   */
    0x70,   /* 7 = A B C         */
    0x7F,   /* 8 = all           */
    0x7B,   /* 9 = A B C D F G   */
};

#define D_W   4         /* largeur d'un chiffre */
#define D_H   6         /* hauteur (multiple de 2) */
#define D_HM  3         /* H / 2 */
#define D_GAP 2         /* espace entre chiffres */
#define D_PITCH (D_W + D_GAP)

/* Position d'origine du score (haut-gauche) et des vies (haut-droite) */
#define SCORE_X  4
#define SCORE_Y  4
#define LIVES_X  216    /* à droite, place pour 3 mini-vaisseaux */
#define LIVES_Y  4

static void line(unsigned char x0, unsigned char y0,
                 unsigned char x1, unsigned char y1)
{
    lx0 = x0; ly0 = y0; lx1 = x1; ly1 = y1;
    draw_line_xor();
}

/* Trace les segments allumés du chiffre d, origine (px, py) */
static void draw_digit(unsigned char d, unsigned char px, unsigned char py)
{
    unsigned char m = digit_segs[d];
    /* A — top horizontal */
    if (m & 0x40) line(px, py, px + D_W, py);
    /* B — top right */
    if (m & 0x20) line(px + D_W, py, px + D_W, py + D_HM);
    /* C — bottom right */
    if (m & 0x10) line(px + D_W, py + D_HM, px + D_W, py + D_H);
    /* D — bottom horizontal */
    if (m & 0x08) line(px, py + D_H, px + D_W, py + D_H);
    /* E — bottom left */
    if (m & 0x04) line(px, py + D_HM, px, py + D_H);
    /* F — top left */
    if (m & 0x02) line(px, py, px, py + D_HM);
    /* G — middle horizontal */
    if (m & 0x01) line(px, py + D_HM, px + D_W, py + D_HM);
}

void hud_xor_5digits(unsigned int s, unsigned char px, unsigned char py);

/* Tracer un score 5 chiffres (avec zéros à gauche) à (px, py) en XOR */
static void draw_score(unsigned int s, unsigned char px, unsigned char py)
{
    unsigned char d;
    /* Décomposition par soustraction de puissances de 10 (évite la division
     * cc65 longue) */
    unsigned int t = s;
    unsigned char d4 = 0;
    while (t >= 10000U) { t -= 10000U; d4++; }
    d = 0;
    while (t >= 1000U)  { t -= 1000U;  d++; }
    draw_digit(d4, px, py);
    draw_digit(d,  px + D_PITCH, py);
    d = 0;
    while (t >= 100U)   { t -= 100U;   d++; }
    draw_digit(d, px + 2 * D_PITCH, py);
    d = 0;
    while (t >= 10U)    { t -= 10U;    d++; }
    draw_digit(d, px + 3 * D_PITCH, py);
    draw_digit((unsigned char)t, px + 4 * D_PITCH, py);
}

/* Mini-triangle vaisseau (4 px haut, 3 px large) — pointe vers le haut */
static void draw_mini_ship(unsigned char cx, unsigned char cy)
{
    line(cx,     cy - 2, cx - 2, cy + 2);   /* P0 → P1 */
    line(cx,     cy - 2, cx + 2, cy + 2);   /* P0 → P2 */
    line(cx - 2, cy + 2, cx + 2, cy + 2);   /* P1 → P2 */
}

static void draw_lives(unsigned char n, unsigned char px, unsigned char py)
{
    unsigned char i;
    for (i = 0; i < n; i++) {
        draw_mini_ship(px + i * 6, py + 2);
    }
}

void hud_init(void)
{
    score = 0;
    score_extra = HUD_EXTRA_BONUS;
    lives = HUD_LIVES_INIT;
    score_shown = 0;
    lives_shown = 0;
    hud_first_frame = 1;        /* force le 1er draw, même si score==shown==0 */
    gameover = 0;
}

void hud_draw(void)
{
    if (hud_first_frame) {
        draw_score(score, SCORE_X, SCORE_Y);
        draw_lives(lives, LIVES_X, LIVES_Y);
        score_shown = score;
        lives_shown = lives;
        hud_first_frame = 0;
        return;
    }
    if (score != score_shown) {
        draw_score(score_shown, SCORE_X, SCORE_Y);    /* effacer ancien */
        draw_score(score, SCORE_X, SCORE_Y);           /* nouveau */
        score_shown = score;
    }
    if (lives != lives_shown) {
        draw_lives(lives_shown, LIVES_X, LIVES_Y);     /* effacer */
        draw_lives(lives, LIVES_X, LIVES_Y);            /* nouveau */
        lives_shown = lives;
    }
}

void hud_add_score(unsigned int delta)
{
    score += delta;
    if (score >= score_extra) {
        lives++;
        score_extra += HUD_EXTRA_BONUS;
    }
}

void hud_lose_life(void)
{
    if (lives == 0) {
        gameover = 1;
        return;
    }
    lives--;
    if (lives == 0) gameover = 1;
}

/* Wrapper public exporté pour réutiliser draw_score (table hi-scores) */
void hud_xor_5digits(unsigned int s, unsigned char px, unsigned char py)
{
    draw_score(s, px, py);
}
