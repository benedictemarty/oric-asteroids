/*
 * Phase 2 — Bresenham XOR en asm 6502.
 * Tests couverts : idempotence XOR, h/v/diagonales/obliques.
 * Référence visuelle pour make check : triangle + croix + bords + obliques.
 */

/* Routines asm (line.s) */
void hires_init(void);
void draw_line_xor(void);

/* Paramètres de ligne en ZP (exportés depuis line.s).
 * #pragma zpsym force cc65 à utiliser l'adressage page zéro. */
extern unsigned char lx0, ly0, lx1, ly1;
#pragma zpsym ("lx0")
#pragma zpsym ("ly0")
#pragma zpsym ("lx1")
#pragma zpsym ("ly1")

/* Wrapper C → ZP → asm */
static void line(unsigned char x0, unsigned char y0,
                 unsigned char x1, unsigned char y1)
{
    lx0 = x0; ly0 = y0; lx1 = x1; ly1 = y1;
    draw_line_xor();
}

/* ------------------------------------------------------------------ */
void main(void)
{
    hires_init();

    /* --- Test idempotence XOR : 3 passes sur le triangle ---
     *  Pass 1 : dessine
     *  Pass 2 : efface (XOR = retour à l'état vierge)
     *  Pass 3 : redessine (visible sur la capture de référence)
     */
#define TRI_TX 120
#define TRI_TY  28
#define TRI_LX  72
#define TRI_LY 168
#define TRI_RX 168
#define TRI_RY 168

    line(TRI_TX, TRI_TY, TRI_LX, TRI_LY);
    line(TRI_TX, TRI_TY, TRI_RX, TRI_RY);
    line(TRI_LX, TRI_LY, TRI_RX, TRI_RY);

    line(TRI_TX, TRI_TY, TRI_LX, TRI_LY);   /* efface */
    line(TRI_TX, TRI_TY, TRI_RX, TRI_RY);
    line(TRI_LX, TRI_LY, TRI_RX, TRI_RY);

    line(TRI_TX, TRI_TY, TRI_LX, TRI_LY);   /* redessine */
    line(TRI_TX, TRI_TY, TRI_RX, TRI_RY);
    line(TRI_LX, TRI_LY, TRI_RX, TRI_RY);

    /* --- Lignes géométriques de test (10 cas) --- */
    line(  0,   0, 239,   0);   /* horizontale haute         */
    line(  0, 199, 239, 199);   /* horizontale basse         */
    line(  0,   0,   0, 199);   /* verticale gauche          */
    line(239,   0, 239, 199);   /* verticale droite          */
    line(  0,   0, 239, 199);   /* diagonale ↘               */
    line(239,   0,   0, 199);   /* diagonale ↙               */
    line(  0, 100, 239, 100);   /* horizontale centrale      */
    line(120,   0, 120, 199);   /* verticale centrale        */
    line( 60,  50, 180, 150);   /* oblique pente ~1.1        */
    line( 60, 150, 180,  50);   /* oblique pente ~-1.1       */

    /* --- Boucle de benchmark : 1000 tracés de la même ligne diagonale ---
     * Ligne (30,30)→(180,150) : dx=150, dy=120 → 151 pixels.
     * 1000 × 151 px = 151 000 pixels. Le profiler mesure le coût réel.
     * XOR = alternance draw/erase → l'écran oscille mais c'est voulu ici. */
    {
        unsigned int n;
        for (n = 1000; n != 0; n--) {
            line(30, 30, 180, 150);
        }
    }

    for (;;) {}
}
