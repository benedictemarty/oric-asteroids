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

/* ------------------------------------------------------------------ */
/* Symboles importés                                                   */
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

#define SHIP_RADIUS         7
#define INVINCIBLE_FRAMES   40

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
#define DEBRIS_TTL        50

/* Vélocités exactes ShipExpVelTbl arcade rev 4 ($50EC), nibbles haut
 * sign-extended. Chaque pair = (vx, vy) pour un fragment. */
static const signed char ship_debris_vx[DEBRIS_COUNT] = { -3, +3,  0, +3,  0, -3 };
static const signed char ship_debris_vy[DEBRIS_COUNT] = { +1, -2, -4, +1, +4, -3 };

static unsigned char  dbr_x[DEBRIS_COUNT];
static unsigned char  dbr_y[DEBRIS_COUNT];
static signed char    dbr_vx[DEBRIS_COUNT];
static signed char    dbr_vy[DEBRIS_COUNT];
static unsigned char  dbr_ttl[DEBRIS_COUNT];

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
        dbr_ttl[i] = DEBRIS_TTL - i * 6;      /* séquentielle arcade */
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

/* Tracé des fragments en XOR : chaque fragment = mini-segment de 3 px
 * dans la direction du mouvement. */
static void debris_render(void)
{
    unsigned char i;
    for (i = 0; i < DEBRIS_COUNT; i++) {
        if (dbr_ttl[i] == 0) continue;
        lx0 = dbr_x[i];
        ly0 = dbr_y[i];
        lx1 = (unsigned char)((signed char)dbr_x[i] + (dbr_vx[i] >> 1));
        ly1 = (unsigned char)((signed char)dbr_y[i] + (dbr_vy[i] >> 1));
        draw_line_xor();
    }
}

static void debris_init(void)
{
    unsigned char i;
    for (i = 0; i < DEBRIS_COUNT; i++) dbr_ttl[i] = 0;
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
    ship_vx = 0;
    ship_vy = 0;
    ship_invincible = INVINCIBLE_FRAMES / 2;     /* mini-invincibilité */
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
            blt_vx[i]  = ship_thrx[ship_angle];
            blt_vy[i]  = ship_thry[ship_angle];
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
        plot(blt_x[i], blt_y[i]);
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

/* Insère final_score dans la table triée si éligible */
static void hiscores_insert(unsigned int final_score)
{
    unsigned char i, j;
    for (i = 0; i < HISCORE_COUNT; i++) {
        if (final_score > hiscores[i]) {
            /* Décaler vers le bas et insérer */
            for (j = HISCORE_COUNT - 1; j > i; j--) {
                hiscores[j] = hiscores[j - 1];
            }
            hiscores[i] = final_score;
            return;
        }
    }
}

/* Dessine la table des high scores en game over (centre écran).
 * Phase 9 : utilise hud_xor_5digits (rendu 7-seg propre). */
static void hiscores_draw_table(void)
{
    unsigned char i, py;
    for (i = 0; i < HISCORE_COUNT; i++) {
        py = 70 + i * 14;
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
        for (i = 0; i < 200; i++) {
            key_scan();
            if ((key_state & 0x08) && !prev_space) break;
            prev_space = key_state & 0x08;
            asteroids_draw();
            asteroids_update();
            asteroids_draw();
            /* Phase 10m : toggle PRESS SPACE tous les 24 frames (~1.4 s à 17 Hz) */
            if ((i & 0x17) == 0x17) {
                presspace_erase(110);
                ps_visible = !ps_visible;
                if (ps_visible) presspace_draw(110);
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
        key_scan();

        /* Effacer (XOR) — ordre inverse du tracé */
        debris_render();             /* erase debris si actifs */
        ufo_bullet_draw();
        ufo_draw();
        bullets_render();
        asteroids_draw();
        if (ship_was_drawn) ship_erase();
        if (gameover && hiscores_drawn) {
            hiscores_draw_table();
        }
        if (gameover && gameover_text_drawn) {
            gameover_erase();
            presspace_erase(140);
        }

        /* Restart : SPACE en game over */
        if (gameover && (key_state & 0x08)) {
            game_reset();
            prev_gameover = 0;
            hiscores_drawn = 0;
            gameover_text_drawn = 0;
            continue;
        }

        /* Input → actions (ignoré en game over) */
        if (!gameover) {
            if (key_state & 0x01) ship_rotate((signed char)-1);
            if (key_state & 0x02) ship_rotate((signed char)+1);
            if (key_state & 0x04) {
                ship_apply_thrust();
                /* Phase 9f : son thrust intermittent quand aucun autre FX */
                if (sfx_id == FX_NONE) sound_play_fx(FX_THRUST);
            }

            fire_now = key_state & 0x08;
            if (fire_now && !prev_fire) bullet_fire();
            prev_fire = fire_now;

            /* Hyperespace : edge-trigger sur DOWN + cooldown */
            hyper_now = key_state & 0x10;
            if (hyper_now && !prev_hyper && hyper_cd == 0) {
                ship_hyperspace();
                hyper_cd = HYPER_COOLDOWN;
            }
            prev_hyper = hyper_now;
            if (hyper_cd) hyper_cd--;
        }

        /* Mise à jour physique */
        if (!gameover) ship_update();
        bullets_update();
        asteroids_update();
        if (!gameover) ufo_tick(ship_x, ship_y, score);
        ufo_bullet_update();
        debris_update();

        /* Collisions */
        collisions_bullets_asteroids();
        collisions_ufobullet_asteroids();
        if (!gameover) collisions_ship_asteroids();

        if (ship_invincible) ship_invincible--;
        check_next_wave();

        /* Phase 9f : extra life détectée (lives a augmenté) → FX_LIFE */
        if (lives > lives_prev && sfx_id == FX_NONE) {
            sound_play_fx(FX_LIFE);
        }
        lives_prev = lives;

        /* Phase 10d : afficher "WAVE n" si la vague a changé */
        if (current_wave != wave_displayed) {
            if (wave_displayed != 0) wave_label_erase(WAVE_HUD_Y, wave_displayed);
            wave_label_draw(WAVE_HUD_Y, current_wave);
            wave_displayed = current_wave;
        }

        /* Phase 8 : thump cadencé sur asteroids_count, accélère quand
         * il reste peu d'asteroids (cf. arcade : tension croissante).
         * Phase 10n : prioritaire sur UFO sound si UFO actif. */
        if (!gameover && sfx_id == FX_NONE) {
            if (ufo_active) {
                /* Phase 10n : bip-bip UFO continu tant qu'UFO actif */
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

        /* Détecter passage en game over (insérer dans hi-scores) */
        if (gameover && !prev_gameover) {
            final_score = score;
            hiscores_insert(final_score);
            ufo_kill();             /* propre : retirer UFO de l'écran */
        }
        prev_gameover = gameover;

        /* Vaisseau visible (clignotement pendant invincibilité) */
        ship_visible = !gameover &&
                       (ship_invincible == 0 || (ship_invincible & 2));

        /* Redessiner */
        if (ship_visible) {
            ship_draw();
            ship_was_drawn = 1;
        } else {
            ship_was_drawn = 0;
        }
        asteroids_draw();
        bullets_render();
        ufo_draw();
        ufo_bullet_draw();
        debris_render();             /* redraw debris aux nouvelles positions */
        hud_draw();
        if (gameover) {
            hiscores_draw_table();
            hiscores_drawn = 1;
            gameover_draw();
            presspace_draw(140);
            gameover_text_drawn = 1;
        }

        frame_wait();
    }
}
