/*
 * test_keys.c — Tests host de la table matrice → nom (Phase 40)
 *
 * Contrairement à test_rng.c (copies), ce test inclut la VRAIE table
 * partagée src/keys_tab.h (aussi incluse par keys.c) : toute dérive
 * de la table casse le test.
 *
 * Build : gcc -Wall -Wextra -std=c90 -O2 -o test_keys test_keys.c
 */

#include <stdio.h>
#include <string.h>

#include "../../src/keys_tab.h"

static int failures = 0;

#define CHECK(cond, msg) \
    do { if (!(cond)) { printf("    FAIL: %s\n", msg); failures++; } } while (0)

/* T1 — les 5 défauts pointent vers des positions nommées et attendues */
static void t1_defaults(void)
{
    const char *expect[5] = {"LEFT", "RIGHT", "UP", "SPACE", "DOWN"};
    unsigned char i, j;
    for (i = 0; i < 5; i++) {
        unsigned char c = key_default_code[i];
        CHECK(c < 64, "code défaut hors matrice");
        CHECK(key_name[c] != 0, "code défaut sans nom");
        CHECK(key_name[c] && strcmp(key_name[c], expect[i]) == 0,
              "nom du défaut inattendu");
        for (j = 0; j < i; j++)
            CHECK(key_default_code[j] != c, "doublon dans les défauts");
    }
    printf("  T1 défauts nommés       : %s\n", failures ? "FAIL" : "PASS");
}

/* T2 — positions ancrées (validées Phosphoric/matériel réel + ROM $FF70) */
static void t2_anchors(void)
{
    int f0 = failures;
    CHECK(strcmp(key_name[KEY_CODE(4, 0)], "SPACE") == 0, "SPACE (4,0)");
    CHECK(strcmp(key_name[KEY_CODE(3, 0)], "K") == 0,     "K (3,0)");
    CHECK(strcmp(key_name[KEY_CODE(6, 5)], "A") == 0,     "A (6,5)");
    CHECK(strcmp(key_name[KEY_CODE(7, 5)], "RETURN") == 0, "RETURN (7,5)");
    CHECK(strcmp(key_name[KEY_CODE(4, 4)], "SHIFT") == 0, "LSHIFT (4,4)");
    CHECK(key_name[KEY_CODE(1, 5)] == 0, "ESC (1,5) doit être NULL (réservé)");
    printf("  T2 ancres matrice       : %s\n", failures > f0 ? "FAIL" : "PASS");
}

/* T3 — pas de nom dupliqué parmi les positions assignables (les deux
 * SHIFT exceptés : physiquement deux touches, même légende) */
static void t3_no_dup_names(void)
{
    int f0 = failures;
    unsigned char i, j;
    for (i = 0; i < 64; i++) {
        if (key_name[i] == 0) continue;
        for (j = (unsigned char)(i + 1); j < 64; j++) {
            if (key_name[j] == 0) continue;
            if (strcmp(key_name[i], key_name[j]) == 0)
                CHECK(strcmp(key_name[i], "SHIFT") == 0,
                      "nom dupliqué (hors SHIFT)");
        }
    }
    printf("  T3 unicité des noms     : %s\n", failures > f0 ? "FAIL" : "PASS");
}

/* T4 — le masque R14 dérivé d'un code (formule de keys.c key_apply)
 * retombe sur les masques historiques de input.s pour les défauts */
static void t4_mask_formula(void)
{
    int f0 = failures;
    static const unsigned char hist_col[5]  = {4, 4, 4, 4, 4};
    static const unsigned char hist_mask[5] = {0xDF, 0x7F, 0xF7, 0xFE, 0xBF};
    unsigned char i;
    for (i = 0; i < 5; i++) {
        unsigned char code = key_default_code[i];
        unsigned char col  = (unsigned char)(code >> 3);
        unsigned char mask = (unsigned char)~(1 << (code & 7));
        CHECK(col == hist_col[i], "colonne dérivée != input.s historique");
        CHECK(mask == hist_mask[i], "masque R14 dérivé != input.s historique");
    }
    printf("  T4 formule col/masque   : %s\n", failures > f0 ? "FAIL" : "PASS");
}

int main(void)
{
    printf("test_keys (host C90, src/keys_tab.h — vraies sources)\n");
    t1_defaults();
    t2_anchors();
    t3_no_dup_names();
    t4_mask_formula();
    if (failures) {
        printf("Total : ECHECS = %d\n", failures);
        return 1;
    }
    printf("Total : 4/4\n");
    return 0;
}
