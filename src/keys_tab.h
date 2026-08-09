/*
 * keys_tab.h — Table matrice clavier Oric-1 → nom de touche (Phase 40)
 *
 * Code touche = col*8 + row, avec :
 *   col = colonne matérielle (VIA ORB[0:2], 74LS138)
 *   row = rangée matérielle (bit PSG R14, actif bas)
 *
 * Source : matrice dérivée des tables clavier ROM $FF70 (référence :
 * char_map de Phosphoric src/io/keyboard.c, elle-même validée contre
 * la ROM Oric-1 BASIC 1.0). Cohérente avec les positions déjà validées
 * du projet : SPACE (4,0), UP (4,3), LEFT (4,5), DOWN (4,6),
 * RIGHT (4,7), ESC (1,5), LSHIFT (4,4).
 *
 * NULL = position inconnue/non connectée/réservée : refusée par
 * l'écran de config (on n'invente pas les positions non documentées).
 * ESC (code 13) est NULL car réservé : il ANNULE la configuration.
 *
 * Ce header contient des définitions de données : à inclure UNIQUEMENT
 * par keys.c et par le test host (tests/host/test_keys.c) — pas par
 * les autres modules (duplication de la table sinon).
 */

#ifndef KEYS_TAB_H
#define KEYS_TAB_H

#define KEY_CODE(col, row) ((unsigned char)((col) * 8 + (row)))

static const char * const key_name[64] = {
    /* col 0 */ "7",     "N",     "5",     "V",     0,       "1",     "X",     "3",
    /* col 1 */ "J",     "T",     "R",     "F",     0,       0 /*ESC*/, "Q",   "D",
    /* col 2 */ "M",     "6",     "B",     "4",     "CTRL",  "Z",     "2",     "C",
    /* col 3 */ "K",     "9",     "SEMI",  "MINUS", 0,       0,       "BSLASH","QUOTE",
    /* col 4 */ "SPACE", "COMMA", "DOT",   "UP",    "SHIFT", "LEFT",  "DOWN",  "RIGHT",
    /* col 5 */ "U",     "I",     "O",     "P",     "FUNCT", "DEL",   "RBRK",  "LBRK",
    /* col 6 */ "Y",     "H",     "G",     "E",     0,       "A",     "S",     "W",
    /* col 7 */ "8",     "L",     "0",     "SLASH", "SHIFT", "RETURN", 0,      "EQUAL"
};

/* Défauts des 5 actions, ordre = bits de key_state (LEFT=bit0, RIGHT=1,
 * THRUST=2, FIRE=3, HYPER=4) — mêmes positions que le mapping historique
 * flèches + SPACE de input.s. */
static const unsigned char key_default_code[5] = {
    KEY_CODE(4, 5),     /* LEFT   = ←     */
    KEY_CODE(4, 7),     /* RIGHT  = →     */
    KEY_CODE(4, 3),     /* THRUST = ↑     */
    KEY_CODE(4, 0),     /* FIRE   = SPACE */
    KEY_CODE(4, 6)      /* HYPER  = ↓     */
};

#endif /* KEYS_TAB_H */
