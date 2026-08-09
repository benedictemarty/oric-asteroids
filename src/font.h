/*
 * font.h — Police vectorielle A-Z + 0-9 (Phase 40)
 *
 * Lettres 8×10 px en segments XOR (format compact hérité de title.c),
 * chiffres 7-segments 4×6 via hud_xor_digit. Pitch fixe 12 px/caractère.
 */

#ifndef FONT_H
#define FONT_H

/* Pitch horizontal d'un caractère (lettre, chiffre ou espace) */
#define FONT_PITCH 12

/* Dessine (XOR) la chaîne s à partir de (x, y). Supporte 'A'-'Z',
 * '0'-'9' et ' ' ; tout autre caractère est traité comme un espace.
 * Erase = rappeler avec la même chaîne (XOR idempotent). */
void text_draw(const char *s, unsigned char x, unsigned char y);

#endif /* FONT_H */
