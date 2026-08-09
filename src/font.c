/*
 * font.c — Police vectorielle A-Z (Phase 40)
 *
 * Lettres 8×10 px déplacées depuis title.c (Phases 9c-18i) + les 12
 * lettres manquantes (B D F J K L N Q U X Y Z) pour l'écran de
 * configuration des touches. Chiffres = 7-segments 4×6 (hud_xor_digit).
 *
 * Format compact par lettre :
 *   liste de 4-tuples (x0,y0,x1,y1) — segments —
 *   0xFF marqueur fin segments
 *   liste de 2-tuples (x,y) — plots de sommets partagés —
 *   0xFE marqueur fin total (peut suivre directement 0xFF si pas de plots)
 *
 * Le replot des sommets partagés contre-balance le double-XOR : un pixel
 * traversé par un nombre PAIR de segments est XOR pair → éteint ; le
 * replot le re-XOR → tracé. Un pixel traversé par 3 segments reste
 * allumé sans replot (cf. K, Y, B en 6,4).
 */

#include "font.h"
#include "line.h"
#include "hud.h"

static const unsigned char letter_A[] = {
    0, 9, 4, 0,         /* gauche montante */
    4, 0, 8, 9,         /* droite descendante */
    2, 5, 6, 5,         /* barre milieu */
    0xFF,
    4, 0,               /* plot pointe (partagée) */
    0xFE
};

static const unsigned char letter_B[] = {
    0, 0, 0, 9,
    0, 0, 6, 0,
    6, 0, 8, 2,
    8, 2, 6, 4,
    6, 4, 0, 4,
    6, 4, 8, 6,
    8, 6, 6, 9,
    6, 9, 0, 9,
    0xFF,
    0, 0,  6, 0,  8, 2,  0, 4,  8, 6,  6, 9,  0, 9,
    0xFE
};

static const unsigned char letter_C[] = {
    8, 1, 0, 1,
    0, 1, 0, 9,
    0, 9, 8, 9,
    0xFF,
    0, 1,  0, 9,
    0xFE
};

static const unsigned char letter_D[] = {
    0, 0, 0, 9,
    0, 0, 5, 0,
    5, 0, 8, 3,
    8, 3, 8, 6,
    8, 6, 5, 9,
    5, 9, 0, 9,
    0xFF,
    0, 0,  5, 0,  8, 3,  8, 6,  5, 9,  0, 9,
    0xFE
};

static const unsigned char letter_E[] = {
    0, 0, 0, 9,
    0, 0, 8, 0,
    0, 5, 6, 5,
    0, 9, 8, 9,
    0xFF,
    0, 0,  0, 5,  0, 9,
    0xFE
};

static const unsigned char letter_F[] = {
    0, 0, 0, 9,
    0, 0, 8, 0,
    0, 5, 6, 5,
    0xFF,
    0, 0,  0, 5,
    0xFE
};

static const unsigned char letter_G[] = {
    8, 1, 0, 1,
    0, 1, 0, 9,
    0, 9, 8, 9,
    8, 9, 8, 5,
    8, 5, 4, 5,
    0xFF,
    0, 1,  0, 9,  8, 9,  8, 5,
    0xFE
};

static const unsigned char letter_H[] = {
    0, 0, 0, 9,         /* verticale gauche */
    8, 0, 8, 9,         /* verticale droite */
    0, 4, 8, 4,         /* barre milieu */
    0xFF,
    0, 4,  8, 4,        /* sommets partagés barre/verticales */
    0xFE
};

static const unsigned char letter_I[] = {
    4, 0, 4, 9,
    0, 0, 8, 0,
    0, 9, 8, 9,
    0xFF,
    4, 0,  4, 9,
    0xFE
};

static const unsigned char letter_J[] = {
    0, 0, 8, 0,
    5, 0, 5, 9,
    5, 9, 0, 9,
    0, 9, 0, 7,
    0xFF,
    5, 0,  5, 9,  0, 9,
    0xFE
};

static const unsigned char letter_K[] = {
    0, 0, 0, 9,         /* verticale */
    8, 0, 0, 5,         /* diag haute */
    0, 5, 8, 9,         /* diag basse (0,5 = 3 segments → allumé) */
    0xFF,
    0xFE
};

static const unsigned char letter_L[] = {
    0, 0, 0, 9,
    0, 9, 8, 9,
    0xFF,
    0, 9,
    0xFE
};

static const unsigned char letter_M[] = {
    0, 9, 0, 0,
    0, 0, 4, 5,
    4, 5, 8, 0,
    8, 0, 8, 9,
    0xFF,
    0, 0,  4, 5,  8, 0,
    0xFE
};

static const unsigned char letter_N[] = {
    0, 9, 0, 0,
    0, 0, 8, 9,
    8, 9, 8, 0,
    0xFF,
    0, 0,  8, 9,
    0xFE
};

