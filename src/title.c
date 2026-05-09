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
    0xFF
};

static const unsigned char letter_S[] = {
    8, 1, 0, 1,         /* haut */
    0, 1, 0, 5,         /* descente gauche haut */
    0, 5, 8, 5,         /* milieu */
    8, 5, 8, 9,         /* descente droite bas */
    8, 9, 0, 9,         /* bas */
    0xFF
};

static const unsigned char letter_T[] = {
    0, 0, 8, 0,         /* barre haute */
    4, 0, 4, 9,         /* verticale centre */
    0xFF
};

static const unsigned char letter_E[] = {
    0, 0, 0, 9,         /* verticale gauche */
    0, 0, 8, 0,         /* haut */
    0, 5, 6, 5,         /* milieu */
    0, 9, 8, 9,         /* bas */
    0xFF
};

static const unsigned char letter_R[] = {
    0, 0, 0, 9,         /* verticale gauche */
    0, 0, 6, 0,         /* haut */
    6, 0, 8, 2,         /* coin haut-droit */
    8, 2, 8, 4,         /* descente courte */
    8, 4, 0, 5,         /* diagonale interne */
    0, 5, 8, 9,         /* jambe droite */
    0xFF
};

static const unsigned char letter_O[] = {
    0, 0, 8, 0,         /* haut */
    8, 0, 8, 9,         /* droite */
    8, 9, 0, 9,         /* bas */
    0, 9, 0, 0,         /* gauche */
    0xFF
};

static const unsigned char letter_I[] = {
    4, 0, 4, 9,         /* verticale */
    0, 0, 8, 0,         /* serif haut */
    0, 9, 8, 9,         /* serif bas */
    0xFF
};

static const unsigned char letter_D[] = {
    0, 0, 0, 9,         /* gauche */
    0, 0, 6, 0,         /* haut */
    6, 0, 8, 3,         /* coin haut-droit */
    8, 3, 8, 6,         /* droite */
    8, 6, 6, 9,         /* coin bas-droit */
    6, 9, 0, 9,         /* bas */
    0xFF
};

/* Phase 9d — lettres pour "GAME OVER" */

static const unsigned char letter_G[] = {
    8, 1, 0, 1,         /* haut */
    0, 1, 0, 9,         /* gauche */
    0, 9, 8, 9,         /* bas */
    8, 9, 8, 5,         /* droite bas */
    8, 5, 4, 5,         /* barre milieu */
    0xFF
};

static const unsigned char letter_M[] = {
    0, 9, 0, 0,         /* gauche */
    0, 0, 4, 5,         /* descente vers centre */
    4, 5, 8, 0,         /* montée vers droite */
    8, 0, 8, 9,         /* droite */
    0xFF
};

static const unsigned char letter_V[] = {
    0, 0, 4, 9,         /* gauche descendante */
    4, 9, 8, 0,         /* droite montante */
    0xFF
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
