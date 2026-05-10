/*
 * title.c — Écran titre "ASTEROIDS" Phase 9c
 *
 * Lettres dessinées en segments XOR via _draw_line_xor.
 * Format compact : liste de segments delta hardcodés par lettre.
 * Hauteur lettre = 10 px, largeur 8 px, espace inter-lettre = 12 px.
 *
 * "ASTEROIDS" = 9 lettres × 4-5 segments = ~40 segments XOR.
 */

extern unsigned char lx0, ly0, lx1, ly1;
#pragma zpsym ("lx0")
#pragma zpsym ("ly0")
#pragma zpsym ("lx1")
#pragma zpsym ("ly1")

void draw_line_xor(void);

/* Format : liste de couples (x0, y0, x1, y1) terminée par 0xFF.
 * Coords relatives au coin haut-gauche de la lettre (largeur 8, hauteur 10). */

static const unsigned char letter_A[] = {
    0, 9, 4, 0,         /* gauche montante */
    4, 0, 8, 9,         /* droite descendante */
    2, 5, 6, 5,         /* barre milieu */
    0xFF,
    4, 0,               /* plot pointe (partagée) */
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

static const unsigned char letter_E[] = {
    0, 0, 0, 9,
    0, 0, 8, 0,
    0, 5, 6, 5,
    0, 9, 8, 9,
    0xFF,
    0, 0,  0, 5,  0, 9,
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

static const unsigned char letter_O[] = {
    0, 0, 8, 0,
    8, 0, 8, 9,
    8, 9, 0, 9,
    0, 9, 0, 0,
    0xFF,
    0, 0,  8, 0,  8, 9,  0, 9,
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

static const unsigned char letter_D[] = {
    0, 0, 0, 9,
    0, 0, 6, 0,
    6, 0, 8, 3,
    8, 3, 8, 6,
    8, 6, 6, 9,
    6, 9, 0, 9,
    0xFF,
    0, 0,  6, 0,  8, 3,  8, 6,  6, 9,  0, 9,
    0xFE
};

/* Phase 9d — lettres pour "GAME OVER" */

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

static const unsigned char letter_M[] = {
    0, 9, 0, 0,
    0, 0, 4, 5,
    4, 5, 8, 0,
    8, 0, 8, 9,
    0xFF,
    0, 0,  4, 5,  8, 0,
    0xFE
};

static const unsigned char letter_V[] = {
    0, 0, 4, 9,
    4, 9, 8, 0,
    0xFF,
    4, 9,
    0xFE
};

/* Phase 10d — lettre W (V doublé) */
static const unsigned char letter_W[] = {
    0, 0, 2, 9,         /* gauche descendante */
    2, 9, 4, 4,         /* montée vers centre */
    4, 4, 6, 9,         /* descente droite-centre */
    6, 9, 8, 0,         /* montée droite */
    0xFF,
    2, 9,  4, 4,  6, 9,
    0xFE
};

/* Phase 15 — lettre H (2 verticales + barre milieu) */
static const unsigned char letter_H[] = {
    0, 0, 0, 9,         /* verticale gauche */
    8, 0, 8, 9,         /* verticale droite */
    0, 4, 8, 4,         /* barre milieu */
    0xFF,
    0, 4,  8, 4,        /* sommets partagés barre/verticales */
    0xFE
};

/* Phase 9e — lettres pour "PRESS SPACE" */

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

static const unsigned char letter_C[] = {
    8, 1, 0, 1,
    0, 1, 0, 9,
    0, 9, 8, 9,
    0xFF,
    0, 1,  0, 9,
    0xFE
};

/* Format compact :
 *   liste de 4-tuples (x0,y0,x1,y1) — segments —
 *   0xFF marqueur fin segments
 *   liste de 2-tuples (x,y) — plots de sommets partagés —
 *   0xFE marqueur fin total (peut suivre directement 0xFF si pas de plots)
 *
 * Le replot des sommets partagés contre-balance le double-XOR (un sommet
 * touché par 2 segments est XOR 2× → effacé ; le replot le re-XOR → tracé). */
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

/* Dessine "ASTEROIDS" centré horizontalement à y donné.
 * Largeur totale = 9 * 12 - 4 = 104 pixels → x = (240 - 104) / 2 = 68. */
void title_draw(void)
{
    unsigned char x = 68;
    unsigned char y = 80;
    draw_letter(letter_A, x +   0, y);
    draw_letter(letter_S, x +  12, y);
    draw_letter(letter_T, x +  24, y);
    draw_letter(letter_E, x +  36, y);
    draw_letter(letter_R, x +  48, y);
    draw_letter(letter_O, x +  60, y);
    draw_letter(letter_I, x +  72, y);
    draw_letter(letter_D, x +  84, y);
    draw_letter(letter_S, x +  96, y);
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
    unsigned char x = 66;
    unsigned char y = 70;
    draw_letter(letter_G, x +   0, y);
    draw_letter(letter_A, x +  12, y);
    draw_letter(letter_M, x +  24, y);
    draw_letter(letter_E, x +  36, y);
    /* (espace en x+48) */
    draw_letter(letter_O, x +  60, y);
    draw_letter(letter_V, x +  72, y);
    draw_letter(letter_E, x +  84, y);
    draw_letter(letter_R, x +  96, y);
}

void gameover_erase(void)
{
    gameover_draw();
}

/* Dessine "PRESS SPACE" centré en y=70 ou param py.
 * 11 caractères × 12 = 132 → x = (240 - 132) / 2 = 54. */
void presspace_draw(unsigned char py)
{
    unsigned char x = 54;
    draw_letter(letter_P, x +   0, py);
    draw_letter(letter_R, x +  12, py);
    draw_letter(letter_E, x +  24, py);
    draw_letter(letter_S, x +  36, py);
    draw_letter(letter_S, x +  48, py);
    /* (espace) */
    draw_letter(letter_S, x +  72, py);
    draw_letter(letter_P, x +  84, py);
    draw_letter(letter_A, x +  96, py);
    draw_letter(letter_C, x + 108, py);
    draw_letter(letter_E, x + 120, py);
}

void presspace_erase(unsigned char py)
{
    presspace_draw(py);
}

/* Phase 10d/10j — affichage "WAVE nn" en haut-centre.
 * 5 caractères ("WAVE ") + 1 ou 2 chiffres.
 * Phase 10j : si wave > 9, afficher 2 chiffres (10, 11). */
void wave_label_draw(unsigned char py, unsigned char digit)
{
    extern void hud_xor_digit(unsigned char d, unsigned char px, unsigned char py);
    unsigned char x = 80;
    draw_letter(letter_W, x +   0, py);
    draw_letter(letter_A, x +  12, py);
    draw_letter(letter_V, x +  24, py);
    draw_letter(letter_E, x +  36, py);
    /* (espace en x+48) */
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
    unsigned char x = 54;
    draw_letter(letter_H, x +   0, py);
    draw_letter(letter_I, x +  12, py);
    draw_letter(letter_G, x +  24, py);
    draw_letter(letter_H, x +  36, py);
    /* (espace en x+48) */
    draw_letter(letter_S, x +  60, py);
    draw_letter(letter_C, x +  72, py);
    draw_letter(letter_O, x +  84, py);
    draw_letter(letter_R, x +  96, py);
    draw_letter(letter_E, x + 108, py);
    draw_letter(letter_S, x + 120, py);
}

void hiscores_label_erase(unsigned char py)
{
    hiscores_label_draw(py);
}
