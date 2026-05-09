/*
 * Phase 1 — Squelette OSDK-free : HIRES init + triangle statique (vaisseau).
 * Cible : Oric-1 48K, compilé avec cc65 --target none + ld65 oric1.cfg.
 *
 * Encodage HIRES Oric (confirmé Phosphoric video.c) :
 *   (byte & 0x60)==0  → attribut  (modifie INK/PAPER, pas de pixels)
 *   (byte & 0x60)!=0  → pixel      bit7=1=inverse, bits5-0=6 pixels
 *   Init "vierge"     = 0x40       (bit6=1 → pixel, aucun bit allumé)
 *   Déclencher HIRES  = écrire 0x1C à $BB80 (attribut mode vidéo=4)
 */

#define HIRES_BASE  0xA000U
#define HIRES_W     240
#define HIRES_H     200
#define HIRES_COLS   40

#define BB80        0xBB80U     /* 1er octet de la zone texte, ligne 0 */

#define POKE(a,v)   (*((volatile unsigned char *)(a)) = (unsigned char)(v))
#define PEEK(a)     (*((volatile unsigned char *)(a)))

/* ------------------------------------------------------------------ */
static void hires_init(void)
{
    unsigned int i;

    /* Déclencher le mode HIRES depuis le mode texte : attribut 0x1C
     * → vid_mode = 4 (bit HIRES), persiste entre frames. */
    POKE(BB80, 0x1C);

    /* Remplir l'écran HIRES avec 0x40 : octet pixel vierge, noir.
     * 0x40 = bit6=1 → non-attribut, bits5-0=0 → aucun pixel allumé. */
    for (i = 0; i < (unsigned int)(HIRES_COLS * HIRES_H); i++) {
        POKE(HIRES_BASE + i, 0x40);
    }
}

/* ------------------------------------------------------------------ */
static void plot_pixel(int x, int y)
{
    unsigned int addr;
    unsigned char col, mask, byte;

    if ((unsigned int)x >= (unsigned int)HIRES_W) return;
    if ((unsigned int)y >= (unsigned int)HIRES_H) return;

    col  = (unsigned char)((unsigned int)x / 6U);
    mask = (unsigned char)(0x20U >> ((unsigned int)x % 6U));
    addr = HIRES_BASE + (unsigned int)y * (unsigned int)HIRES_COLS + col;

    byte = PEEK(addr);
    /* Allumer le pixel, forcer bit6=1 (prévient détection attribut),
     * forcer bit7=0 (mode normal, pas d'inverse vidéo). */
    POKE(addr, (byte | mask | 0x40U) & 0x7FU);
}

/* ------------------------------------------------------------------ */
static void draw_line(int x0, int y0, int x1, int y1)
{
    int dx, dy, sx, sy, adx, ady, err, e2;

    dx  = x1 - x0;  dy  = y1 - y0;
    adx = dx < 0 ? -dx : dx;
    ady = dy < 0 ? -dy : dy;
    sx  = dx < 0 ? -1 : 1;
    sy  = dy < 0 ? -1 : 1;
    err = adx - ady;

    for (;;) {
        plot_pixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 > -ady) { err -= ady; x0 += sx; }
        if (e2 <  adx) { err += adx; y0 += sy; }
    }
}

/* ------------------------------------------------------------------ */
void main(void)
{
    /* Sommets du vaisseau triangulaire (centré, style arcade Asteroids) */
    int tx = 120, ty = 28;   /* apex haut      */
    int lx =  72, ly = 168;  /* bas gauche     */
    int rx = 168, ry = 168;  /* bas droite     */

    hires_init();

    draw_line(tx, ty, lx, ly);   /* bord gauche    */
    draw_line(tx, ty, rx, ry);   /* bord droit     */
    draw_line(lx, ly, rx, ry);   /* base           */

    /* Boucle d'attente — Phosphoric --cycles N coupe automatiquement
     * pour les tests CI ; en interactif, fermer la fenêtre. */
    for (;;) {}
}
