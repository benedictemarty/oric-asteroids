/*
 * title.c — Écran titre "ASTERORIC" Phase 9c (retitré 2026-06-12,
 * ex-"ASTEROIDS" — cf. NOTICE.md, marque Atari Interactive)
 *
 * Phase 40 : les glyphes vectoriels et draw_letter ont migré dans
 * font.c (police complète A-Z pour l'écran de config des touches) ;
 * ce fichier ne garde que les labels du jeu, exprimés en chaînes via
 * text_draw (pitch 12, positions inchangées — snapshot titre vérifié
 * bit-à-bit identique au refactor).
 */

#include "font.h"
#include "hud.h"

/* Dessine "ASTERORIC" centré horizontalement.
 * Largeur totale = 9 * 12 - 4 = 104 pixels → x = (240 - 104) / 2 = 68. */
void title_draw(void)
{
    text_draw("ASTERORIC", 68, 80);
}

/* Erase = même routine (XOR idempotent) */
void title_erase(void)
{
    title_draw();
}

/* Dessine "GAME OVER" centré en y=70.
 * 9 caractères (avec espace) × 12 = 108 → x = (240 - 108) / 2 = 66. */
void gameover_draw(void)
{
    text_draw("GAME OVER", 66, 70);
}

void gameover_erase(void)
{
    gameover_draw();
}

/* Dessine "PRESS SPACE" à y donné.
 * 11 caractères × 12 = 132 → x = (240 - 132) / 2 = 54. */
void presspace_draw(unsigned char py)
{
    text_draw("PRESS SPACE", 54, py);
}

void presspace_erase(unsigned char py)
{
    presspace_draw(py);
}

/* Phase 10d/10j — affichage "WAVE nn" en haut-centre.
 * 5 caractères ("WAVE ") + 1 ou 2 chiffres.
 * Phase 10j : si wave > 9, afficher 2 chiffres (10, 11).
 * Les chiffres gardent leur placement historique (x+56, pitch 6),
 * distinct du pitch 12 de text_draw. */
void wave_label_draw(unsigned char py, unsigned char digit)
{
    unsigned char x = 80;
    text_draw("WAVE", x, py);
    if (digit > 99) digit = 99;
    if (digit < 10) {
        hud_xor_digit(digit, x + 56, py);
    } else {
        hud_xor_digit(digit / 10, x + 56, py);
        hud_xor_digit(digit % 10, x + 62, py);
    }
}

void wave_label_erase(unsigned char py, unsigned char digit)
{
    wave_label_draw(py, digit);
}

/* Phase 15 — affichage "HIGH SCORES" centré horizontal.
 * 11 caractères × 12 = 132 px, x = (240-132)/2 = 54. */
void hiscores_label_draw(unsigned char py)
{
    text_draw("HIGH SCORES", 54, py);
}

void hiscores_label_erase(unsigned char py)
{
    hiscores_label_draw(py);
}

/* Phase 18i — "OR ESC TO STOP" sous "PRESS SPACE" en game over.
 * 14 caractères (avec 3 espaces) × 12 = 168, x = (240-168)/2 = 36. */
void quit_label_draw(unsigned char py)
{
    text_draw("OR ESC TO STOP", 36, py);
}

void quit_label_erase(unsigned char py)
{
    quit_label_draw(py);
}

/* Phase 40 — "K CONTROLS" sur l'écran titre (accès config touches).
 * 10 caractères × 12 = 120, x = (240-120)/2 = 60. */
void keyshint_draw(unsigned char py)
{
    text_draw("K CONTROLS", 60, py);
}

void keyshint_erase(unsigned char py)
{
    keyshint_draw(py);
}