static const unsigned char letter_O[] = {
    0, 0, 8, 0,
    8, 0, 8, 9,
    8, 9, 0, 9,
    0, 9, 0, 0,
    0xFF,
    0, 0,  8, 0,  8, 9,  0, 9,
    0xFE
};

static const unsigned char letter_P[] = {
    0, 0, 0, 9,
    0, 0, 6, 0,
    6, 0, 8, 2,
    8, 2, 8, 4,
    8, 4, 0, 5,
    0xFF,
    0, 0,  6, 0,  8, 2,  8, 4,
    0xFE
};

static const unsigned char letter_Q[] = {
    0, 0, 8, 0,
    8, 0, 8, 9,
    8, 9, 0, 9,
    0, 9, 0, 0,
    5, 6, 7, 8,         /* queue interne (ne touche pas le contour) */
    0xFF,
    0, 0,  8, 0,  8, 9,  0, 9,
    0xFE
};

static const unsigned char letter_R[] = {
    0, 0, 0, 9,
    0, 0, 6, 0,
    6, 0, 8, 2,
    8, 2, 8, 4,
    8, 4, 0, 5,
    0, 5, 8, 9,
    0xFF,
    0, 0,  6, 0,  8, 2,  8, 4,  0, 5,
    0xFE
};

static const unsigned char letter_S[] = {
    8, 1, 0, 1,
    0, 1, 0, 5,
    0, 5, 8, 5,
    8, 5, 8, 9,
    8, 9, 0, 9,
    0xFF,
    0, 1,  0, 5,  8, 5,  8, 9,
    0xFE
};

static const unsigned char letter_T[] = {
    0, 0, 8, 0,
    4, 0, 4, 9,
    0xFF,
    4, 0,
    0xFE
};

static const unsigned char letter_U[] = {
    0, 0, 0, 9,
    0, 9, 8, 9,
    8, 9, 8, 0,
    0xFF,
    0, 9,  8, 9,
    0xFE
};

static const unsigned char letter_V[] = {
    0, 0, 4, 9,
    4, 9, 8, 0,
    0xFF,
    4, 9,
    0xFE
};

static const unsigned char letter_W[] = {
    0, 0, 2, 9,         /* gauche descendante */
    2, 9, 4, 4,         /* montée vers centre */
    4, 4, 6, 9,         /* descente droite-centre */
    6, 9, 8, 0,         /* montée droite */
    0xFF,
    2, 9,  4, 4,  6, 9,
    0xFE
};

static const unsigned char letter_X[] = {
    0, 0, 4, 4,         /* 2 V opposés — croisement central déterministe */
    4, 4, 8, 0,
    0, 9, 4, 5,
    4, 5, 8, 9,
    0xFF,
    4, 4,  4, 5,
    0xFE
};

static const unsigned char letter_Y[] = {
    0, 0, 4, 4,
    8, 0, 4, 4,
    4, 4, 4, 9,         /* 4,4 = 3 segments → allumé sans replot */
    0xFF,
    0xFE
};

static const unsigned char letter_Z[] = {
    0, 0, 8, 0,
    8, 0, 0, 9,
    0, 9, 8, 9,
    0xFF,
    8, 0,  0, 9,
    0xFE
};

static const unsigned char * const letters[26] = {
    letter_A, letter_B, letter_C, letter_D, letter_E, letter_F,
    letter_G, letter_H, letter_I, letter_J, letter_K, letter_L,
    letter_M, letter_N, letter_O, letter_P, letter_Q, letter_R,
    letter_S, letter_T, letter_U, letter_V, letter_W, letter_X,
    letter_Y, letter_Z
};

static void draw_letter(const unsigned char *segs,
                        unsigned char ox, unsigned char oy)
{
    unsigned char i = 0;
    while (segs[i] != 0xFF) {
        lx0 = ox + segs[i + 0];
        ly0 = oy + segs[i + 1];
        lx1 = ox + segs[i + 2];
        ly1 = oy + segs[i + 3];
        draw_line_xor();
        i += 4;
    }
    i++;     /* skip 0xFF */
    while (segs[i] != 0xFE) {
        lx0 = ox + segs[i + 0];
        ly0 = oy + segs[i + 1];
        lx1 = lx0;
        ly1 = ly0;
        draw_line_xor();    /* plot 1 pixel */
        i += 2;
    }
}

void text_draw(const char *s, unsigned char x, unsigned char y)
{
    unsigned char c;
    while ((c = (unsigned char)*s) != 0) {
        if (c >= 'A' && c <= 'Z') {
            draw_letter(letters[c - 'A'], x, y);
        } else if (c >= '0' && c <= '9') {
            /* Chiffre 7-seg 4×6 centré dans la cellule 8×10 */
            hud_xor_digit(c - '0', x + 2, y + 2);
        }
        /* ' ' (et tout autre caractère) : avance seulement */
        x += FONT_PITCH;
        s++;
    }
}
