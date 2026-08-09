/*
 * keys.h — Écran de configuration des touches (Phase 40)
 */

#ifndef KEYS_H
#define KEYS_H

/* Codes utiles hors keys.c (col*8 + row — cf. keys_tab.h) */
#define KEY_PROBE_NONE 0xFF     /* key_probe : aucune touche pressée */
#define KEY_PROBE_K    24       /* K (col 3, row 0) — ouvre la config */
#define KEY_PROBE_ESC  13       /* ESC (col 1, row 5) — annule        */

/* input.s — scan complet de la matrice 8×8 : code de la première touche
 * pressée trouvée (col*8 + row), ou KEY_PROBE_NONE. À réserver aux
 * écrans (titre/config) : ~8 écritures PSG + 64 lectures par appel. */
unsigned char key_probe(void);

/* input.s — table des 5 actions remappables, 2 octets par action
 * (colonne ORB, masque R14 actif bas), ordre = bits de key_state.
 * Initialisée aux défauts (flèches + SPACE) au chargement. */
extern unsigned char key_map[10];

/* Écran CONTROLS : re-mappe les 5 actions séquentiellement.
 * ESC annule (mapping restauré). L'appelant doit avoir effacé les
 * textes du titre ; les entités XOR encore affichées peuvent rester
 * (l'écran s'efface proprement par re-XOR en sortie). */
void keys_screen(void);

#endif /* KEYS_H */
