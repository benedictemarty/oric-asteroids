/*
 * keys.c — Écran de configuration des touches (Phase 40)
 *
 * Accessible depuis l'écran titre (touche K). Re-mappe séquentiellement
 * les 5 actions (LEFT, RIGHT, THRUST, FIRE, HYPER) : l'action courante
 * est soulignée, presser une touche connue l'assigne ; ESC annule tout
 * et restaure le mapping d'entrée. Les doublons et les positions
 * inconnues de la matrice (key_name NULL) sont refusés silencieusement.
 *
 * Le mapping vit en RAM (key_map, input.s) — pas de persistance :
 * retour aux défauts (flèches + SPACE) à chaque chargement du .tap.
 *
 * Timing : boucles cadencées sur frame_cnt (IRQ T1 50 Hz, volatile),
 * indépendantes de l'ordonnanceur 25 Hz de game.c (frame_wait y est
 * static). Budget CPU sans objet ici : aucune entité n'est mise à
 * jour pendant la config (démo titre gelée).
 */

#include "keys.h"
#include "keys_tab.h"
#include "font.h"
#include "line.h"
#include "sound.h"

/* Codes courants des 5 actions (miroir C de key_map, pour affichage
 * des noms et détection de doublons). Doit rester synchrone de key_map
 * — seule key_apply écrit les deux. */
static unsigned char key_codes[5] = {
    KEY_CODE(4, 5), KEY_CODE(4, 7), KEY_CODE(4, 3),
    KEY_CODE(4, 0), KEY_CODE(4, 6)
};

static const char * const act_label[5] = {
    "LEFT", "RIGHT", "THRUST", "FIRE", "HYPER"
};

/* Layout : titre + 5 lignes label/nom. Souligné = ligne sous le label
 * de l'action en cours de saisie. */
#define KTITLE_Y  28
#define KROW0_Y   56
#define KROW_H    18
#define KLBL_X    36
#define KNAME_X   126

static unsigned char row_y(unsigned char i)
{
    return (unsigned char)(KROW0_Y + i * KROW_H);
}

/* Applique code à l'action i : miroir C + table asm de key_scan.
 * key_map[i*2] = colonne ORB, key_map[i*2+1] = masque R14 (bit row à 0,
 * actif bas). Écritures sur octets : atomiques vis-à-vis de l'IRQ (qui
 * de toute façon ne lit pas key_map — sound_tick uniquement). */
static void key_apply(unsigned char i, unsigned char code)
{
    key_codes[i] = code;
    key_map[i * 2]     = code >> 3;
    key_map[i * 2 + 1] = (unsigned char)~(1 << (code & 7));
}

/* Attendre le prochain tick 50 Hz (IRQ T1) */
static void kwait(void)
{
    unsigned char t = frame_cnt;
    while (frame_cnt == t) ;
}

static void underline(unsigned char i)
{
    lx0 = KLBL_X;
    ly0 = (unsigned char)(row_y(i) + 12);
    lx1 = KLBL_X + 70;
    ly1 = ly0;
    draw_line_xor();
}

/* Dessiner/effacer (XOR idempotent) tout l'écran : titre + labels +
 * noms courants. Valide comme erase uniquement si l'affichage est
 * synchrone de key_codes — garanti par le flux de keys_screen. */
static void screen_xor(void)
{
    unsigned char i;
    text_draw("CONTROLS", 72, KTITLE_Y);
    for (i = 0; i < 5; i++) {
        text_draw(act_label[i], KLBL_X, row_y(i));
        text_draw(key_name[key_codes[i]], KNAME_X, row_y(i));
    }
}

static void wait_release(void)
{
    do { kwait(); } while (key_probe() != KEY_PROBE_NONE);
}

void keys_screen(void)
{
    unsigned char i, j, k;
    unsigned char cancel = 0;
    unsigned char saved[5];

    for (i = 0; i < 5; i++) saved[i] = key_codes[i];

    screen_xor();

    for (i = 0; i < 5 && !cancel; i++) {
        underline(i);
        wait_release();         /* ne pas capter la touche précédente */
        for (;;) {
            kwait();
            k = key_probe();
            if (k == KEY_PROBE_NONE) continue;
            if (k == KEY_PROBE_ESC) { cancel = 1; break; }
            if (key_name[k] == 0) continue;      /* position inconnue */
            for (j = 0; j < 5; j++)              /* doublon ? */
                if (j != i && key_codes[j] == k) break;
            if (j < 5) continue;
            break;
        }
        if (!cancel && k != key_codes[i]) {
            text_draw(key_name[key_codes[i]], KNAME_X, row_y(i)); /* erase */
            key_apply(i, k);
            text_draw(key_name[k], KNAME_X, row_y(i));            /* draw */
        }
        if (!cancel) sound_play_fx(FX_FIRE);     /* feedback assignation */
        underline(i);           /* erase souligné */
    }

    wait_release();
    screen_xor();               /* erase écran (synchro key_codes ✓) */

    if (cancel) {
        for (i = 0; i < 5; i++) key_apply(i, saved[i]);
    }
}
